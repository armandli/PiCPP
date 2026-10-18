#include <http/sse.h>

#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <util/aliases.h>

using namespace pi::http;

// Feed `input` in one call.
static vec<SseEvent> decode_all(strv input) {
    SseDecoder d;
    return d.feed(input);
}

// Feed `input` one byte per call, collecting every event.
static vec<SseEvent> decode_bytewise(strv input) {
    SseDecoder d;
    vec<SseEvent> out;
    for (char c : input) {
        for (SseEvent& e : d.feed(strv(&c, 1))) out.push_back(s::move(e));
    }
    return out;
}

// Feed `input` cut at seeded random points.
static vec<SseEvent> decode_random(strv input, uint32_t seed) {
    s::mt19937 rng(seed);
    s::uniform_int_distribution<s::size_t> len(1, 7);
    SseDecoder d;
    vec<SseEvent> out;
    while (not input.empty()) {
        s::size_t n = s::min(len(rng), input.size());
        for (SseEvent& e : d.feed(input.substr(0, n))) out.push_back(s::move(e));
        input.remove_prefix(n);
    }
    return out;
}

static SseEvent ev(str data, str event = "message", str id = "") {
    return SseEvent{s::move(event), s::move(data), s::move(id)};
}

// ===========================================================================
// Basic fields
// ===========================================================================

TEST(Sse, SingleDataEvent) {
    EXPECT_EQ(decode_all("data: hello\n\n"), vec<SseEvent>{ev("hello")});
}

TEST(Sse, NothingBeforeBlankLine) {
    // An event is only dispatched by a blank line.
    EXPECT_TRUE(decode_all("data: hello\n").empty());
}

TEST(Sse, UnterminatedEventIsNeverEmitted) {
    // "a" is complete; "b" never gets its blank line.
    EXPECT_EQ(decode_all("data: a\n\ndata: b\n"), vec<SseEvent>{ev("a")});
}

TEST(Sse, TwoEvents) {
    EXPECT_EQ(decode_all("data: one\n\ndata: two\n\n"), (vec<SseEvent>{ev("one"), ev("two")}));
}

TEST(Sse, MultiLineDataJoinedWithNewline) {
    EXPECT_EQ(decode_all("data: line1\ndata: line2\ndata: line3\n\n"),
              vec<SseEvent>{ev("line1\nline2\nline3")});
}

TEST(Sse, OnlyOneLeadingSpaceStripped) {
    EXPECT_EQ(decode_all("data:  two spaces\n\n"), vec<SseEvent>{ev(" two spaces")});
}

TEST(Sse, NoSpaceAfterColon) {
    EXPECT_EQ(decode_all("data:tight\n\n"), vec<SseEvent>{ev("tight")});
}

TEST(Sse, ColonInValueKept) {
    EXPECT_EQ(decode_all("data: {\"a\":1}\n\n"), vec<SseEvent>{ev(R"({"a":1})")});
}

TEST(Sse, FieldWithoutColonHasEmptyValue) {
    // "data" alone appends an empty line: two of them give "\n".
    EXPECT_EQ(decode_all("data\ndata\n\n"), vec<SseEvent>{ev("\n")});
}

TEST(Sse, EmptyDataFieldDispatchesEmptyData) {
    // A data field was present (buffer is "\n" → ""), so the event is emitted.
    EXPECT_EQ(decode_all("data:\n\n"), vec<SseEvent>{ev("")});
}

TEST(Sse, EventTypeField) {
    EXPECT_EQ(decode_all("event: ping\ndata: x\n\n"), vec<SseEvent>{ev("x", "ping")});
}

TEST(Sse, EventTypeResetsAfterDispatch) {
    EXPECT_EQ(decode_all("event: ping\ndata: 1\n\ndata: 2\n\n"),
              (vec<SseEvent>{ev("1", "ping"), ev("2", "message")}));
}

TEST(Sse, IdPersistsAcrossEvents) {
    EXPECT_EQ(decode_all("id: 7\ndata: a\n\ndata: b\n\n"),
              (vec<SseEvent>{ev("a", "message", "7"), ev("b", "message", "7")}));
}

TEST(Sse, IdWithNulIsIgnored) {
    str input = "id: 1\ndata: a\n\nid: x";
    input += '\0';
    input += "y\ndata: b\n\n";
    EXPECT_EQ(decode_all(input), (vec<SseEvent>{ev("a", "message", "1"), ev("b", "message", "1")}));
}

TEST(Sse, RetryAndUnknownFieldsIgnored) {
    EXPECT_EQ(decode_all("retry: 1000\nfoo: bar\ndata: x\n\n"), vec<SseEvent>{ev("x")});
}

// ===========================================================================
// Comments and blank lines
// ===========================================================================

TEST(Sse, CommentLinesIgnored) {
    EXPECT_EQ(decode_all(": keep-alive\ndata: x\n: another\n\n"), vec<SseEvent>{ev("x")});
}

TEST(Sse, CommentOnlyBlockEmitsNothing) {
    EXPECT_TRUE(decode_all(": ping\n\n").empty());
}

TEST(Sse, BlankLineWithoutDataEmitsNothing) {
    EXPECT_TRUE(decode_all("\n\n\n").empty());
}

TEST(Sse, EventWithoutDataEmitsNothing) {
    // `event:` alone doesn't dispatch, and the type doesn't leak into the next event.
    EXPECT_EQ(decode_all("event: lonely\n\ndata: x\n\n"), vec<SseEvent>{ev("x")});
}

