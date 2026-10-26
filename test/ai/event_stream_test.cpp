#include <ai/event_stream.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <ai/event_helpers.h>
#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::ai;
using namespace pi::ai::test;
using pi::util::CancelToken;

namespace {
AssistantMessage text_message(str text, StopReason reason = StopReason::stop) {
    AssistantMessage m;
    m.content = {TextContent{s::move(text)}};
    m.stop_reason = reason;
    return m;
}

DoneEvent done_with(str text) {
    return DoneEvent{StopReason::stop, text_message(s::move(text))};
}
}  // namespace

// ===========================================================================
// Ordering and terminal rules
// ===========================================================================

TEST(EventStream, EventsComeOutInPushOrder) {
    EventStream stream;
    stream.push(StartEvent{});
    stream.push(TextStartEvent{0});
    stream.push(TextDeltaEvent{0, "hi"});
    stream.push(TextEndEvent{0, "hi"});
    stream.push(done_with("hi"));
    EXPECT_EQ(kinds(drain(stream)), (vec<str>{"start", "text_start", "text_delta", "text_end", "done"}));
}

TEST(EventStream, PayloadsSurviveTheQueue) {
    EventStream stream;
    stream.push(TextDeltaEvent{3, "abc"});
    stream.push(done_with("abc"));
    auto first = stream.next();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, (AssistantMessageEvent{TextDeltaEvent{3, "abc"}}));
}

TEST(EventStream, NextIsNulloptAfterTerminal) {
    EventStream stream;
    stream.push(done_with("x"));
    ASSERT_TRUE(stream.next().has_value());
    EXPECT_FALSE(stream.next().has_value());
    EXPECT_FALSE(stream.next().has_value()) << "must keep returning nullopt, not block";
}

TEST(EventStream, PushesAfterTerminalAreDropped) {
    EventStream stream;
    stream.push(StartEvent{});
    stream.push(done_with("first"));
    stream.push(TextDeltaEvent{0, "late"});
    stream.push(ErrorEvent{StopReason::error, text_message("second", StopReason::error)});
    stream.push(done_with("third"));
    EXPECT_EQ(kinds(drain(stream)), (vec<str>{"start", "done"}));
    EXPECT_EQ(text_of(stream.result()), "first");
}

TEST(EventStream, ErrorIsTerminalToo) {
    EventStream stream;
    stream.push(ErrorEvent{StopReason::aborted, text_message("partial", StopReason::aborted)});
    stream.push(done_with("ignored"));
    EXPECT_EQ(kinds(drain(stream)), (vec<str>{"error"}));
    EXPECT_EQ(stream.result().stop_reason, StopReason::aborted);
}

// ===========================================================================
// result() / finished()
// ===========================================================================

TEST(EventStream, ResultIsTheDoneMessage) {
    EventStream stream;
    stream.push(StartEvent{});
    stream.push(done_with("answer"));
    EXPECT_EQ(stream.result(), text_message("answer"));
}

TEST(EventStream, ResultIsTheErrorMessage) {
    EventStream stream;
    AssistantMessage m = text_message("part", StopReason::error);
    m.error_message = "boom";
    stream.push(ErrorEvent{StopReason::error, m});
    EXPECT_EQ(stream.result(), m);
}

TEST(EventStream, ResultDoesNotNeedTheQueueDrained) {
    EventStream stream;
    for (int i = 0; i < 10; ++i) stream.push(TextDeltaEvent{0, "x"});
    stream.push(done_with("x"));
    EXPECT_EQ(text_of(stream.result()), "x");
    // and the queue is still intact afterwards
    EXPECT_EQ(drain(stream).size(), 11u);
}

TEST(EventStream, ResultIsRepeatable) {
    EventStream stream;
    stream.push(done_with("x"));
    EXPECT_EQ(stream.result(), stream.result());
    drain(stream);
    EXPECT_EQ(text_of(stream.result()), "x");
}

TEST(EventStream, FinishedOnlyAfterTerminalPush) {
    EventStream stream;
    EXPECT_FALSE(stream.finished());
    stream.push(StartEvent{});
    EXPECT_FALSE(stream.finished());
    stream.push(done_with("x"));
    EXPECT_TRUE(stream.finished()) << "finished() doesn't wait for the consumer";
}

TEST(EventStream, CopiesShareOneQueue) {
    EventStream consumer;
    EventStream producer = consumer;
    producer.push(StartEvent{});
    producer.push(done_with("shared"));
    EXPECT_EQ(kinds(drain(consumer)), (vec<str>{"start", "done"}));
    EXPECT_TRUE(producer.finished());
}

