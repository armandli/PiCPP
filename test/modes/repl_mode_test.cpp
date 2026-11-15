#include <modes/repl_mode.h>

#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <ai/providers/faux.h>
#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::modes;
namespace ai = pi::ai;
using pi::ai::providers::FauxProvider;
using pi::ai::providers::FauxResponse;
using pi::ai::providers::FauxText;
using pi::ai::providers::FauxThinking;
using pi::util::CancelToken;

namespace {
struct ReplRun {
    int code = -1;
    str out;
    str err;
};

ReplRun repl(
    FauxProvider& provider,
    strv input,
    const ReplOptions& options = {},
    strv system_prompt = "",
    CancelToken cancel = {}
){
    s::istringstream in{str(input)};
    s::ostringstream out, err;
    ReplRun r;
    ai::Model model = provider.models()[0];
    r.code = run_repl_mode(provider, model, system_prompt, {}, options, in, out, err, cancel);
    r.out = out.str();
    r.err = err.str();
    return r;
}

// Text of a user / assistant message in a recorded Context.
str user_text(const ai::Message& m) {
    const ai::UserMessage* u = s::get_if<ai::UserMessage>(&m);
    if (u == nullptr or u->content.empty()) return "<not a user text message>";
    const ai::TextContent* t = s::get_if<ai::TextContent>(&u->content[0]);
    return t ? t->text : "<not text>";
}

str assistant_text(const ai::Message& m) {
    const ai::AssistantMessage* a = s::get_if<ai::AssistantMessage>(&m);
    return a ? ai::text_of(*a) : "<not an assistant message>";
}

FauxResponse failing(str error) {
    FauxResponse r;
    r.error = s::move(error);
    return r;
}

ReplOptions quiet() {
    return ReplOptions{.show_thinking = false, .show_usage = false, .prompt = "> "};
}
}  // namespace

// ===========================================================================
// parse_repl_line
// ===========================================================================

TEST(ParseReplLine, Table) {
    struct Case { strv line; ReplLine expected; };
    const Case cases[] = {
        {"",              {ReplLineKind::Empty, ""}},
        {"   \t",         {ReplLineKind::Empty, ""}},
        {"\r",            {ReplLineKind::Empty, ""}},
        {"/quit",         {ReplLineKind::Quit, ""}},
        {"/exit",         {ReplLineKind::Quit, ""}},
        {"  /quit  ",     {ReplLineKind::Quit, ""}},
        {"/clear",        {ReplLineKind::Clear, ""}},
        {"/models",       {ReplLineKind::Models, ""}},
        {"/models all",   {ReplLineKind::Models, ""}},
        {"/foo",          {ReplLineKind::Unknown, "/foo"}},
        {"/foo bar baz",  {ReplLineKind::Unknown, "/foo"}},
        {"hello",         {ReplLineKind::Prompt, "hello"}},
        {"  hi there \r", {ReplLineKind::Prompt, "hi there"}},
        {"what is a/b?",  {ReplLineKind::Prompt, "what is a/b?"}},
    };
    for (const Case& c : cases)
        EXPECT_EQ(parse_repl_line(c.line), c.expected) << "line: \"" << c.line << '"';
}

// ===========================================================================
// format_model_list
// ===========================================================================

TEST(FormatModelList, MarksTheCurrentModel) {
    vec<ai::Model> models(3);
    models[0].id = "a";
    models[1].id = "b";
    models[2].id = "c";
    EXPECT_EQ(format_model_list(models, "b"), "  a\n* b\n  c\n");
}

TEST(FormatModelList, CurrentNotListed) {
    vec<ai::Model> models(1);
    models[0].id = "a";
    EXPECT_EQ(format_model_list(models, "zzz"), "  a\n");
}

TEST(FormatModelList, Empty) {
    EXPECT_EQ(format_model_list({}, "a"), "no models available\n");
}

// ===========================================================================
// run_repl_mode: input handling
// ===========================================================================

TEST(Repl, EofEndsWithZero) {
    FauxProvider p;
    ReplRun r = repl(p, "", quiet());
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "> \n") << "prompt, then a newline at EOF";
    EXPECT_TRUE(p.calls().empty());
}

TEST(Repl, QuitStopsReading) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("never"));
    ReplRun r = repl(p, "/quit\nhello\n", quiet());
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "> ");
    EXPECT_TRUE(p.calls().empty());
}

TEST(Repl, EmptyLinesAreIgnored) {
    FauxProvider p;
    ReplRun r = repl(p, "\n   \n", quiet());
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "> > > \n");
    EXPECT_TRUE(p.calls().empty());
}

TEST(Repl, UnknownCommandIsReported) {
    FauxProvider p;
    ReplRun r = repl(p, "/frobnicate now\n", quiet());
    EXPECT_EQ(r.code, 0);
    EXPECT_NE(r.err.find("unknown command: /frobnicate"), str::npos) << r.err;
    EXPECT_TRUE(p.calls().empty()) << "commands never reach the model";
}

TEST(Repl, CustomPrompt) {
    FauxProvider p;
    ReplOptions options = quiet();
    options.prompt = "pi> ";
    EXPECT_EQ(repl(p, "", options).out, "pi> \n");
}

// ===========================================================================
// run_repl_mode: conversation
// ===========================================================================

TEST(Repl, OneTurn) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("Hello!"));
    ReplRun r = repl(p, "hi\n", quiet());
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "> Hello!\n> \n");
    ASSERT_EQ(p.calls().size(), 1u);
    ASSERT_EQ(p.calls()[0].messages.size(), 1u);
    EXPECT_EQ(user_text(p.calls()[0].messages[0]), "hi");
}

