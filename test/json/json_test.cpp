#include <json/parse.h>
#include <json/value.h>
#include <json/write.h>

#include <cstdint>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include <util/aliases.h>

using namespace pi::json;

// ===========================================================================
// Value — construction and type queries
// ===========================================================================

TEST(ValueType, DefaultIsNull) {
    Value v;
    EXPECT_TRUE(v.is_null());
    EXPECT_FALSE(v.is_bool());
    EXPECT_FALSE(v.is_int());
    EXPECT_FALSE(v.is_double());
    EXPECT_FALSE(v.is_string());
    EXPECT_FALSE(v.is_array());
    EXPECT_FALSE(v.is_object());
}

TEST(ValueType, NullptrIsNull) {
    Value v(nullptr);
    EXPECT_TRUE(v.is_null());
}

TEST(ValueType, Bool) {
    Value t(true);
    EXPECT_TRUE(t.is_bool());
    EXPECT_FALSE(t.is_null());

    Value f(false);
    EXPECT_TRUE(f.is_bool());
}

TEST(ValueType, Int) {
    Value v(int64_t{42});
    EXPECT_TRUE(v.is_int());
    EXPECT_TRUE(v.is_number());
    EXPECT_FALSE(v.is_double());
}

TEST(ValueType, IntFromInt) {
    Value v(7);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 42 - 35);  // 7
}

TEST(ValueType, Double) {
    Value v(3.14);
    EXPECT_TRUE(v.is_double());
    EXPECT_TRUE(v.is_number());
    EXPECT_FALSE(v.is_int());
}

TEST(ValueType, String) {
    Value v(str{"hello"});
    EXPECT_TRUE(v.is_string());
}

TEST(ValueType, StringFromLiteral) {
    Value v("world");
    EXPECT_TRUE(v.is_string());
}

TEST(ValueType, Array) {
    Value v(Array{});
    EXPECT_TRUE(v.is_array());
}

TEST(ValueType, Object) {
    Value v(Object{});
    EXPECT_TRUE(v.is_object());
}

// ===========================================================================
// Value — as_* extraction
// ===========================================================================

TEST(ValueExtract, AsBoolTrue) {
    Value v(true);
    ASSERT_TRUE(v.as_bool().has_value());
    EXPECT_EQ(*v.as_bool(), true);
}

TEST(ValueExtract, AsBoolFalse) {
    Value v(false);
    ASSERT_TRUE(v.as_bool().has_value());
    EXPECT_EQ(*v.as_bool(), false);
}

TEST(ValueExtract, AsBoolWrongType) {
    Value v(int64_t{1});
    EXPECT_FALSE(v.as_bool().has_value());
}

TEST(ValueExtract, AsInt) {
    Value v(int64_t{-99});
    ASSERT_TRUE(v.as_int().has_value());
    EXPECT_EQ(*v.as_int(), -99);
}

TEST(ValueExtract, AsIntWrongType) {
    Value v(3.14);
    EXPECT_FALSE(v.as_int().has_value());
}

TEST(ValueExtract, AsDouble) {
    Value v(1.5);
    ASSERT_TRUE(v.as_double().has_value());
    EXPECT_DOUBLE_EQ(*v.as_double(), 1.5);
}

TEST(ValueExtract, AsDoubleWrongType) {
    Value v(int64_t{1});
    EXPECT_FALSE(v.as_double().has_value());
}

TEST(ValueExtract, AsString) {
    Value v("hello");
    ASSERT_TRUE(v.as_string().has_value());
    EXPECT_EQ(*v.as_string(), "hello");
}

TEST(ValueExtract, AsStringWrongType) {
    Value v;
    EXPECT_FALSE(v.as_string().has_value());
}

TEST(ValueExtract, AsArrayNonNull) {
    Value v(Array{});
    EXPECT_NE(v.as_array(), nullptr);
}

TEST(ValueExtract, AsArrayWrongType) {
    Value v;
    EXPECT_EQ(v.as_array(), nullptr);
}

TEST(ValueExtract, AsObjectNonNull) {
    Value v(Object{});
    EXPECT_NE(v.as_object(), nullptr);
}

