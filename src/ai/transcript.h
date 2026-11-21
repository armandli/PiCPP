#ifndef AI_TRANSCRIPT_H
#define AI_TRANSCRIPT_H

#include <string>
#include <vector>

#include <ai/types.h>
#include <util/aliases.h>

// The system prompt and tool set as transcript state (Pi v1.5.0:
// ai/src/utils/transcript.ts and getSystemMessageText /
// renderSystemMessageUpdate in ai/src/utils/text.ts).
//
// A Context's system_prompt and tools are shorthand: normalize_context folds
// them into a leading SystemMessage.  Later SystemMessages add instructions,
// patch sections by name and add / remove tools.  Replaying every system message
// in order gives the current prompt and tools.
//
// TODO(M3): every function below is a stub (see transcript.cpp); the contracts
// here are what test/ai/transcript_test.cpp checks.

namespace pi::ai {

// What providers receive (Pi: TranscriptContext).  A distinct type from Context
// so a raw Context can't reach request building by accident: only
// normalize_context, collapse_system_messages and resolve_transcript produce one.
struct TranscriptContext {
  vec<Message> messages;

  bool operator==(const TranscriptContext&) const = default;
};

// The leading system message for a prompt and tool set:
// SystemMessage{content = system_prompt, tools_added = tools, timestamp = 0}.
// nullopt when both are empty, so an empty transcript stays empty.
opt<SystemMessage> create_initial_system_message(strv system_prompt, const vec<Tool>& tools);

// context.messages, preceded by create_initial_system_message(
// context.system_prompt, context.tools) when that isn't nullopt.
TranscriptContext normalize_context(const Context& context);

// messages[0] when it is a SystemMessage, else nullptr.
const SystemMessage* get_initial_system_message(const vec<Message>& messages);

// The tools after applying every system message in order: first remove each of
// its tools_removed (by name), then add or replace each of its tools_added (by
// name).  Order follows Pi's Map: a tool keeps the position where it was first
// added; one removed and later added again moves to the end.
vec<Tool> get_current_tools(const vec<Message>& messages);

// Every system message replayed into one (Pi: getCurrentSystemMessage):
//  - content: the non-empty contents, joined with "\n\n"
//  - sections: patched by name in order (nullopt removes; same ordering rule
//    as get_current_tools)
//  - tools_added: get_current_tools(messages); tools_removed: empty
//  - timestamp: the first system message's
// nullopt when there is no system message at all.
opt<SystemMessage> get_current_system_message(const vec<Message>& messages);

// A system message as one prompt text (Pi: getSystemMessageText): content, then
// each section's text in order (removed sections skipped), dropping empty parts,
// joined with "\n\n".
str system_message_text(const SystemMessage& message);

// system_message_text(get_current_system_message(messages)), or "" when there
// is none.
str get_current_system_prompt(const vec<Message>& messages);

// A later system message, for APIs that accept system messages mid-conversation
// (Pi: renderSystemMessageUpdate).  Parts joined with "\n\n": the content if
// non-empty, then per section
//   removed → `Removed system prompt section "<name>".`
//   else    → `Updated system prompt section "<name>":\n\n<text>`
str render_system_message_update(const SystemMessage& message);

// For APIs without mid-conversation system messages: get_current_system_message
// leads (when there is one), followed by every non-system message in order.
TranscriptContext collapse_system_messages(const TranscriptContext& context);

// `context` unchanged when supports_mid_convo_system_messages, else
// collapse_system_messages(context).
TranscriptContext resolve_transcript(const TranscriptContext& context, bool supports_mid_convo_system_messages);

}  // namespace pi::ai

#endif  // AI_TRANSCRIPT_H
