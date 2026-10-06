#include <util/cancel.h>

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

using namespace pi::util;

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST(CancelToken, InitiallyNotCancelled) {
    CancelToken tok;
    EXPECT_FALSE(tok.is_cancelled());
}

// ---------------------------------------------------------------------------
// cancel()
// ---------------------------------------------------------------------------

TEST(CancelToken, CancelSetsCancelled) {
    CancelToken tok;
    tok.cancel();
    EXPECT_TRUE(tok.is_cancelled());
}

TEST(CancelToken, CancelIsIdempotent) {
    CancelToken tok;
    tok.cancel();
    tok.cancel();  // must not throw or crash
    EXPECT_TRUE(tok.is_cancelled());
}

// ---------------------------------------------------------------------------
// on_cancel callback
// ---------------------------------------------------------------------------

TEST(CancelToken, OnCancelFiredAfterCancel) {
    CancelToken tok;
    bool fired = false;
    auto handle = tok.on_cancel([&] { fired = true; });
    EXPECT_FALSE(fired);
    tok.cancel();
    EXPECT_TRUE(fired);
}

TEST(CancelToken, OnCancelFiredImmediatelyIfAlreadyCancelled) {
    CancelToken tok;
    tok.cancel();
    bool fired = false;
    // Registering AFTER cancel() should fire immediately.
    auto handle = tok.on_cancel([&] { fired = true; });
    EXPECT_TRUE(fired);
}

TEST(CancelToken, OnCancelNotFiredAfterHandleDestroyed) {
    CancelToken tok;
    bool fired = false;
    {
        auto handle = tok.on_cancel([&] { fired = true; });
        // handle goes out of scope here, unregistering the callback.
    }
    tok.cancel();
    EXPECT_FALSE(fired);
}

TEST(CancelToken, MultipleCallbacks) {
    CancelToken tok;
    int count = 0;
    auto h1 = tok.on_cancel([&] { ++count; });
    auto h2 = tok.on_cancel([&] { ++count; });
    tok.cancel();
    EXPECT_EQ(count, 2);
}

TEST(CancelToken, OnCancelOnlyFirstOfTwoCallbacks) {
    CancelToken tok;
    int count = 0;
    auto h1 = tok.on_cancel([&] { ++count; });
    {
        auto h2 = tok.on_cancel([&] { ++count; });
        // h2 destroyed here.
    }
    tok.cancel();
    EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// stop_token passthrough
// ---------------------------------------------------------------------------

TEST(CancelToken, StopTokenReflectsCancellation) {
    CancelToken tok;
    std::stop_token st = tok.stop_token();
    EXPECT_FALSE(st.stop_requested());
    tok.cancel();
    EXPECT_TRUE(st.stop_requested());
}

// ---------------------------------------------------------------------------
// Cross-thread cancellation
// ---------------------------------------------------------------------------

TEST(CancelToken, CrossThreadCancel) {
    CancelToken tok;
    std::atomic<bool> observed{false};

    std::jthread worker([&](std::stop_token) {
        // Spin until cancellation is observed.
        while (not tok.is_cancelled()) {
            std::this_thread::yield();
        }
        observed = true;
    });

    // Cancel from the main thread after a brief yield.
    std::this_thread::yield();
    tok.cancel();
    worker.join();

    EXPECT_TRUE(observed.load());
}