TEST(ValueExtract, AsObjectWrongType) {
    Value v;
    EXPECT_EQ(v.as_object(), nullptr);
}

// ===========================================================================
// Value — copy and move
// ===========================================================================

TEST(ValueCopy, CopyNull) {
    Value a;
    Value b = a;
    EXPECT_TRUE(b.is_null());
}

TEST(ValueCopy, CopyString) {
    Value a("hello");
    Value b = a;
    ASSERT_TRUE(b.as_string().has_value());
    EXPECT_EQ(*b.as_string(), "hello");
    // Modifying the original does not affect the copy.
    a = Value("goodbye");
    EXPECT_EQ(*b.as_string(), "hello");
}

TEST(ValueCopy, CopyArray) {
    Array arr;
    arr.push_back(Value(int64_t{1}));
    Value a(s::move(arr));
    Value b = a;
    ASSERT_NE(b.as_array(), nullptr);
    EXPECT_EQ(b.as_array()->size(), 1u);
}

TEST(ValueMove, MoveString) {
    Value a("hello");
    Value b = s::move(a);
    EXPECT_TRUE(a.is_null());  // moved-from is null
    ASSERT_TRUE(b.as_string().has_value());
    EXPECT_EQ(*b.as_string(), "hello");
}

// ===========================================================================
// Value — object accessors
// ===========================================================================

TEST(ValueObject, SetAndFind) {
    Value obj(Object{});
    obj.set("key", Value("val"));
    Value* found = obj.find("key");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found->as_string(), "val");
}

TEST(ValueObject, FindMissing) {
    Value obj(Object{});
    EXPECT_EQ(obj.find("absent"), nullptr);
}

TEST(ValueObject, SetOverwrite) {
    Value obj(Object{});
    obj.set("x", Value(int64_t{1}));
    obj.set("x", Value(int64_t{2}));
    Value* v = obj.find("x");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v->as_int(), 2);
}

TEST(ValueObject, SetPreservesInsertionOrder) {
    Value obj(Object{});
    obj.set("c", Value(int64_t{3}));
    obj.set("a", Value(int64_t{1}));
    obj.set("b", Value(int64_t{2}));

    Object* o = obj.as_object();
    ASSERT_NE(o, nullptr);
    ASSERT_EQ(o->entries.size(), 3u);
    EXPECT_EQ(o->entries[0].first, "c");
    EXPECT_EQ(o->entries[1].first, "a");
    EXPECT_EQ(o->entries[2].first, "b");
}

TEST(ValueObject, Size) {
    Value obj(Object{});
    EXPECT_EQ(obj.size(), 0u);
    EXPECT_TRUE(obj.empty());
    obj.set("k", Value());
    EXPECT_EQ(obj.size(), 1u);
    EXPECT_FALSE(obj.empty());
}

// ===========================================================================
// Value — array accessors
// ===========================================================================

TEST(ValueArray, PushBackAndAccess) {
    Value arr(Array{});
    arr.push_back(Value(int64_t{10}));
    arr.push_back(Value(int64_t{20}));
    EXPECT_EQ(arr.size(), 2u);
    EXPECT_EQ(*arr[s::size_t{0}].as_int(), 10);
    EXPECT_EQ(*arr[s::size_t{1}].as_int(), 20);
}

TEST(ValueArray, Size) {
    Value arr(Array{});
    EXPECT_EQ(arr.size(), 0u);
    EXPECT_TRUE(arr.empty());
    arr.push_back(Value());
    EXPECT_EQ(arr.size(), 1u);
}

// ===========================================================================
// Value — equality
// ===========================================================================

TEST(ValueEq, NullEquality) {
    EXPECT_EQ(Value(), Value());
    EXPECT_EQ(Value(nullptr), Value());
}

TEST(ValueEq, BoolEquality) {
    EXPECT_EQ(Value(true), Value(true));
    EXPECT_EQ(Value(false), Value(false));
    EXPECT_NE(Value(true), Value(false));
}

