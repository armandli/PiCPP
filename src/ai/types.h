#ifndef AI_TYPES_H
#define AI_TYPES_H

#include <cstdint>
#include <expected>
#include <string>
#include <variant>
#include <vector>

#include <json/value.h>
#include <util/aliases.h>

// Provider-neutral LLM data model (Pi: ai/src/types.ts).  Plain data: every
// struct is copyable and comparable so tests can assert whole messages.
//
// JSON uses Pi's field names (camelCase, `type`/`role` tags) so sessions written
// by PiCPP (M10) stay readable next to Pi's.

namespace pi::ai {

// ---------------------------------------------------------------------------
// Content blocks
// ---------------------------------------------------------------------------

// {"type":"text","text":"..."}
struct TextContent {
  str text;

  bool operator==(const TextContent&) const = default;
};

// {"type":"thinking","thinking":"..."}  Model reasoning; never sent back to
// the OpenAI-compatible API as-is.
struct ThinkingContent {
  str thinking;

  bool operator==(const ThinkingContent&) const = default;
};

// {"type":"image","data":"<base64>","mimeType":"image/png"}
struct ImageContent {
  str data;       // base64, no "data:" prefix
  str mime_type;  // JSON: "mimeType"

  bool operator==(const ImageContent&) const = default;
};

// {"type":"toolCall","id":"call_1","name":"read","arguments":{...}}
// Produced by the model; executed by the agent loop in M5.
struct ToolCall {
  str         id;
  str         name;
  json::Value arguments = json::object_value();

  bool operator==(const ToolCall&) const = default;
};

using UserContent      = s::variant<TextContent, ImageContent>;
using AssistantContent = s::variant<TextContent, ThinkingContent, ToolCall>;

// ---------------------------------------------------------------------------
// Usage / stop reason
// ---------------------------------------------------------------------------

// Dollar cost per category; all zero until M15 adds pricing.
struct Cost {
  double input       = 0.0;
  double output      = 0.0;
  double cache_read  = 0.0;  // JSON: "cacheRead"
  double cache_write = 0.0;  // JSON: "cacheWrite"
  double total       = 0.0;

  bool operator==(const Cost&) const = default;
};

// Token counts for one assistant message.
struct Usage {
  int64_t input        = 0;  // uncached prompt tokens
  int64_t output       = 0;
  int64_t cache_read   = 0;  // JSON: "cacheRead"
  int64_t cache_write  = 0;  // JSON: "cacheWrite"
  int64_t total_tokens = 0;  // JSON: "totalTokens"
  Cost    cost;

  bool operator==(const Usage&) const = default;
};

// Why the model stopped.  Names (and JSON strings) match Pi exactly.
enum class StopReason : uint8_t {
  stop,     // natural end of turn
  length,   // hit max_tokens
  toolUse,  // wants tool results (M5)
  error,    // provider / transport failure
  aborted,  // cancelled through the CancelToken
};

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------

// {"role":"user","content":[...],"timestamp":<ms>}
// Pi also allows `content` to be a plain string; decode accepts both and
// stores a string as one TextContent.  Encode always writes the array form.
struct UserMessage {
  vec<UserContent> content;
  int64_t          timestamp = 0;  // ms since Unix epoch

  bool operator==(const UserMessage&) const = default;
};

// {"role":"assistant","content":[...],"api":"...","provider":"...","model":"...",
//  "usage":{...},"stopReason":"stop","errorMessage":"...","timestamp":<ms>}
// `errorMessage` is omitted from JSON when empty.
struct AssistantMessage {
  vec<AssistantContent> content;
  str                   api;       // e.g. "openai-completions"
  str                   provider;  // e.g. "ollama"
  str                   model;     // model id
  Usage                 usage;
  StopReason            stop_reason = StopReason::stop;  // JSON: "stopReason"
  str                   error_message;                   // JSON: "errorMessage"
  int64_t               timestamp = 0;

