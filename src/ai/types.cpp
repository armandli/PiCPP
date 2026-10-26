#include <ai/types.h>

namespace pi::ai {

int64_t now_ms() {
  // TODO(M3): sc::duration_cast<sc::milliseconds>(sc::system_clock::now().time_since_epoch()).count().
  return 0;
}

strv to_string(StopReason reason) {
  // TODO(M3): one case per enumerator; the strings are Pi's ("toolUse" is camelCase).
  (void)reason;
  return "";
}

opt<StopReason> parse_stop_reason(strv text) {
  // TODO(M3): exact inverse of to_string(); anything else → nullopt.
  (void)text;
  return s::nullopt;
}

OpenAiCompat ollama_compat() {
  // TODO(M3): supports_developer_role = false, supports_reasoning_effort = false,
  //   supports_usage_in_streaming = true, max_tokens_field = "max_tokens".
  return {};
}

str text_of(const AssistantMessage& message) {
  // TODO(M3): concatenate TextContent::text of every text block, in order.  No
  //   separator: a provider that emits two text blocks meant them to be adjacent.
  (void)message;
  return {};
}

// ---------------------------------------------------------------------------
// encode
//
// Shapes (see types.h for each struct's JSON comment):
//   TextContent      {"type":"text","text":...}
//   ThinkingContent  {"type":"thinking","thinking":...}
//   ImageContent     {"type":"image","data":...,"mimeType":...}
//   ToolCall         {"type":"toolCall","id":...,"name":...,"arguments":{...}}
//   Usage            {"input","output","cacheRead","cacheWrite","totalTokens",
//                     "cost":{"input","output","cacheRead","cacheWrite","total"}}
//   Messages         "role" first, then the fields in declaration order
//   StopReason       to_string(reason)
// Emit keys in a fixed order (json::Object preserves insertion order), so
// encode(x) is byte-stable and tests can compare serialized text.
// ---------------------------------------------------------------------------

json::Value encode(const Usage& usage) {
  // TODO(M3)
  (void)usage;
  return {};
}

json::Value encode(const UserContent& content) {
  // TODO(M3): s::visit over TextContent / ImageContent.
  (void)content;
  return {};
}

json::Value encode(const AssistantContent& content) {
  // TODO(M3): s::visit over TextContent / ThinkingContent / ToolCall.
  (void)content;
  return {};
}

json::Value encode(const Message& message) {
  // TODO(M3): s::visit; "role" is "user" / "assistant" / "toolResult".
  //   AssistantMessage: omit "errorMessage" when empty.
  (void)message;
  return {};
}

json::Value encode(const Model& model) {
  // TODO(M3): {"id","name","api","provider","baseUrl","reasoning","contextWindow",
  //   "maxTokens","compat":{"supportsDeveloperRole","supportsReasoningEffort",
  //   "supportsUsageInStreaming","maxTokensField"}}.
  (void)model;
  return {};
}

json::Value encode(const Tool& tool) {
  // TODO(M3): {"name","description","parameters"}.
  (void)tool;
  return {};
}

json::Value encode(const Context& context) {
  // TODO(M3): {"systemPrompt"?, "messages":[...], "tools"?}; omit the optional
  //   keys when empty.
  (void)context;
  return {};
}

// ---------------------------------------------------------------------------
// decode
//
// Rules:
//  - the input must be an object; dispatch on "type" (content) or "role" (message)
//  - required string fields: text/thinking/data/mimeType/id/name, role, toolCallId
//  - missing optional fields keep the struct default (e.g. no "usage" → Usage{})
//  - a wrong JSON type for a present field is an error ("usage.input: expected integer")
//  - UserMessage / ToolResultMessage "content" may be a string → one TextContent
//  - stopReason must parse with parse_stop_reason
//  - integers may arrive as doubles from other writers (e.g. 12.0); accept when integral
// A small helper set (get_string(obj, key, path), get_int(...), ...) keeps this short.
// ---------------------------------------------------------------------------

s::expected<Usage, DecodeError> decode_usage(const json::Value& value) {
  // TODO(M3)
  (void)value;
  return s::unexpected(DecodeError("not implemented"));
}

s::expected<UserContent, DecodeError> decode_user_content(const json::Value& value) {
  // TODO(M3): "text" or "image"; any other type is an error.
  (void)value;
  return s::unexpected(DecodeError("not implemented"));
}

s::expected<AssistantContent, DecodeError> decode_assistant_content(const json::Value& value) {
  // TODO(M3): "text", "thinking" or "toolCall"; a missing "arguments" is {}.
  (void)value;
  return s::unexpected(DecodeError("not implemented"));
}

s::expected<Message, DecodeError> decode_message(const json::Value& value) {
  // TODO(M3): dispatch on "role".
  (void)value;
  return s::unexpected(DecodeError("not implemented"));
}

s::expected<Model, DecodeError> decode_model(const json::Value& value) {
  // TODO(M3): "id" required; everything else optional with struct defaults.
  (void)value;
  return s::unexpected(DecodeError("not implemented"));
}

s::expected<Tool, DecodeError> decode_tool(const json::Value& value) {
  // TODO(M3): "name" required; "description" optional; "parameters" defaults to {}.
  (void)value;
  return s::unexpected(DecodeError("not implemented"));
}

s::expected<Context, DecodeError> decode_context(const json::Value& value) {
  // TODO(M3): decode each element of "messages" / "tools"; the first failure is
  //   returned with its index in the path, e.g. "messages[2]: unknown role \"bot\"".
  (void)value;
  return s::unexpected(DecodeError("not implemented"));
}

}  // namespace pi::ai