TEST(ValueEq, IntEquality) {
    EXPECT_EQ(Value(int64_t{42}), Value(int64_t{42}));
    EXPECT_NE(Value(int64_t{42}), Value(int64_t{43}));
}

TEST(ValueEq, CrossTypeInequality) {
    EXPECT_NE(Value(int64_t{1}), Value(true));
    EXPECT_NE(Value(int64_t{0}), Value());
    EXPECT_NE(Value(""), Value());
}

TEST(ValueEq, StringEquality) {
    EXPECT_EQ(Value("hello"), Value("hello"));
    EXPECT_NE(Value("hello"), Value("world"));
}

TEST(ValueEq, NestedArrayEquality) {
    Array a1, a2;
    a1.push_back(Value(int64_t{1}));
    a2.push_back(Value(int64_t{1}));
    EXPECT_EQ(Value(s::move(a1)), Value(s::move(a2)));
}

TEST(ValueEq, ObjectEquality) {
    Value o1(Object{}), o2(Object{});
    o1.set("k", Value(int64_t{1}));
    o2.set("k", Value(int64_t{1}));
    EXPECT_EQ(o1, o2);
}

TEST(ValueEq, ObjectKeyOrderMatters) {
    Value o1(Object{}), o2(Object{});
    o1.set("a", Value(int64_t{1}));
    o1.set("b", Value(int64_t{2}));
    o2.set("b", Value(int64_t{2}));
    o2.set("a", Value(int64_t{1}));
    // Different insertion order → not equal.
    EXPECT_NE(o1, o2);
}

// ===========================================================================
// parse
// ===========================================================================

TEST(Parse, Null) {
    auto r = parse("null");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->is_null());
}

TEST(Parse, BoolTrue) {
    auto r = parse("true");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_bool().has_value());
    EXPECT_EQ(*r->as_bool(), true);
}

TEST(Parse, BoolFalse) {
    auto r = parse("false");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_bool().has_value());
    EXPECT_EQ(*r->as_bool(), false);
}

TEST(Parse, Integer) {
    auto r = parse("42");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_int().has_value());
    EXPECT_EQ(*r->as_int(), 42);
}

TEST(Parse, NegativeInteger) {
    auto r = parse("-7");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_int().has_value());
    EXPECT_EQ(*r->as_int(), -7);
}

TEST(Parse, Double) {
    auto r = parse("3.14");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_double().has_value());
    EXPECT_DOUBLE_EQ(*r->as_double(), 3.14);
}

TEST(Parse, SimpleString) {
    auto r = parse(R"("hello")");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_string().has_value());
    EXPECT_EQ(*r->as_string(), "hello");
}

TEST(Parse, StringWithEscapes) {
    // Input: "\"\\n" (quote, backslash, letter n)
    auto r = parse(R"("\"\\n")");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_string().has_value());
    EXPECT_EQ(*r->as_string(), "\"\\n");
}

TEST(Parse, StringWithUnicodeEscape) {
    // A = 'A'
    auto r = parse(R"("A")");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_string().has_value());
    EXPECT_EQ(*r->as_string(), "A");
}

TEST(Parse, EmptyArray) {
    auto r = parse("[]");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->is_array());
    EXPECT_EQ(r->size(), 0u);
}

TEST(Parse, ArrayOfInts) {
    auto r = parse("[1,2,3]");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->is_array());
    EXPECT_EQ(r->size(), 3u);
    EXPECT_EQ(*r->operator[](s::size_t{0}).as_int(), 1);
    EXPECT_EQ(*r->operator[](s::size_t{1}).as_int(), 2);
    EXPECT_EQ(*r->operator[](s::size_t{2}).as_int(), 3);
}

TEST(Parse, EmptyObject) {
    auto r = parse("{}");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->is_object());
    EXPECT_EQ(r->size(), 0u);
}

