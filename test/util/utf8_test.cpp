#include <util/utf8.h>

#include <cstddef>
#include <string>

#include <gtest/gtest.h>

using namespace pi::util;

// ---------------------------------------------------------------------------
// decode_utf8
// ---------------------------------------------------------------------------

TEST(DecodeUtf8, ASCII) {
    std::string s = "A";
    std::size_t pos = 0;
    auto cp = decode_utf8(s, pos);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(*cp, U'A');
    EXPECT_EQ(pos, 1u);
}

TEST(DecodeUtf8, TwoByteSequence) {
    // U+00E9 LATIN SMALL LETTER E WITH ACUTE → 0xC3 0xA9
    std::string s = "\xC3\xA9";
    std::size_t pos = 0;
    auto cp = decode_utf8(s, pos);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(*cp, U'é');
    EXPECT_EQ(pos, 2u);
}

TEST(DecodeUtf8, ThreeByteSequence) {
    // U+4E2D CJK "middle" → 0xE4 0xB8 0xAD
    std::string s = "\xE4\xB8\xAD";
    std::size_t pos = 0;
    auto cp = decode_utf8(s, pos);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(*cp, U'中');
    EXPECT_EQ(pos, 3u);
}

TEST(DecodeUtf8, FourByteSequence) {
    // U+1F600 GRINNING FACE → 0xF0 0x9F 0x98 0x80
    std::string s = "\xF0\x9F\x98\x80";
    std::size_t pos = 0;
    auto cp = decode_utf8(s, pos);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(*cp, U'\U0001F600');
    EXPECT_EQ(pos, 4u);
}

TEST(DecodeUtf8, InvalidLeadByte) {
    // 0xFF is never valid in UTF-8.
    std::string s = "\xFF";
    std::size_t pos = 0;
    auto cp = decode_utf8(s, pos);
    EXPECT_FALSE(cp.has_value());
    EXPECT_EQ(pos, 0u);  // pos must be unchanged
}

TEST(DecodeUtf8, OverlongEncoding) {
    // Overlong encoding of U+0000: 0xC0 0x80 — not valid.
    std::string s = "\xC0\x80";
    std::size_t pos = 0;
    auto cp = decode_utf8(s, pos);
    EXPECT_FALSE(cp.has_value());
    EXPECT_EQ(pos, 0u);
}

TEST(DecodeUtf8, SurrogateHalf) {
    // U+D800 encoded as 3-byte: 0xED 0xA0 0x80 — not valid in UTF-8.
    std::string s = "\xED\xA0\x80";
    std::size_t pos = 0;
    auto cp = decode_utf8(s, pos);
    EXPECT_FALSE(cp.has_value());
    EXPECT_EQ(pos, 0u);
}

TEST(DecodeUtf8, TruncatedSequence) {
    // Two-byte lead byte but only one byte available.
    std::string s = "\xC3";
    std::size_t pos = 0;
    auto cp = decode_utf8(s, pos);
    EXPECT_FALSE(cp.has_value());
    EXPECT_EQ(pos, 0u);
}

TEST(DecodeUtf8, AdvancesThroughString) {
    // Decode "AB" character by character.
    std::string s = "AB";
    std::size_t pos = 0;
    auto first = decode_utf8(s, pos);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, U'A');
    EXPECT_EQ(pos, 1u);
    auto second = decode_utf8(s, pos);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, U'B');
    EXPECT_EQ(pos, 2u);
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
    EXPECT_EQ(display_width(std::string("\x00", 1)), 0u);
}
