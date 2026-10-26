#include <modes/cli_args.h>

#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#include <util/aliases.h>

using namespace pi::modes;

// ===========================================================================
// parse_model_ref
// ===========================================================================

TEST(ParseModelRef, ProviderAndId) {
    auto r = parse_model_ref("ollama/qwen3.8:27b-mlx");
    ASSERT_TRUE(r.has_value()) << r.error();
    EXPECT_EQ(*r, (ModelRef{"ollama", "qwen3.8:27b-mlx"}));
}

TEST(ParseModelRef, SplitsAtFirstSlashOnly) {
    auto r = parse_model_ref("ollama/hf.co/org/model:q4");
    ASSERT_TRUE(r.has_value()) << r.error();
    EXPECT_EQ(*r, (ModelRef{"ollama", "hf.co/org/model:q4"}));
}

TEST(ParseModelRef, BareIdUsesDefaultProvider) {
    auto r = parse_model_ref("gemma4:31b-mlx");
    ASSERT_TRUE(r.has_value()) << r.error();
    EXPECT_EQ(*r, (ModelRef{"ollama", "gemma4:31b-mlx"}));
    auto f = parse_model_ref("x", "faux");
    ASSERT_TRUE(f.has_value()) << f.error();
    EXPECT_EQ(f->provider, "faux");
}

TEST(ParseModelRef, Errors) {
    EXPECT_FALSE(parse_model_ref("").has_value());
    EXPECT_FALSE(parse_model_ref("/qwen").has_value());
    EXPECT_FALSE(parse_model_ref("ollama/").has_value());
}

// ===========================================================================
// parse_cli_args
// ===========================================================================

// Clears the env vars the parser reads, so the host environment can't leak in.
class CliArgsTest : public ::testing::Test {
protected:
    void SetUp() override {
        unsetenv("PI_MODEL");
        unsetenv("PI_OLLAMA_URL");
    }
    void TearDown() override { SetUp(); }
};

TEST_F(CliArgsTest, NoArgumentsIsValid) {
    auto r = parse_cli_args(vec<str>{});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(*r, CliArgs{});
}

TEST_F(CliArgsTest, PrintWithModel) {
    auto r = parse_cli_args(vec<str>{"-p", "hello there", "--model", "ollama/qwen3.8:27b-mlx"});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->print_prompt, "hello there");
    EXPECT_EQ(r->model, (ModelRef{"ollama", "qwen3.8:27b-mlx"}));
}

TEST_F(CliArgsTest, ArgumentOrderDoesNotMatter) {
    // Also catches feeding CLI11's vector overload un-reversed.
    auto r = parse_cli_args(vec<str>{"--model", "faux/x", "--print", "q"});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->print_prompt, "q");
    EXPECT_EQ(r->model, (ModelRef{"faux", "x"}));
}

TEST_F(CliArgsTest, PromptWithSpacesAndQuotesIsOneArgument) {
    auto r = parse_cli_args(vec<str>{"-p", R"(say "hi" -- now)", "--model", "faux/x"});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->print_prompt, R"(say "hi" -- now)");
}

TEST_F(CliArgsTest, BareModelIdMeansOllama) {
    auto r = parse_cli_args(vec<str>{"-p", "q", "--model", "qwen3.8:27b-mlx"});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->model, (ModelRef{"ollama", "qwen3.8:27b-mlx"}));
}

TEST_F(CliArgsTest, PrintRequiresModel) {
    auto r = parse_cli_args(vec<str>{"-p", "q"});
    ASSERT_FALSE(r.has_value());
    EXPECT_TRUE(r.error().is_error());
    EXPECT_NE(r.error().output.find("--model"), str::npos) << r.error().output;
}

TEST_F(CliArgsTest, ModelFromEnvironment) {
    setenv("PI_MODEL", "faux/from-env", 1);
    auto r = parse_cli_args(vec<str>{"-p", "q"});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->model, (ModelRef{"faux", "from-env"}));
}