TEST(Parse, SimpleObject) {
    auto r = parse(R"({"x":1,"y":2})");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->is_object());
    EXPECT_EQ(r->size(), 2u);
    Value* x = r->find("x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(*x->as_int(), 1);
    Value* y = r->find("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(*y->as_int(), 2);
}

TEST(Parse, NestedObject) {
    auto r = parse(R"({"a":{"b":42}})");
    ASSERT_TRUE(r.has_value());
    Value* a = r->find("a");
    ASSERT_NE(a, nullptr);
    ASSERT_TRUE(a->is_object());
    Value* b = a->find("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b->as_int(), 42);
}

TEST(Parse, ParseErrorOnTruncated) {
    auto r = parse("{");
    EXPECT_FALSE(r.has_value());
}

TEST(Parse, ParseErrorOnInvalidEscape) {
    auto r = parse(R"("\q")");
    EXPECT_FALSE(r.has_value());
}

TEST(Parse, ParseErrorOnTrailingGarbage) {
    auto r = parse("42 garbage");
    EXPECT_FALSE(r.has_value());
}

TEST(Parse, ParseErrorHasNonZeroOffset) {
    // Typically an error near the end has a non-zero offset.
    auto r = parse(R"({"key": !!})");
    ASSERT_FALSE(r.has_value());
    // We can't assert an exact value but the message or offset should be set.
    (void)r.error().offset;
    (void)r.error().message;
}

// ===========================================================================
// parse — error offsets
//
// ParseError::offset is the byte index of the first byte at which the input
// stops being a prefix of any valid JSON document.  If the whole input is a
// valid prefix but the document is incomplete (truncated), the offset is
// input.size().  Offsets are 0-based byte indices, not code points or columns.
// ===========================================================================

static s::size_t error_offset(strv input) {
    auto r = parse(input);
    EXPECT_FALSE(r.has_value()) << "input unexpectedly parsed: " << input;
    return r.has_value() ? s::size_t(-1) : r.error().offset;
}

TEST(ParseErrorOffset, UnexpectedTokenInObjectValue) {
    // {"key": !!}
    // 0123456789
    EXPECT_EQ(error_offset(R"({"key": !!})"), 8u);
}

TEST(ParseErrorOffset, TrailingCommaInObject) {
    // {"a": 1,}  -> the '}' after ',' is invalid
    EXPECT_EQ(error_offset(R"({"a": 1,})"), 8u);
}

TEST(ParseErrorOffset, TrailingCommaInArray) {
    // [1,2,]  -> the ']' after ',' is invalid
    EXPECT_EQ(error_offset("[1,2,]"), 5u);
}

TEST(ParseErrorOffset, MissingCommaBetweenArrayElements) {
    // [1 2]  -> '2' where ',' or ']' was expected
    EXPECT_EQ(error_offset("[1 2]"), 3u);
}

TEST(ParseErrorOffset, NonStringObjectKey) {
    // {1: 2}  -> '1' where a string key was expected
    EXPECT_EQ(error_offset("{1: 2}"), 1u);
}

TEST(ParseErrorOffset, InvalidEscapeCharacter) {
    // "\q"  -> 'q' is not a valid escape
    EXPECT_EQ(error_offset(R"("\q")"), 2u);
}

TEST(ParseErrorOffset, InvalidLiteral) {
    // trux  -> 'x' breaks the literal "true"
    EXPECT_EQ(error_offset("trux"), 3u);
}

TEST(ParseErrorOffset, TrailingGarbageAfterValue) {
    // [1] x  -> whitespace after the value is fine, 'x' is not
    EXPECT_EQ(error_offset("[1] x"), 4u);
}

TEST(ParseErrorOffset, InvalidUtf8InString) {
    // "\xFF"  -> 0xFF can never start a UTF-8 sequence
    EXPECT_EQ(error_offset("\"\xFF\""), 1u);
}

TEST(ParseErrorOffset, ErrorOnLaterLineIsByteOffset) {
    // Missing ',' before "b": offset counts '\n' bytes, it is not a column.
    str input = "{\n  \"a\": 1\n  \"b\": 2\n}";
    EXPECT_EQ(error_offset(input), input.find("\"b\""));
    EXPECT_EQ(error_offset(input), 13u);
}

TEST(ParseErrorOffset, TruncatedObjectPointsAtEnd) {
    EXPECT_EQ(error_offset("{"), 1u);
}

TEST(ParseErrorOffset, TruncatedArrayPointsAtEnd) {
    EXPECT_EQ(error_offset("[1, 2"), 5u);
}

TEST(ParseErrorOffset, TruncatedLiteralPointsAtEnd) {
    EXPECT_EQ(error_offset("tru"), 3u);
}

TEST(ParseErrorOffset, UnterminatedStringPointsAtEnd) {
    EXPECT_EQ(error_offset(R"("abc)"), 4u);
}

TEST(ParseErrorOffset, WhitespaceOnlyPointsAtEnd) {
    EXPECT_EQ(error_offset("   "), 3u);
}

TEST(ParseErrorOffset, EmptyInputIsOffsetZero) {
    EXPECT_EQ(error_offset(""), 0u);
}

TEST(ParseErrorOffset, OffsetNeverExceedsInputSize) {
    for (strv input : {"{", "[1,", R"({"a":})", "nul", "\"x", "[1]]", "{}{}"}) {
        EXPECT_LE(error_offset(input), input.size()) << "input: " << input;
    }
}

// ===========================================================================
// parse — big integers
//
// Decision: integers in [INT64_MIN, INT64_MAX] parse as Kind::Int, exactly.
// Integers outside that range parse as Kind::Double: the nearest double,
// accepting the loss of precision (doubles are exact only up to 2^53).  This
// matches JavaScript's JSON.parse, which Pi relies on.  They never wrap
// around and never cause a parse error.  Integers beyond DBL_MAX (~1.8e308)
// become infinity, just as JSON.parse returns Infinity.
// ===========================================================================

TEST(ParseBigInt, Int64MaxIsExactInt) {
    auto r = parse("9223372036854775807");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->is_int());
    EXPECT_EQ(*r->as_int(), s::numeric_limits<int64_t>::max());
}

TEST(ParseBigInt, Int64MinIsExactInt) {
    auto r = parse("-9223372036854775808");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->is_int());
    EXPECT_EQ(*r->as_int(), s::numeric_limits<int64_t>::min());
}

TEST(ParseBigInt, Int64MaxPlusOneDoesNotWrapNegative) {
    // 2^63: one past INT64_MAX.  A static_cast<int64_t> wraps it to INT64_MIN.
    auto r = parse("9223372036854775808");
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->is_int() and *r->as_int() < 0)
        << "2^63 wrapped to " << *r->as_int();
    ASSERT_TRUE(r->is_double());
    EXPECT_EQ(*r->as_double(), 9223372036854775808.0);
}

