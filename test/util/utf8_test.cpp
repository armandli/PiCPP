#include <util/utf8.h>

#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include <util/aliases.h>

using namespace pi::util;

// ---------------------------------------------------------------------------
// decode_utf8
// ---------------------------------------------------------------------------

TEST(DecodeUtf8, ASCII) {
    str text = "A";
    auto d = decode_utf8(text, 0);
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->code_point, U'A');
    EXPECT_EQ(d->next, 1u);
}

TEST(DecodeUtf8, TwoByteSequence) {
    // U+00E9 LATIN SMALL LETTER E WITH ACUTE → 0xC3 0xA9
    str text = "\xC3\xA9";
    auto d = decode_utf8(text, 0);
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->code_point, U'é');
    EXPECT_EQ(d->next, 2u);
}

TEST(DecodeUtf8, ThreeByteSequence) {
    // U+4E2D CJK "middle" → 0xE4 0xB8 0xAD
    str text = "\xE4\xB8\xAD";
    auto d = decode_utf8(text, 0);
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->code_point, U'中');
    EXPECT_EQ(d->next, 3u);
}

TEST(DecodeUtf8, FourByteSequence) {
    // U+1F600 GRINNING FACE → 0xF0 0x9F 0x98 0x80
    str text = "\xF0\x9F\x98\x80";
    auto d = decode_utf8(text, 0);
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->code_point, U'\U0001F600');
    EXPECT_EQ(d->next, 4u);
}

TEST(DecodeUtf8, InvalidLeadByte) {
    // 0xFF is never valid in UTF-8.
    str text = "\xFF";
    EXPECT_FALSE(decode_utf8(text, 0).has_value());
}

TEST(DecodeUtf8, OverlongEncoding) {
    // Overlong encoding of U+0000: 0xC0 0x80 — not valid.
    str text = "\xC0\x80";
    EXPECT_FALSE(decode_utf8(text, 0).has_value());
}

TEST(DecodeUtf8, OverlongFourByte) {
    // Overlong 4-byte encoding of U+0000: 0xF0 0x80 0x80 0x80 — not valid.
    str text = "\xF0\x80\x80\x80";
    EXPECT_FALSE(decode_utf8(text, 0).has_value());
}

TEST(DecodeUtf8, SurrogateHalf) {
    // U+D800 encoded as 3-byte: 0xED 0xA0 0x80 — not valid in UTF-8.
    str text = "\xED\xA0\x80";
    EXPECT_FALSE(decode_utf8(text, 0).has_value());
}

TEST(DecodeUtf8, AboveMaxCodePoint) {
    // 0xF4 0x90 0x80 0x80 would be U+110000, beyond the Unicode range.
    str text = "\xF4\x90\x80\x80";
    EXPECT_FALSE(decode_utf8(text, 0).has_value());
}

TEST(DecodeUtf8, TruncatedSequence) {
    // Two-byte lead byte but only one byte available.
    str text = "\xC3";
    EXPECT_FALSE(decode_utf8(text, 0).has_value());
}

TEST(DecodeUtf8, AdvancesThroughString) {
    // Decode "AB" character by character, feeding next back in as pos.
    str text = "AB";
    auto first = decode_utf8(text, 0);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, (DecodedCodePoint{U'A', 1}));
    auto second = decode_utf8(text, first->next);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, (DecodedCodePoint{U'B', 2}));
}

TEST(DecodeUtf8, MidStringPosition) {
    // Decoding from offset 1 of "A" + U+00E9 returns an absolute next position.
    str text = "A\xC3\xA9";
    auto d = decode_utf8(text, 1);
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->code_point, U'é');
    EXPECT_EQ(d->next, 3u);
}

TEST(DecodeUtf8, PosAtEnd) {
    str text = "A";
    EXPECT_FALSE(decode_utf8(text, 1).has_value());
    EXPECT_FALSE(decode_utf8(text, 5).has_value());
}

TEST(DecodeUtf8, ConstPosition) {
    // pos is taken by value: a const position is accepted and left unchanged.
    str text = "\xC3\xA9";
    const s::size_t pos = 0;
    auto [code_point, next] = *decode_utf8(text, pos);
    EXPECT_EQ(code_point, U'é');
    EXPECT_EQ(next, 2u);
    EXPECT_EQ(pos, 0u);
}