// ===========================================================================
// Line endings and BOM
// ===========================================================================

TEST(Sse, CrLfLineEndings) {
    EXPECT_EQ(decode_all("data: a\r\ndata: b\r\n\r\n"), vec<SseEvent>{ev("a\nb")});
}

TEST(Sse, LoneCrLineEndings) {
    EXPECT_EQ(decode_all("data: a\rdata: b\r\r"), vec<SseEvent>{ev("a\nb")});
}

TEST(Sse, MixedLineEndings) {
    EXPECT_EQ(decode_all("data: a\r\ndata: b\ndata: c\r\r\n"), vec<SseEvent>{ev("a\nb\nc")});
}

TEST(Sse, BomAtStartSkipped) {
    EXPECT_EQ(decode_all("\xEF\xBB\xBF" "data: x\n\n"), vec<SseEvent>{ev("x")});
}

TEST(Sse, BomOnlySkippedAtStreamStart) {
    // A BOM after the first byte is ordinary data.
    EXPECT_EQ(decode_all("data: \xEF\xBB\xBFx\n\n"), vec<SseEvent>{ev("\xEF\xBB\xBFx")});
}

// ===========================================================================
// Chunk boundaries
// ===========================================================================

TEST(SseChunks, CrLfSplitAcrossFeeds) {
    // "\r" ends one feed and "\n" starts the next: one line ending, not two.
    SseDecoder d;
    vec<SseEvent> out;
    for (strv part : {"data: a\r", "\ndata: b\r", "\n\r", "\n"}) {
        for (SseEvent& e : d.feed(part)) out.push_back(s::move(e));
    }
    EXPECT_EQ(out, vec<SseEvent>{ev("a\nb")});
}

TEST(SseChunks, BomSplitAcrossFeeds) {
    SseDecoder d;
    vec<SseEvent> out;
    for (strv part : {"\xEF", "\xBB", "\xBF" "da", "ta: x\n\n"}) {
        for (SseEvent& e : d.feed(part)) out.push_back(s::move(e));
    }
    EXPECT_EQ(out, vec<SseEvent>{ev("x")});
}

TEST(SseChunks, Utf8SequenceSplitAcrossFeeds) {
    SseDecoder d;
    vec<SseEvent> out;
    // "é" is C3 A9; "€" is E2 82 AC.
    for (strv part : {"data: caf\xC3", "\xA9 \xE2\x82", "\xAC\n\n"}) {
        for (SseEvent& e : d.feed(part)) out.push_back(s::move(e));
    }
    EXPECT_EQ(out, vec<SseEvent>{ev("caf\xC3\xA9 \xE2\x82\xAC")});
}

TEST(SseChunks, FieldNameSplitAcrossFeeds) {
    SseDecoder d;
    vec<SseEvent> out;
    for (strv part : {"da", "ta", ":", " x", "\n", "\n"}) {
        for (SseEvent& e : d.feed(part)) out.push_back(s::move(e));
    }
    EXPECT_EQ(out, vec<SseEvent>{ev("x")});
}

TEST(SseChunks, EmptyFeedIsHarmless) {
    SseDecoder d;
    EXPECT_TRUE(d.feed("").empty());
    EXPECT_TRUE(d.feed("data: x\n").empty());
    EXPECT_TRUE(d.feed("").empty());
    EXPECT_EQ(d.feed("\n"), vec<SseEvent>{ev("x")});
}

// An OpenAI / Ollama style stream with every tricky feature at once.
static const str kStream =
    "\xEF\xBB\xBF"
    ": ping\r\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}\r\n\r\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"lo \xE2\x82\xAC\"}}]}\n\n"
    "event: usage\r"
    "data: {\"usage\":\r"
    "data: {\"total\":3}}\r\r"
    "id: 42\n"
    "data: [DONE]\n\n";

TEST(SseChunks, RealisticStreamDecodesAsExpected) {
    vec<SseEvent> want = {
        ev(R"({"choices":[{"delta":{"content":"Hel"}}]})"),
        ev("{\"choices\":[{\"delta\":{\"content\":\"lo \xE2\x82\xAC\"}}]}"),
        ev("{\"usage\":\n{\"total\":3}}", "usage"),
        ev("[DONE]", "message", "42"),
    };
    EXPECT_EQ(decode_all(kStream), want);
}

TEST(SseChunks, ByteAtATimeEqualsAllAtOnce) {
    EXPECT_EQ(decode_bytewise(kStream), decode_all(kStream));
}

TEST(SseChunks, RandomSplitsEqualAllAtOnce) {
    vec<SseEvent> whole = decode_all(kStream);
    for (uint32_t seed = 0; seed < 200; ++seed) {
        ASSERT_EQ(decode_random(kStream, seed), whole) << "seed=" << seed;
    }
}

TEST(SseChunks, EverySingleSplitPointEqualsAllAtOnce) {
    vec<SseEvent> whole = decode_all(kStream);
    for (s::size_t cut = 0; cut <= kStream.size(); ++cut) {
        SseDecoder d;
        vec<SseEvent> out = d.feed(strv(kStream).substr(0, cut));
        for (SseEvent& e : d.feed(strv(kStream).substr(cut))) out.push_back(s::move(e));
        ASSERT_EQ(out, whole) << "cut=" << cut;
    }
}