TEST(ParseBigInt, Uint64MaxDoesNotWrapToMinusOne) {
    // 2^64 - 1.  A static_cast<int64_t> wraps it to -1.
    auto r = parse("18446744073709551615");
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->is_int() and *r->as_int() == -1)
        << "UINT64_MAX wrapped to -1";
    ASSERT_TRUE(r->is_double());
    EXPECT_DOUBLE_EQ(*r->as_double(), 18446744073709551615.0);
}

TEST(ParseBigInt, BigIntInArrayKeepsSign) {
    // Wrapping inside containers is just as wrong: element must stay positive.
    auto r = parse("[1, 10000000000000000000]");
    ASSERT_TRUE(r.has_value());
    const Array* arr = r->as_array();
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->size(), 2u);
    ASSERT_TRUE((*arr)[1].is_double());
    EXPECT_EQ(*(*arr)[1].as_double(), 1e19);
}

TEST(ParseBigInt, BeyondUint64ParsesAsDouble) {
    // 2^64: too large for any 64-bit integer type.
    auto r = parse("18446744073709551616");
    ASSERT_TRUE(r.has_value()) << r.error().message;
    ASSERT_TRUE(r->is_double());
    EXPECT_EQ(*r->as_double(), 18446744073709551616.0);
}

TEST(ParseBigInt, BelowInt64MinParsesAsDouble) {
    // -(2^63) - 1: one below INT64_MIN.
    auto r = parse("-9223372036854775809");
    ASSERT_TRUE(r.has_value()) << r.error().message;
    ASSERT_TRUE(r->is_double());
    EXPECT_EQ(*r->as_double(), -9223372036854775808.0);
}

