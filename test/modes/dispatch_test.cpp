#include <modes/dispatch.h>

#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::modes;
using pi::util::CancelToken;

namespace {
CliArgs with_model(str provider, str id) {
    CliArgs a;
    a.model = ModelRef{s::move(provider), s::move(id)};
    return a;
}
}  // namespace

// ===========================================================================
// make_provider / resolve_model
// ===========================================================================

TEST(MakeProvider, Ollama) {
    auto p = make_provider(with_model("ollama", "qwen"));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->id(), "ollama");
}

TEST(MakeProvider, Faux) {
    auto p = make_provider(with_model("faux", "x"));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->id(), "faux");
}

TEST(MakeProvider, UnknownOrMissing) {
    EXPECT_EQ(make_provider(with_model("openai", "gpt")), nullptr);
    EXPECT_EQ(make_provider(CliArgs{}), nullptr);
}

TEST(ResolveModel, OllamaAppendsV1) {
    CliArgs a = with_model("ollama", "qwen3.8:27b-mlx");
    auto m = resolve_model(a);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->id, "qwen3.8:27b-mlx");
    EXPECT_EQ(m->provider, "ollama");
    EXPECT_EQ(m->api, "openai-completions");
    EXPECT_EQ(m->base_url, "http://localhost:11434/v1");
}

TEST(ResolveModel, OllamaUrlTrailingSlash) {
    CliArgs a = with_model("ollama", "m");
    a.ollama_url = "http://gpu:11434/";
    auto m = resolve_model(a);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->base_url, "http://gpu:11434/v1");
}

TEST(ResolveModel, Faux) {
    auto m = resolve_model(with_model("faux", "scripted"));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->id, "scripted");
    EXPECT_EQ(m->provider, "faux");
}

TEST(ResolveModel, UnknownOrMissing) {
    EXPECT_FALSE(resolve_model(with_model("openai", "gpt")).has_value());
    EXPECT_FALSE(resolve_model(CliArgs{}).has_value());
}

// ===========================================================================
// dispatch
// ===========================================================================

TEST(Dispatch, NoModeSelected) {
    s::ostringstream out, err;
    EXPECT_EQ(dispatch(CliArgs{}, out, err, CancelToken{}), 2);
    EXPECT_EQ(out.str(), "");
    EXPECT_NE(err.str().find("-p"), str::npos) << err.str();
}

TEST(Dispatch, UnknownProvider) {
    CliArgs a = with_model("openai", "gpt");
    a.print_prompt = "q";
    s::ostringstream out, err;
    EXPECT_EQ(dispatch(a, out, err, CancelToken{}), 2);
    EXPECT_NE(err.str().find(R"(unknown provider "openai")"), str::npos) << err.str();
}

TEST(Dispatch, PrintModeRunsThroughTheProvider) {
    // The faux provider made by make_provider has nothing queued, so the run
    // fails, but the failure proves print mode reached the provider.
    CliArgs a = with_model("faux", "x");
    a.print_prompt = "q";
    s::ostringstream out, err;
    EXPECT_EQ(dispatch(a, out, err, CancelToken{}), 1);
    EXPECT_NE(err.str().find("no faux response queued"), str::npos) << err.str();
}