// ---------------------------------------------------------------------------
// is_valid_utf8
// ---------------------------------------------------------------------------

TEST(IsValidUtf8, PureASCII) {
    EXPECT_TRUE(is_valid_utf8("Hello, world!"));
}

TEST(IsValidUtf8, EmptyString) {
    EXPECT_TRUE(is_valid_utf8(""));
}

TEST(IsValidUtf8, ValidMultibyte) {
    // "café" — contains U+00E9
    EXPECT_TRUE(is_valid_utf8("caf\xC3\xA9"));
}

TEST(IsValidUtf8, ValidCJK) {
    // U+4E2D (3 bytes)
    EXPECT_TRUE(is_valid_utf8("\xE4\xB8\xAD"));
}

TEST(IsValidUtf8, ValidEmoji) {
    EXPECT_TRUE(is_valid_utf8("\xF0\x9F\x98\x80"));
}

TEST(IsValidUtf8, InvalidByte) {
    EXPECT_FALSE(is_valid_utf8("\xFF"));
}

TEST(IsValidUtf8, OverlongNull) {
    EXPECT_FALSE(is_valid_utf8("\xC0\x80"));
}

TEST(IsValidUtf8, SurrogateRange) {
    EXPECT_FALSE(is_valid_utf8("\xED\xA0\x80"));
}

TEST(IsValidUtf8, TruncatedSequence) {
    EXPECT_FALSE(is_valid_utf8("\xC3"));
}

TEST(IsValidUtf8, ValidThenInvalid) {
    EXPECT_FALSE(is_valid_utf8("ok\xFF"));
}

// ---------------------------------------------------------------------------
// count_code_points
// ---------------------------------------------------------------------------

TEST(CountCodePoints, ASCIIOnly) {
    EXPECT_EQ(count_code_points("hello"), 5u);
}

TEST(CountCodePoints, EmptyString) {
    EXPECT_EQ(count_code_points(""), 0u);
}

TEST(CountCodePoints, TwoByteCp) {
    // "café" = 4 code points even though the string is 5 bytes.
    EXPECT_EQ(count_code_points("caf\xC3\xA9"), 4u);
}

TEST(CountCodePoints, ThreeByteCp) {
    // Single 3-byte character = 1 code point.
    EXPECT_EQ(count_code_points("\xE4\xB8\xAD"), 1u);
}

TEST(CountCodePoints, FourByteCp) {
    // Single emoji = 1 code point.
    EXPECT_EQ(count_code_points("\xF0\x9F\x98\x80"), 1u);
}

TEST(CountCodePoints, Mixed) {
    // "A" + U+00E9 + "B" = 3 code points, 4 bytes.
    EXPECT_EQ(count_code_points("A\xC3\xA9""B"), 3u);
}

// ---------------------------------------------------------------------------
// display_width
// ---------------------------------------------------------------------------

TEST(DisplayWidth, ASCIIChars) {
    EXPECT_EQ(display_width("hello"), 5u);
}

TEST(DisplayWidth, EmptyString) {
    EXPECT_EQ(display_width(""), 0u);
}

TEST(DisplayWidth, CJKWideChars) {
    // U+4E2D is East-Asian Wide → 2 cells.
    EXPECT_EQ(display_width("\xE4\xB8\xAD"), 2u);
}

TEST(DisplayWidth, TwoCJKChars) {
    // Two CJK chars → 4 cells.  U+4E2D U+6587 ("Chinese text")
    EXPECT_EQ(display_width("\xE4\xB8\xAD\xE6\x96\x87"), 4u);
}

TEST(DisplayWidth, MixedASCIIAndWide) {
    // "A" (1) + U+4E2D (2) = 3 cells.
    EXPECT_EQ(display_width("A\xE4\xB8\xAD"), 3u);
}

TEST(DisplayWidth, ControlChar) {
    // Null byte is non-printable → 0 cells.
    EXPECT_EQ(display_width(str("\x00", 1)), 0u);
}