TEST(RoundTrip, Int64Extremes) {
    for (int64_t i : {s::numeric_limits<int64_t>::max(), s::numeric_limits<int64_t>::min()}) {
        Value v(i);
        auto r = parse(to_json(v));
        ASSERT_TRUE(r.has_value()) << "i=" << i;
        EXPECT_EQ(*r, v) << "i=" << i;
    }
}

TEST(ParseBigInt, Int64MaxPlusOneLosesTheLowBits) {
    // 2^63 + 1 is not representable as a double: it rounds to 2^63.
    auto r = parse("9223372036854775809");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->is_double());
    EXPECT_EQ(*r->as_double(), 9223372036854775808.0);
}

TEST(ParseBigInt, DistinctBigIntsMayCollapseToSameDouble) {
    // Precision loss is accepted: 2^64 and 2^64 + 1 parse to the same value.
    auto a = parse("18446744073709551616");
    auto b = parse("18446744073709551617");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(*a, *b);
}

TEST(ParseBigInt, NegativeBeyond64BitsParsesAsDouble) {
    auto r = parse("-18446744073709551616");
    ASSERT_TRUE(r.has_value()) << r.error().message;
    ASSERT_TRUE(r->is_double());
    EXPECT_EQ(*r->as_double(), -18446744073709551616.0);
}

TEST(ParseBigInt, BigIntInObjectParsesAsDouble) {
    auto r = parse(R"({"id": 123456789012345678901234567890, "n": 1})");
    ASSERT_TRUE(r.has_value()) << r.error().message;
    const Value* id = r->find("id");
    ASSERT_NE(id, nullptr);
    ASSERT_TRUE(id->is_double());
    EXPECT_DOUBLE_EQ(*id->as_double(), 1.2345678901234568e29);
    // Neighbouring ordinary integers are unaffected.
    ASSERT_TRUE(r->find("n")->is_int());
}

TEST(ParseBigInt, IntegerBeyondDoubleRangeBecomesInfinity) {
    // 400 digits > DBL_MAX: like JSON.parse, the result is +/-Infinity.
    auto pos = parse(str(400, '9'));
    ASSERT_TRUE(pos.has_value()) << pos.error().message;
    ASSERT_TRUE(pos->is_double());
    EXPECT_EQ(*pos->as_double(), s::numeric_limits<double>::infinity());

    auto neg = parse("-" + str(400, '9'));
    ASSERT_TRUE(neg.has_value()) << neg.error().message;
    ASSERT_TRUE(neg->is_double());
    EXPECT_EQ(*neg->as_double(), -s::numeric_limits<double>::infinity());
}

TEST(ParseBigInt, OrdinaryNumbersUnaffected) {
    // Keeping big integers as strings in simdjson must not change other numbers.
    auto r = parse("[0, -1, 1.5, 2e3, 9223372036854775807]");
    ASSERT_TRUE(r.has_value());
    const Array& a = *r->as_array();
    EXPECT_TRUE(a[0].is_int());
    EXPECT_TRUE(a[1].is_int());
    EXPECT_TRUE(a[2].is_double());
    EXPECT_TRUE(a[3].is_double());
    EXPECT_TRUE(a[4].is_int());
}

TEST(RoundTrip, BigIntComesBackAsDouble) {
    // Lossy by design: the original digits are not preserved.
    auto r = parse("18446744073709551615");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(to_json(*r), "1.8446744073709552e+19");
}

TEST(RoundTrip, BigIntBeyondDoubleRangeWritesNull) {
    // Infinity has no JSON form, so to_json writes null (see ToJson.*WritesNull).
    auto r = parse(str(400, '9'));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(to_json(*r), "null");
}

// ===========================================================================
// to_json
// ===========================================================================

TEST(ToJson, Null) {
    EXPECT_EQ(to_json(Value{}), "null");
}

TEST(ToJson, BoolTrue) {
    EXPECT_EQ(to_json(Value(true)), "true");
}

