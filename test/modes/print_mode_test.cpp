#include <modes/print_mode.h>

#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <ai/providers/faux.h>
#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::modes;
namespace ai = pi::ai;
using pi::ai::providers::FauxOptions;
using pi::ai::providers::FauxProvider;
using pi::ai::providers::FauxResponse;
using pi::ai::providers::FauxText;
using pi::ai::providers::FauxThinking;
using pi::util::CancelToken;

namespace {
struct PrintRun {
    int         code = -1;
    str         out;
    str         err;
};

PrintRun print(FauxProvider& provider, const PrintOptions& options = {}, CancelToken cancel = {}) {
    s::ostringstream out, err;
    PrintRun r;
    ai::Model model = provider.models().empty() ? ai::Model{} : provider.models()[0];
    r.code = run_print_mode(provider, model, make_print_context("q"), {}, options, out, err, cancel);
    r.out = out.str();
    r.err = err.str();
    return r;
}

FauxResponse with_usage(FauxResponse r) {
    r.usage = ai::Usage{.input = 28, .output = 33, .cache_read = 0, .cache_write = 0, .total_tokens = 61, .cost = {}};
    return r;
}
}  // namespace

// ===========================================================================
// Helpers
// ===========================================================================

TEST(MakePrintContext, OneUserMessage) {
    ai::Context c = make_print_context("hello", "be brief");
    EXPECT_EQ(c.system_prompt, "be brief");
    ASSERT_EQ(c.messages.size(), 1u);
    ASSERT_TRUE(s::holds_alternative<ai::UserMessage>(c.messages[0]));
    const ai::UserMessage& u = s::get<ai::UserMessage>(c.messages[0]);
    EXPECT_EQ(u.content, (vec<ai::UserContent>{ai::TextContent{"hello"}}));
    EXPECT_GT(u.timestamp, 1'700'000'000'000);
    EXPECT_TRUE(c.tools.empty());
}

TEST(MakePrintContext, NoSystemPromptByDefault) {
    EXPECT_EQ(make_print_context("x").system_prompt, "");
}

TEST(FormatUsage, Exact) {
    ai::Usage u{.input = 18, .output = 33, .cache_read = 10, .cache_write = 0, .total_tokens = 61, .cost = {}};
    EXPECT_EQ(format_usage(u), "tokens: 18 in, 33 out, 10 cached, 61 total");
}

// ===========================================================================
// run_print_mode
// ===========================================================================

TEST(PrintMode, PrintsAnswerWithTrailingNewline) {
    FauxProvider p(FauxOptions{.chunk_size = 3});
    p.enqueue(with_usage(FauxResponse::text("Hello world")));
    PrintRun r = print(p);
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "Hello world\n");
    EXPECT_NE(r.err.find("tokens: 28 in, 33 out, 0 cached, 61 total"), str::npos) << r.err;
}

TEST(PrintMode, NoExtraNewlineWhenAnswerEndsWithOne) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("line\n"));
    EXPECT_EQ(print(p).out, "line\n");
}

TEST(PrintMode, EmptyAnswerPrintsNothing) {
    FauxProvider p;
    p.enqueue(FauxResponse{});
    PrintRun r = print(p);
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "");
}

TEST(PrintMode, ThinkingHiddenByDefault) {
    FauxProvider p;
    p.enqueue(FauxResponse{.steps = {FauxThinking{"secret plan"}, FauxText{"4"}}, .stop_reason = {}, .usage = {}, .error = {}});
    PrintRun r = print(p);
    EXPECT_EQ(r.out, "4\n");
    EXPECT_EQ(r.err.find("secret plan"), str::npos) << r.err;
}

TEST(PrintMode, ShowThinkingWritesItToStderr) {
    FauxProvider p;
    p.enqueue(FauxResponse{.steps = {FauxThinking{"secret plan"}, FauxText{"4"}}, .stop_reason = {}, .usage = {}, .error = {}});
    PrintRun r = print(p, PrintOptions{.show_thinking = true, .show_usage = false});
    EXPECT_EQ(r.out, "4\n") << "stdout carries only the answer";
    EXPECT_NE(r.err.find("secret plan"), str::npos) << r.err;
}

TEST(PrintMode, UsageCanBeDisabled) {
    FauxProvider p;
    p.enqueue(with_usage(FauxResponse::text("x")));
    PrintRun r = print(p, PrintOptions{.show_thinking = false, .show_usage = false});
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "x\n");
    EXPECT_EQ(r.err, "");
}

TEST(PrintMode, ErrorExitsOne) {
    FauxProvider p;
    FauxResponse resp = FauxResponse::text("par");
    resp.error = "boom";
    p.enqueue(resp);
    PrintRun r = print(p);
    EXPECT_EQ(r.code, 1);
    EXPECT_NE(r.err.find("error: boom"), str::npos) << r.err;
}

TEST(PrintMode, ProviderErrorBeforeAnyTextExitsOne) {
    FauxProvider p;  // nothing queued
    PrintRun r = print(p);
    EXPECT_EQ(r.code, 1);
    EXPECT_EQ(r.out, "");
    EXPECT_NE(r.err.find("no faux response queued"), str::npos) << r.err;
}

TEST(PrintMode, AbortedExits130) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("never"));
    CancelToken cancel;
    cancel.cancel();
    PrintRun r = print(p, {}, cancel);
    EXPECT_EQ(r.code, 130);
    EXPECT_NE(r.err.find("aborted"), str::npos) << r.err;
    EXPECT_EQ(r.out, "");
}

TEST(PrintMode, SendsTheContextToTheProvider) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("x"));
    ai::Context c = make_print_context("what is RAII?", "sys");
    s::ostringstream out, err;
    run_print_mode(p, ai::Model{}, c, {}, {}, out, err, CancelToken{});
    ASSERT_EQ(p.calls().size(), 1u);
    EXPECT_EQ(p.calls()[0], c);
}