TEST(DisplayWidth, SingleEmoji) {
    // U+1F600 GRINNING FACE is East-Asian Wide → 2 cells.
    EXPECT_EQ(display_width("\xF0\x9F\x98\x80"), 2u);
}

TEST(DisplayWidth, AsciiAndEmoji) {
    // "A" (1) + U+1F600 (2) = 3 cells.
    EXPECT_EQ(display_width("A\xF0\x9F\x98\x80"), 3u);
}

TEST(DisplayWidth, RecentEmoji) {
    // U+1FAE0 MELTING FACE (Unicode 14) → 2 cells; checks the tables are current.
    EXPECT_EQ(display_width("\xF0\x9F\xAB\xA0"), 2u);
}

TEST(DisplayWidth, TextSymbolNarrow) {
    // U+2764 HEAVY BLACK HEART without VS16 uses text presentation → 1 cell.
    EXPECT_EQ(display_width("\xE2\x9D\xA4"), 1u);
}

TEST(DisplayWidth, Vs16MakesEmojiWide) {
    // U+2764 + U+FE0F VS16 requests emoji presentation → 2 cells.
    EXPECT_EQ(display_width("\xE2\x9D\xA4\xEF\xB8\x8F"), 2u);
}

TEST(DisplayWidth, SkinToneModifier) {
    // U+1F44D THUMBS UP + U+1F3FD skin tone → one 2-cell glyph.
    EXPECT_EQ(display_width("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD"), 2u);
}

TEST(DisplayWidth, StandaloneSkinTone) {
    // U+1F3FD skin tone on its own is drawn as a colour swatch → 2 cells.
    EXPECT_EQ(display_width("\xF0\x9F\x8F\xBD"), 2u);
}

TEST(DisplayWidth, ZwjFamily) {
    // U+1F468 ZWJ U+1F469 ZWJ U+1F467 (family) → one 2-cell glyph.
    EXPECT_EQ(display_width("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"), 2u);
}

TEST(DisplayWidth, ZwjThenAscii) {
    // Family emoji (2) + "x" (1) = 3 cells; the cluster ends at the ASCII char.
    EXPECT_EQ(display_width("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7""x"), 3u);
}

TEST(DisplayWidth, FlagPair) {
    // U+1F1EF U+1F1F5 regional indicators J P → Japanese flag, 2 cells.
    EXPECT_EQ(display_width("\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"), 2u);
}

TEST(DisplayWidth, TwoFlags) {
    // Japanese flag + U+1F1FA U+1F1F8 US flag → 4 cells.
    EXPECT_EQ(display_width("\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"), 4u);
}

TEST(DisplayWidth, LoneRegionalIndicator) {
    // A single U+1F1EF without a partner → 1 cell.
    EXPECT_EQ(display_width("\xF0\x9F\x87\xAF"), 1u);
}

TEST(DisplayWidth, Keycap) {
    // "1" + U+FE0F VS16 + U+20E3 COMBINING ENCLOSING KEYCAP → 2 cells.
    EXPECT_EQ(display_width("1\xEF\xB8\x8F\xE2\x83\xA3"), 2u);
}

TEST(DisplayWidth, TagSequenceFlag) {
    // U+1F3F4 black flag + tag characters g b e n g + cancel tag (England) → 2 cells.
    EXPECT_EQ(display_width("\xF0\x9F\x8F\xB4\xF3\xA0\x81\xA7\xF3\xA0\x81\xA2\xF3\xA0\x81\xA5\xF3\xA0\x81\xAE\xF3\xA0\x81\xA7\xF3\xA0\x81\xBF"), 2u);
}

TEST(DisplayWidth, CombiningAccent) {
    // "e" + U+0301 COMBINING ACUTE ACCENT → 1 cell.
    EXPECT_EQ(display_width("e\xCC\x81"), 1u);
}

TEST(DisplayWidth, FullwidthLatin) {
    // U+FF21 FULLWIDTH LATIN CAPITAL LETTER A → 2 cells.
    EXPECT_EQ(display_width("\xEF\xBC\xA1"), 2u);
}