TEST(ToJson, BoolFalse) {
    EXPECT_EQ(to_json(Value(false)), "false");
}

TEST(ToJson, PositiveInt) {
    EXPECT_EQ(to_json(Value(int64_t{42})), "42");
}

TEST(ToJson, NegativeInt) {
    EXPECT_EQ(to_json(Value(int64_t{-1})), "-1");
}

TEST(ToJson, Double) {
    // Shortest round-trip: 3.14 must parse back to exactly 3.14.
    str text = to_json(Value(3.14));
    auto r = parse(text);
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_double().has_value());
    EXPECT_DOUBLE_EQ(*r->as_double(), 3.14);
}

TEST(ToJson, IntegralDoubleKeepsDecimalPoint) {
    // Integral doubles get ".0" so they parse back as Double, not Int.
    EXPECT_EQ(to_json(Value(0.0)), "0.0");
    EXPECT_EQ(to_json(Value(1.0)), "1.0");
    EXPECT_EQ(to_json(Value(-1.0)), "-1.0");
    // Exponent form already parses as a double; no ".0" appended.
    EXPECT_EQ(to_json(Value(1e100)), "1e+100");
}

// JSON (RFC 8259) has no literal for NaN or Infinity. to_json follows
// JSON.stringify and writes null for them, so the output is always valid JSON.
// The original double cannot be recovered: parsing the result yields Null.

TEST(ToJson, NaNWritesNull) {
    EXPECT_EQ(to_json(Value(s::numeric_limits<double>::quiet_NaN())), "null");
}

TEST(ToJson, PositiveInfinityWritesNull) {
    EXPECT_EQ(to_json(Value(s::numeric_limits<double>::infinity())), "null");
}

TEST(ToJson, NegativeInfinityWritesNull) {
    EXPECT_EQ(to_json(Value(-s::numeric_limits<double>::infinity())), "null");
}

TEST(ToJson, NonFiniteInContainersWritesNull) {
    // Non-finite doubles nested in arrays / objects must also produce valid JSON.
    Array arr;
    arr.push_back(Value(s::numeric_limits<double>::quiet_NaN()));
    arr.push_back(Value(s::numeric_limits<double>::infinity()));
    EXPECT_EQ(to_json(Value(s::move(arr))), "[null,null]");

    Value obj(Object{});
    obj.set("x", Value(-s::numeric_limits<double>::infinity()));
    EXPECT_EQ(to_json(obj), R"({"x":null})");
}

TEST(RoundTrip, NonFiniteParsesAsNull) {
    // Lossy by design: a non-finite double comes back as Null.
    auto r = parse(to_json(Value(s::numeric_limits<double>::quiet_NaN())));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->is_null());
}

TEST(ToJson, SimpleString) {
    EXPECT_EQ(to_json(Value("hello")), R"("hello")");
}

TEST(ToJson, StringEscapesQuote) {
    EXPECT_EQ(to_json(Value(str{R"(say "hi")"})), R"("say \"hi\"")");
}

TEST(ToJson, StringEscapesBackslash) {
    EXPECT_EQ(to_json(Value(str{"a\\b"})), R"("a\\b")");
}

TEST(ToJson, StringEscapesNewline) {
    EXPECT_EQ(to_json(Value(str{"a\nb"})), R"("a\nb")");
}

TEST(ToJson, StringEscapesTab) {
    EXPECT_EQ(to_json(Value(str{"a\tb"})), R"("a\tb")");
}

TEST(ToJson, StringEscapesCarriageReturn) {
    EXPECT_EQ(to_json(Value(str{"a\rb"})), R"("a\rb")");
}

TEST(ToJson, StringEscapesControlChar) {
    // U+0001 must be written as \u0001.
    str text = to_json(Value(str{"\x01"}));
    EXPECT_EQ(text, R"("\u0001")");
}

TEST(ToJson, StringPassesThroughUtf8) {
    // Non-ASCII UTF-8 bytes must be passed through, not escaped.
    // U+00E9 (é) = 0xC3 0xA9
    str input = "caf\xC3\xA9";
    str expected = "\"caf\xC3\xA9\"";
    EXPECT_EQ(to_json(Value(input)), expected);
}

