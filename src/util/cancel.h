#ifndef UTIL_CANCEL_H
#define UTIL_CANCEL_H

#include <functional>
#include <stop_token>

#include <util/aliases.h>

namespace pi::util {

// Type alias for the handle returned by on_cancel().
// Keep the returned handle alive for as long as the callback should be active;
// destroying it unregisters the callback.
using CancelCallback = s::stop_callback<s::function<void()>>;

// Cancellation handle passed from an agent run to its tools and HTTP requests.
// Wraps std::stop_source / std::stop_token (C++20).  One instance per agent turn.
// Analogous to Pi's AbortSignal / AbortController pair.
struct CancelToken {
    // Signal cancellation; idempotent.
    void cancel() {
        mSource.request_stop();
    }

    // True iff cancel() has been called.
    bool is_cancelled() const {
        return mSource.stop_requested();
    }

    // Register a callback invoked when cancel() is called.
    // If cancel() has already been called, the callback fires immediately.
    // The callback is unregistered when the returned handle is destroyed.
    [[nodiscard]] CancelCallback on_cancel(s::function<void()> cb) {
        return CancelCallback(mSource.get_token(), s::move(cb));
    }

    // Expose the underlying stop_token for passing to std:: utilities directly.
    s::stop_token stop_token() const {
        return mSource.get_token();
    }

private:
    s::stop_source mSource;
};

}  // namespace pi::util

#endif  // UTIL_CANCEL_H
