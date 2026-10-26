#include <util/function_ref.h>

#include <string>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

#include <util/aliases.h>

using namespace pi::util;

namespace {
int twice(int x) { return 2 * x; }

int call(FunctionRef<int(int)> fn, int x) { return fn(x); }
}  // namespace

static_assert(sizeof(FunctionRef<void()>) == 2 * sizeof(void*));
static_assert(s::is_trivially_copyable_v<FunctionRef<void(strv)>>);

TEST(FunctionRef, CallsCapturingLambda) {
    str got;
    auto append = [&](strv c) { got += c; };
    FunctionRef<void(strv)> fn = append;
    fn("ab");
    fn("cd");
    EXPECT_EQ(got, "abcd");
}

TEST(FunctionRef, BindsTemporaryLambdaAsArgument) {
    int offset = 3;
    EXPECT_EQ(call([&](int x) { return x + offset; }, 4), 7);
}

TEST(FunctionRef, CallsFreeFunction) {
    EXPECT_EQ(call(twice, 5), 10);
    EXPECT_EQ(call(&twice, 6), 12);
}

TEST(FunctionRef, RefersToCallableNotCopy) {
    // Mutations of the referenced object are visible through the ref.
    struct Counter {
        int n = 0;
        void operator()() { ++n; }
    } counter;
    FunctionRef<void()> fn = counter;
    fn();
    fn();
    EXPECT_EQ(counter.n, 2);
}

TEST(FunctionRef, CopiesReferToSameCallable) {
    int n = 0;
    auto bump = [&] { ++n; };
    FunctionRef<void()> a = bump;
    FunctionRef<void()> b = a;  // copy, not a ref-to-ref
    b();
    EXPECT_EQ(n, 1);
}

TEST(FunctionRef, CallsConstCallable) {
    const auto add = [](int a, int b) { return a + b; };
    FunctionRef<int(int, int)> fn = add;
    EXPECT_EQ(fn(2, 3), 5);
}

TEST(FunctionRef, ConvertsReturnType) {
    auto small = [] { return 'A'; };
    FunctionRef<int()> fn = small;
    EXPECT_EQ(fn(), 65);
}