TEST_F(CliArgsTest, FlagBeatsEnvironment) {
    setenv("PI_MODEL", "faux/from-env", 1);
    auto r = parse_cli_args(vec<str>{"-p", "q", "--model", "ollama/flag"});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->model, (ModelRef{"ollama", "flag"}));
}

TEST_F(CliArgsTest, BadModelIsUsageError) {
    auto r = parse_cli_args(vec<str>{"-p", "q", "--model", "ollama/"});
    ASSERT_FALSE(r.has_value());
    EXPECT_TRUE(r.error().is_error());
}

TEST_F(CliArgsTest, OptionalFlags) {
    auto r = parse_cli_args(vec<str>{"-p", "q", "--model", "faux/x", "--system-prompt", "be brief",
                                     "--max-tokens", "64", "--show-thinking", "--ollama-url", "http://gpu:11434"});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->system_prompt, "be brief");
    EXPECT_EQ(r->max_tokens, 64);
    EXPECT_TRUE(r->show_thinking);
    EXPECT_EQ(r->ollama_url, "http://gpu:11434");
}

TEST_F(CliArgsTest, Defaults) {
    auto r = parse_cli_args(vec<str>{"-p", "q", "--model", "faux/x"});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->system_prompt, "");
    EXPECT_FALSE(r->max_tokens.has_value());
    EXPECT_FALSE(r->show_thinking);
    EXPECT_EQ(r->ollama_url, "http://localhost:11434");
}

TEST_F(CliArgsTest, OllamaUrlFromEnvironment) {
    setenv("PI_OLLAMA_URL", "http://env:1", 1);
    auto r = parse_cli_args(vec<str>{});
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->ollama_url, "http://env:1");
}

TEST_F(CliArgsTest, MaxTokensMustBePositive) {
    EXPECT_FALSE(parse_cli_args(vec<str>{"-p", "q", "--model", "faux/x", "--max-tokens", "0"}).has_value());
    EXPECT_FALSE(parse_cli_args(vec<str>{"-p", "q", "--model", "faux/x", "--max-tokens", "-5"}).has_value());
    EXPECT_FALSE(parse_cli_args(vec<str>{"-p", "q", "--model", "faux/x", "--max-tokens", "lots"}).has_value());
}

TEST_F(CliArgsTest, PrintNeedsAValue) {
    auto r = parse_cli_args(vec<str>{"--model", "faux/x", "-p"});
    ASSERT_FALSE(r.has_value());
    EXPECT_TRUE(r.error().is_error());
}

TEST_F(CliArgsTest, UnknownArgumentIsUsageError) {
    auto r = parse_cli_args(vec<str>{"--bogus"});
    ASSERT_FALSE(r.has_value());
    EXPECT_TRUE(r.error().is_error());
    EXPECT_NE(r.error().output.find("--bogus"), str::npos) << r.error().output;
}

TEST_F(CliArgsTest, HelpExitsZeroWithUsage) {
    for (const char* flag : {"-h", "--help"}) {
        auto r = parse_cli_args(vec<str>{flag});
        ASSERT_FALSE(r.has_value()) << flag;
        EXPECT_EQ(r.error().code, 0) << flag;
        EXPECT_FALSE(r.error().is_error());
        EXPECT_NE(r.error().output.find("--print"), str::npos) << r.error().output;
        EXPECT_NE(r.error().output.find("--model"), str::npos) << r.error().output;
    }
}

TEST_F(CliArgsTest, VersionExitsZero) {
    for (const char* flag : {"-v", "--version"}) {
        auto r = parse_cli_args(vec<str>{flag});
        ASSERT_FALSE(r.has_value()) << flag;
        EXPECT_EQ(r.error().code, 0) << flag;
        EXPECT_NE(r.error().output.find("pi (PiCPP) " PICPP_VERSION), str::npos) << r.error().output;
    }
}

TEST_F(CliArgsTest, ArgvOverloadSkipsProgramName) {
    const char* argv[] = {"pi", "-p", "q", "--model", "faux/x"};
    auto r = parse_cli_args(5, argv);
    ASSERT_TRUE(r.has_value()) << r.error().output;
    EXPECT_EQ(r->print_prompt, "q");
}
