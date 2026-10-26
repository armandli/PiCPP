#ifndef TEST_AI_EVENT_HELPERS_H
#define TEST_AI_EVENT_HELPERS_H

// Shared helpers for tests that inspect AssistantMessageEvent sequences.  Kept
// independent of ai::event_type() so a bug there doesn't mask other failures.

#include <array>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <ai/event_stream.h>
#include <ai/events.h>
#include <util/aliases.h>

namespace pi::ai::test {

inline strv kind_name(const AssistantMessageEvent& event) {
    static constexpr s::array<strv, s::variant_size_v<AssistantMessageEvent>> kNames = {
        "start",
        "text_start", "text_delta", "text_end",
        "thinking_start", "thinking_delta", "thinking_end",
        "toolcall_start", "toolcall_delta", "toolcall_end",
        "done", "error",
    };
    return kNames[event.index()];
}

// Pops events until next() returns nullopt, at most `limit` (guards a broken
// stream that never terminates *without* blocking; a blocking next() still hangs).
inline vec<AssistantMessageEvent> drain(EventStream& stream, s::size_t limit = 100000) {
    vec<AssistantMessageEvent> out;
    while (out.size() < limit) {
        opt<AssistantMessageEvent> event = stream.next();
        if (not event) break;
        out.push_back(s::move(*event));
    }
    return out;
}

inline vec<str> kinds(const vec<AssistantMessageEvent>& events) {
    vec<str> out;
    for (const AssistantMessageEvent& e : events) out.emplace_back(kind_name(e));
    return out;
}

// Like kinds(), but runs of the same kind collapse to one entry, so
// "text_delta x 17" compares as a single "text_delta".
inline vec<str> collapsed_kinds(const vec<AssistantMessageEvent>& events) {
    vec<str> out;
    for (const AssistantMessageEvent& e : events) {
        strv name = kind_name(e);
        if (out.empty() or out.back() != name) out.emplace_back(name);
    }
    return out;
}

// Concatenation of every text_delta (or thinking_delta) in `events`.
inline str joined_text_deltas(const vec<AssistantMessageEvent>& events) {
    str out;
    for (const AssistantMessageEvent& e : events)
        if (const auto* d = s::get_if<TextDeltaEvent>(&e)) out += d->delta;
    return out;
}
inline str joined_thinking_deltas(const vec<AssistantMessageEvent>& events) {
    str out;
    for (const AssistantMessageEvent& e : events)
        if (const auto* d = s::get_if<ThinkingDeltaEvent>(&e)) out += d->delta;
    return out;
}

// The terminal event's message, or a default message if the last event isn't terminal.
inline AssistantMessage terminal_message(const vec<AssistantMessageEvent>& events) {
    if (events.empty()) return {};
    if (const auto* d = s::get_if<DoneEvent>(&events.back())) return d->message;
    if (const auto* e = s::get_if<ErrorEvent>(&events.back())) return e->message;
    return {};
}

// Contents of test/<relative_path>.
inline str read_fixture(strv relative_path) {
    s::ifstream in(str(PICPP_TEST_DIR) + "/" + str(relative_path), s::ios::binary);
    s::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

}  // namespace pi::ai::test

#endif  // TEST_AI_EVENT_HELPERS_H