  bool operator==(const AssistantMessage&) const = default;
};

// {"role":"toolResult","toolCallId":"...","toolName":"...","content":[...],
//  "isError":false,"timestamp":<ms>}
struct ToolResultMessage {
  str              tool_call_id;  // JSON: "toolCallId"
  str              tool_name;     // JSON: "toolName"
  vec<UserContent> content;
  bool             is_error = false;  // JSON: "isError"
  int64_t          timestamp = 0;

  bool operator==(const ToolResultMessage&) const = default;
};

using Message = s::variant<UserMessage, AssistantMessage, ToolResultMessage>;

// ---------------------------------------------------------------------------
// Model / tools / context
// ---------------------------------------------------------------------------

// Quirks of one OpenAI-compatible server (Pi: `compat` / detectCompat).
// Defaults describe api.openai.com; ollama_compat() returns Ollama's.
struct OpenAiCompat {
  bool supports_developer_role     = true;   // else the system prompt uses role "system"
  bool supports_reasoning_effort   = true;   // else never send "reasoning_effort"
  bool supports_usage_in_streaming = true;   // send stream_options.include_usage
  str  max_tokens_field = "max_completion_tokens";  // Ollama: "max_tokens"

  bool operator==(const OpenAiCompat&) const = default;
};

struct Model {
  str          id;        // what the API expects, e.g. "qwen3.8:27b-mlx"
  str          name;      // human-readable
  str          api;       // wire protocol, e.g. "openai-completions"
  str          provider;  // e.g. "ollama"
  str          base_url;  // e.g. "http://localhost:11434/v1" (JSON: "baseUrl")
  bool         reasoning = false;        // emits thinking
  int64_t      context_window = 0;       // JSON: "contextWindow"
  int64_t      max_tokens = 0;           // default output cap (JSON: "maxTokens")
  OpenAiCompat compat;

  bool operator==(const Model&) const = default;
};

// A tool the model may call (fully used from M5).  `parameters` is a JSON schema.
struct Tool {
  str         name;
  str         description;
  json::Value parameters = json::object_value();

  bool operator==(const Tool&) const = default;
};

// Everything sent to the model for one request.
// JSON: {"systemPrompt":"...","messages":[...],"tools":[...]}; systemPrompt and
// tools are omitted when empty.
struct Context {
  str          system_prompt;
  vec<Message> messages;
  vec<Tool>    tools;

  bool operator==(const Context&) const = default;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Milliseconds since the Unix epoch, for message timestamps.
int64_t now_ms();

// "stop", "length", "toolUse", "error", "aborted".
strv to_string(StopReason reason);
// Inverse of to_string; nullopt for anything else (case-sensitive).
opt<StopReason> parse_stop_reason(strv text);

// The compat flags Pi applies for Ollama's /v1 endpoint: no developer role, no
// reasoning_effort, usage in streaming supported, "max_tokens".
OpenAiCompat ollama_compat();

// All text blocks of `message` concatenated, in order (thinking and tool calls
// skipped).  What print mode shows as "the answer".
str text_of(const AssistantMessage& message);

// ---------------------------------------------------------------------------
// JSON (de)serialization.  encode() never fails; decode_*() return a message
// naming the offending field on malformed input, e.g. "message: unknown role \"bot\"".
// Unknown extra fields are ignored (forward compatibility with Pi sessions).
// ---------------------------------------------------------------------------

using DecodeError = str;

json::Value encode(const Usage& usage);
json::Value encode(const UserContent& content);
json::Value encode(const AssistantContent& content);
json::Value encode(const Message& message);
json::Value encode(const Model& model);
json::Value encode(const Tool& tool);
json::Value encode(const Context& context);

s::expected<Usage, DecodeError>            decode_usage(const json::Value& value);
s::expected<UserContent, DecodeError>      decode_user_content(const json::Value& value);
s::expected<AssistantContent, DecodeError> decode_assistant_content(const json::Value& value);
s::expected<Message, DecodeError>          decode_message(const json::Value& value);
s::expected<Model, DecodeError>            decode_model(const json::Value& value);
s::expected<Tool, DecodeError>             decode_tool(const json::Value& value);
s::expected<Context, DecodeError>          decode_context(const json::Value& value);

}  // namespace pi::ai

#endif  // AI_TYPES_H