// ===========================================================================
// Threads
// ===========================================================================

TEST(EventStream, ProducerOnAnotherThreadKeepsOrder) {
    EventStream stream;
    constexpr int kCount = 2000;
    s::jthread producer([stream]() mutable {
        stream.push(StartEvent{});
        for (int i = 0; i < kCount; ++i) stream.push(TextDeltaEvent{0, s::to_string(i)});
        stream.push(done_with("end"));
    });
    vec<AssistantMessageEvent> events = drain(stream);
    ASSERT_EQ(events.size(), static_cast<s::size_t>(kCount + 2));
    for (int i = 0; i < kCount; ++i) {
        const auto* d = s::get_if<TextDeltaEvent>(&events[static_cast<s::size_t>(i + 1)]);
        ASSERT_NE(d, nullptr) << "event " << i + 1;
        ASSERT_EQ(d->delta, s::to_string(i));
    }
    EXPECT_EQ(kind_name(events.back()), "done");
}

TEST(EventStream, NextBlocksUntilSomethingIsPushed) {
    EventStream stream;
    s::atomic<bool> pushed = false;
    s::jthread producer([stream, &pushed]() mutable {
        st::sleep_for(sc::milliseconds(50));
        pushed = true;
        stream.push(done_with("late"));
    });
    auto event = stream.next();
    EXPECT_TRUE(pushed) << "next() returned before anything was pushed";
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(kind_name(*event), "done");
}

TEST(EventStream, ResultBlocksUntilTerminal) {
    EventStream stream;
    s::atomic<bool> pushed = false;
    s::jthread producer([stream, &pushed]() mutable {
        stream.push(StartEvent{});
        st::sleep_for(sc::milliseconds(50));
        pushed = true;
        stream.push(done_with("done"));
    });
    AssistantMessage m = stream.result();
    EXPECT_TRUE(pushed) << "result() returned before the terminal event";
    EXPECT_EQ(text_of(m), "done");
}

TEST(EventStream, ConsumerCancellationEndsWithAborted) {
    // The pattern every provider follows: the consumer cancels the token, the
    // producer notices and ends the stream with ErrorEvent{aborted}.
    EventStream stream;
    CancelToken cancel;
    s::jthread producer([stream, cancel]() mutable {
        stream.push(StartEvent{});
        stream.push(TextStartEvent{0});
        str text;
        auto deadline = sc::steady_clock::now() + sc::seconds(5);
        while (not cancel.is_cancelled() and sc::steady_clock::now() < deadline) {
            text += "x";
            stream.push(TextDeltaEvent{0, "x"});
            st::sleep_for(sc::milliseconds(1));
        }
        AssistantMessage m = text_message(text, StopReason::aborted);
        m.error_message = "request aborted";
        stream.push(ErrorEvent{StopReason::aborted, m});
    });

    int deltas = 0;
    while (auto event = stream.next()) {
        if (s::holds_alternative<TextDeltaEvent>(*event) and ++deltas == 3) cancel.cancel();
        if (is_terminal(*event)) {
            ASSERT_TRUE(s::holds_alternative<ErrorEvent>(*event)) << kind_name(*event);
            EXPECT_EQ(s::get<ErrorEvent>(*event).reason, StopReason::aborted);
        }
    }
    EXPECT_GE(deltas, 3);
    EXPECT_EQ(stream.result().stop_reason, StopReason::aborted);
}

TEST(EventStream, AdoptedWorkerDeliversEvents) {
    EventStream stream;
    stream.adopt_worker(s::jthread([producer = stream]() mutable {
        st::sleep_for(sc::milliseconds(20));
        producer.push(StartEvent{});
        producer.push(done_with("bg"));
    }));
    EXPECT_EQ(kinds(drain(stream)), (vec<str>{"start", "done"}));
}

TEST(EventStream, WorkerHoldingTheLastCopyIsSafe) {
    // Drop every consumer handle while the worker still runs: the State dies on
    // the worker thread and must not try to join itself.
    s::atomic<bool> worker_done = false;
    {
        EventStream stream;
        stream.adopt_worker(s::jthread([producer = stream, &worker_done]() mutable {
            st::sleep_for(sc::milliseconds(20));
            producer.push(done_with("nobody listens"));
            worker_done = true;
        }));
    }
    auto deadline = sc::steady_clock::now() + sc::seconds(5);
    while (not worker_done and sc::steady_clock::now() < deadline) st::sleep_for(sc::milliseconds(5));
    EXPECT_TRUE(worker_done);
    st::sleep_for(sc::milliseconds(20));  // let the worker unwind and destroy the State
}
