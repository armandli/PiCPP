#include <util/strings.h>

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

using namespace pi::util;

// ---------------------------------------------------------------------------
// split
// ---------------------------------------------------------------------------

TEST(Split, BasicDelimiter) {
    auto parts = split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(Split, SingleToken) {
    auto parts = split("hello", ',');
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "hello");
}

TEST(Split, EmptyInput) {
    auto parts = split("", ',');
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "");
}

TEST(Split, ConsecutiveDelimiters) {
    auto parts = split("a,,b", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "");
    EXPECT_EQ(parts[2], "b");
}

TEST(Split, LeadingDelimiter) {
    auto parts = split(",a", ',');
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "");
    EXPECT_EQ(parts[1], "a");
}

TEST(Split, TrailingDelimiter) {
    auto parts = split("a,", ',');
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "");
}

TEST(Split, OnlyDelimiters) {
    auto parts = split(",,", ',');
    ASSERT_EQ(parts.size(), 3u);
    for (auto& p : parts) EXPECT_EQ(p, "");
}

// ---------------------------------------------------------------------------
// split_lines
// ---------------------------------------------------------------------------

TEST(SplitLines, UnixNewlines) {
    auto lines = split_lines("a\nb\nc");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
    EXPECT_EQ(lines[2], "c");
}

TEST(SplitLines, CRLFNewlines) {
    auto lines = split_lines("a\r\nb\r\nc");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
    EXPECT_EQ(lines[2], "c");
}

TEST(SplitLines, MixedLineEndings) {
    auto lines = split_lines("a\nb\r\nc");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
    EXPECT_EQ(lines[2], "c");
}

TEST(SplitLines, TrailingNewlineIsIgnored) {
    auto lines = split_lines("a\nb\n");
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
}

TEST(SplitLines, TrailingCRLFIsIgnored) {
    auto lines = split_lines("a\r\nb\r\n");
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
}

TEST(SplitLines, EmptyInput) {
    auto lines = split_lines("");
    // An empty string has no lines.
    EXPECT_TRUE(lines.empty());
}

TEST(SplitLines, SingleLineNoNewline) {
    auto lines = split_lines("hello");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "hello");
}

TEST(SplitLines, EmptyLines) {
    auto lines = split_lines("a\n\nb");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "");
    EXPECT_EQ(lines[2], "b");
}

// ---------------------------------------------------------------------------
// join (string_view overload)
// ---------------------------------------------------------------------------

TEST(JoinSV, MultipleElements) {
    std::vector<std::string_view> parts = {"a", "b", "c"};
    EXPECT_EQ(join(parts, ", "), "a, b, c");
}

TEST(JoinSV, SingleElement) {
    std::vector<std::string_view> parts = {"only"};
    EXPECT_EQ(join(parts, ","), "only");
}

TEST(JoinSV, EmptyVector) {
    std::vector<std::string_view> parts;
    EXPECT_EQ(join(parts, ","), "");
}

TEST(JoinSV, EmptySeparator) {
    std::vector<std::string_view> parts = {"a", "b", "c"};
    EXPECT_EQ(join(parts, ""), "abc");
}

// ---------------------------------------------------------------------------
// join (string overload)
// ---------------------------------------------------------------------------

TEST(JoinStr, MultipleElements) {
    std::vector<std::string> parts = {"foo", "bar", "baz"};
    EXPECT_EQ(join(parts, "-"), "foo-bar-baz");
}

TEST(JoinStr, EmptyVector) {
    std::vector<std::string> parts;
    EXPECT_EQ(join(parts, "-"), "");
}

// ---------------------------------------------------------------------------
// trim
// ---------------------------------------------------------------------------

TEST(Trim, LeadingSpaces) {
    EXPECT_EQ(trim("  hello"), "hello");
}

TEST(Trim, TrailingSpaces) {
    EXPECT_EQ(trim("hello  "), "hello");
}

TEST(Trim, BothEnds) {
    EXPECT_EQ(trim("  hello  "), "hello");
}

TEST(Trim, Tabs) {
    EXPECT_EQ(trim("\thello\t"), "hello");
}

TEST(Trim, NewlinesAndSpaces) {
    EXPECT_EQ(trim("\n  hello\r\n"), "hello");
}

TEST(Trim, OnlyWhitespace) {
    EXPECT_EQ(trim("   "), "");
}

TEST(Trim, EmptyInput) {
    EXPECT_EQ(trim(""), "");
}

TEST(Trim, NoWhitespace) {
    EXPECT_EQ(trim("hello"), "hello");
}

// ---------------------------------------------------------------------------
// starts_with / ends_with
// ---------------------------------------------------------------------------

TEST(StartsWith, MatchingPrefix) {
    EXPECT_TRUE(starts_with("hello world", "hello"));
}

TEST(StartsWith, NoMatch) {
    EXPECT_FALSE(starts_with("hello world", "world"));
}

TEST(StartsWith, EmptyPrefix) {
    EXPECT_TRUE(starts_with("hello", ""));
}

TEST(StartsWith, EmptyString) {
    EXPECT_FALSE(starts_with("", "hello"));
}

TEST(StartsWith, ExactMatch) {
    EXPECT_TRUE(starts_with("hello", "hello"));
}

TEST(StartsWith, PrefixLongerThanString) {
    EXPECT_FALSE(starts_with("hi", "hello"));
}

TEST(EndsWith, MatchingSuffix) {
    EXPECT_TRUE(ends_with("hello world", "world"));
}

TEST(EndsWith, NoMatch) {
    EXPECT_FALSE(ends_with("hello world", "hello"));
}

TEST(EndsWith, EmptySuffix) {
    EXPECT_TRUE(ends_with("hello", ""));
}

TEST(EndsWith, ExactMatch) {
    EXPECT_TRUE(ends_with("hello", "hello"));
}

TEST(EndsWith, SuffixLongerThanString) {
    EXPECT_FALSE(ends_with("hi", "hello"));
}

// ---------------------------------------------------------------------------
// replace_all
// ---------------------------------------------------------------------------

TEST(ReplaceAll, BasicReplacement) {
    EXPECT_EQ(replace_all("aababc", "ab", "X"), "aXXc");
}

TEST(ReplaceAll, NoOccurrences) {
    EXPECT_EQ(replace_all("hello", "xyz", "!!!"), "hello");
}

TEST(ReplaceAll, EmptyFrom) {
    EXPECT_EQ(replace_all("hello", "", "X"), "hello");
}

TEST(ReplaceAll, ReplaceWithEmpty) {
    EXPECT_EQ(replace_all("a-b-c", "-", ""), "abc");
}

TEST(ReplaceAll, SingleChar) {
    EXPECT_EQ(replace_all("banana", "a", "o"), "bonono");
}

TEST(ReplaceAll, ReplacementContainsFrom) {
    // Non-overlapping: "aa" in "aaaa" → "XX" (matches at offsets 0 and 2).
    EXPECT_EQ(replace_all("aaaa", "aa", "X"), "XX");
}

TEST(ReplaceAll, EmptyInput) {
    EXPECT_EQ(replace_all("", "a", "b"), "");
}
