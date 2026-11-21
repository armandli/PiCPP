#include <ai/transcript.h>

namespace pi::ai {

opt<SystemMessage> create_initial_system_message(strv system_prompt, const vec<Tool>& tools) {
  // TODO(M3): see header.
  (void)system_prompt; (void)tools;
  return s::nullopt;
}

TranscriptContext normalize_context(const Context& context) {
  // TODO(M3): see header.  Until then the prompt and tools are dropped.
  return TranscriptContext{context.messages};
}

const SystemMessage* get_initial_system_message(const vec<Message>& messages) {
  // TODO(M3): see header.
  (void)messages;
  return nullptr;
}

vec<Tool> get_current_tools(const vec<Message>& messages) {
  // TODO(M3): see header.
  (void)messages;
  return {};
}

opt<SystemMessage> get_current_system_message(const vec<Message>& messages) {
  // TODO(M3): see header.
  (void)messages;
  return s::nullopt;
}

str system_message_text(const SystemMessage& message) {
  // TODO(M3): see header.
  (void)message;
  return {};
}

str get_current_system_prompt(const vec<Message>& messages) {
  // TODO(M3): see header.
  (void)messages;
  return {};
}

str render_system_message_update(const SystemMessage& message) {
  // TODO(M3): see header.
  (void)message;
  return {};
}

TranscriptContext collapse_system_messages(const TranscriptContext& context) {
  // TODO(M3): see header.
  return context;
}

TranscriptContext resolve_transcript(const TranscriptContext& context, bool supports_mid_convo_system_messages) {
  // TODO(M3): see header.
  (void)supports_mid_convo_system_messages;
  return context;
}

}  // namespace pi::ai