TEST(ToJson, EmptyArray) {
    EXPECT_EQ(to_json(Value(Array{})), "[]");
}

TEST(ToJson, ArrayOfInts) {
    Array a;
    a.push_back(Value(int64_t{1}));
    a.push_back(Value(int64_t{2}));
    a.push_back(Value(int64_t{3}));
    EXPECT_EQ(to_json(Value(s::move(a))), "[1,2,3]");
}

TEST(ToJson, EmptyObject) {
    EXPECT_EQ(to_json(Value(Object{})), "{}");
}

TEST(ToJson, SimpleObject) {
    Value obj(Object{});
    obj.set("a", Value(int64_t{1}));
    obj.set("b", Value(int64_t{2}));
    EXPECT_EQ(to_json(obj), R"({"a":1,"b":2})");
}

TEST(ToJson, ObjectKeyOrderPreserved) {
    Value obj(Object{});
    obj.set("z", Value(int64_t{3}));
    obj.set("a", Value(int64_t{1}));
    obj.set("m", Value(int64_t{2}));
    str text = to_json(obj);
    // z must appear before a, a before m.
    auto zpos = text.find("\"z\"");
    auto apos = text.find("\"a\"");
    auto mpos = text.find("\"m\"");
    ASSERT_NE(zpos, str::npos);
    ASSERT_NE(apos, str::npos);
    ASSERT_NE(mpos, str::npos);
    EXPECT_LT(zpos, apos);
    EXPECT_LT(apos, mpos);
}

// ===========================================================================
// Round-trip: parse(to_json(v)) == v
// ===========================================================================

TEST(RoundTrip, Null) {
    Value v;
    auto r = parse(to_json(v));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, v);
}

TEST(RoundTrip, Bool) {
    for (bool b : {true, false}) {
        Value v(b);
        auto r = parse(to_json(v));
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(*r, v);
    }
}

TEST(RoundTrip, Int) {
    for (int64_t i : {int64_t{0}, int64_t{1}, int64_t{-1}, int64_t{9999}}) {
        Value v(i);
        auto r = parse(to_json(v));
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(*r, v);
    }
}

TEST(RoundTrip, Double) {
    for (double d : {0.0, 1.0, -1.0, 3.14, 1e100, 1.23456789}) {
        Value v(d);
        auto r = parse(to_json(v));
        ASSERT_TRUE(r.has_value()) << "d=" << d;
        ASSERT_TRUE(r->as_double().has_value()) << "d=" << d;
        EXPECT_DOUBLE_EQ(*r->as_double(), d) << "d=" << d;
    }
}

TEST(RoundTrip, StringWithSpecialChars) {
    str raw = "tab:\there\nnewline \"quote\" back\\slash";
    Value v(raw);
    auto r = parse(to_json(v));
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->as_string().has_value());
    EXPECT_EQ(*r->as_string(), raw);
}

TEST(RoundTrip, NestedStructure) {
    Value obj(Object{});
    Array arr;
    arr.push_back(Value(int64_t{1}));
    arr.push_back(Value("two"));
    arr.push_back(Value(true));
    obj.set("list", Value(s::move(arr)));
    obj.set("n", Value());

    str json = to_json(obj);
    auto r = parse(json);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, obj);
}

TEST(RoundTrip, KeyOrderPreservedAfterRoundTrip) {
    Value obj(Object{});
    obj.set("c", Value(int64_t{3}));
    obj.set("a", Value(int64_t{1}));
    obj.set("b", Value(int64_t{2}));

    auto r = parse(to_json(obj));
    ASSERT_TRUE(r.has_value());
    ASSERT_NE(r->as_object(), nullptr);
    ASSERT_EQ(r->as_object()->entries.size(), 3u);
    EXPECT_EQ(r->as_object()->entries[0].first, "c");
    EXPECT_EQ(r->as_object()->entries[1].first, "a");
    EXPECT_EQ(r->as_object()->entries[2].first, "b");
}