TEST(Repl, FollowUpSeesTheHistory) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("Paris."));
    p.enqueue(FauxResponse::text("About 2.1 million."));
    repl(p, "Capital of France?\nHow many people live there?\n", quiet());
    ASSERT_EQ(p.calls().size(), 2u);
    const vec<ai::Message>& m = p.calls()[1].messages;
    ASSERT_EQ(m.size(), 3u);
    EXPECT_EQ(user_text(m[0]), "Capital of France?");
    EXPECT_EQ(assistant_text(m[1]), "Paris.");
    EXPECT_EQ(user_text(m[2]), "How many people live there?");
}

TEST(Repl, SystemPromptIsSentEveryTurn) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("a"));
    p.enqueue(FauxResponse::text("b"));
    repl(p, "one\ntwo\n", quiet(), "be brief");
    ASSERT_EQ(p.calls().size(), 2u);
    EXPECT_EQ(p.calls()[0].system_prompt, "be brief");
    EXPECT_EQ(p.calls()[1].system_prompt, "be brief");
    EXPECT_TRUE(p.calls()[1].tools.empty());
}

TEST(Repl, ClearForgetsTheHistory) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("a"));
    p.enqueue(FauxResponse::text("b"));
    ReplRun r = repl(p, "one\n/clear\ntwo\n", quiet(), "sys");
    ASSERT_EQ(p.calls().size(), 2u);
    ASSERT_EQ(p.calls()[1].messages.size(), 1u);
    EXPECT_EQ(user_text(p.calls()[1].messages[0]), "two");
    EXPECT_EQ(p.calls()[1].system_prompt, "sys") << "/clear keeps the system prompt";
    EXPECT_NE(r.err.find("context cleared"), str::npos) << r.err;
}

TEST(Repl, ModelsListsTheProvidersModels) {
    FauxProvider p;  // one model, id "faux", which is also the current one
    ReplRun r = repl(p, "/models\n", quiet());
    EXPECT_EQ(r.code, 0);
    EXPECT_NE(r.out.find("* faux\n"), str::npos) << r.out;
    EXPECT_TRUE(p.calls().empty());
}

// ===========================================================================
// run_repl_mode: failures
// ===========================================================================

TEST(Repl, ErrorIsReportedAndTheReplContinues) {
    FauxProvider p;
    p.enqueue(failing("boom"));
    p.enqueue(FauxResponse::text("ok"));
    ReplRun r = repl(p, "first\nsecond\n", quiet());
    EXPECT_EQ(r.code, 0);
    EXPECT_NE(r.err.find("error: boom"), str::npos) << r.err;
    EXPECT_NE(r.out.find("ok\n"), str::npos) << r.out;
}

TEST(Repl, FailedTurnIsDroppedFromTheHistory) {
    FauxProvider p;
    p.enqueue(failing("boom"));
    p.enqueue(FauxResponse::text("ok"));
    repl(p, "first\nsecond\n", quiet());
    ASSERT_EQ(p.calls().size(), 2u);
    ASSERT_EQ(p.calls()[1].messages.size(), 1u) << "no unanswered 'first' left behind";
    EXPECT_EQ(user_text(p.calls()[1].messages[0]), "second");
}

TEST(Repl, AbortEndsTheSessionWith130) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("never"));
    p.enqueue(FauxResponse::text("never either"));
    CancelToken cancel;
    cancel.cancel();
    ReplRun r = repl(p, "hi\nagain\n", quiet(), "", cancel);
    EXPECT_EQ(r.code, 130);
    EXPECT_EQ(p.calls().size(), 1u) << "no turn after the abort";
    EXPECT_NE(r.err.find("aborted"), str::npos) << r.err;
}

// ===========================================================================
// run_repl_mode: output streams
// ===========================================================================

TEST(Repl, UsageLineAfterEachAnswer) {
    FauxProvider p;
    FauxResponse a = FauxResponse::text("a");
    a.usage = ai::Usage{.input = 1, .output = 2, .cache_read = 0, .cache_write = 0, .total_tokens = 3, .cost = {}};
    p.enqueue(a);
    p.enqueue(a);
    ReplOptions options = quiet();
    options.show_usage = true;
    ReplRun r = repl(p, "x\ny\n", options);
    const str line = "tokens: 1 in, 2 out, 0 cached, 3 total";
    s::size_t first = r.err.find(line);
    ASSERT_NE(first, str::npos) << r.err;
    EXPECT_NE(r.err.find(line, first + 1), str::npos) << "one usage line per turn: " << r.err;
}

TEST(Repl, ThinkingStaysOffStdout) {
    FauxProvider p;
    p.enqueue(FauxResponse{.steps = {FauxThinking{"secret plan"}, FauxText{"4"}}, .stop_reason = {}, .usage = {}, .error = {}});
    ReplOptions options = quiet();
    options.show_thinking = true;
    ReplRun r = repl(p, "2+2?\n", options);
    EXPECT_EQ(r.out.find("secret plan"), str::npos) << r.out;
    EXPECT_NE(r.err.find("secret plan"), str::npos) << r.err;
}

TEST(Repl, AnswerTextCarriesIntoTheFollowUp) {
    // The assistant message keeps its thinking block (a provider decides what to
    // replay), but its text is what the follow-up sees.
    FauxProvider p;
    p.enqueue(FauxResponse{.steps = {FauxThinking{"plan"}, FauxText{"4"}}, .stop_reason = {}, .usage = {}, .error = {}});
    p.enqueue(FauxResponse::text("8"));
    repl(p, "2+2?\ndouble it\n", quiet());
    ASSERT_EQ(p.calls().size(), 2u);
    ASSERT_EQ(p.calls()[1].messages.size(), 3u);
    EXPECT_EQ(assistant_text(p.calls()[1].messages[1]), "4");
}
