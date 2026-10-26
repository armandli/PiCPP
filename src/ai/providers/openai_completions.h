#ifndef AI_PROVIDERS_OPENAI_COMPLETIONS_H
#define AI_PROVIDERS_OPENAI_COMPLETIONS_H

#include <memory>
#include <string>
#include <vector>

#include <ai/provider.h>
#include <ai/stream_builder.h>
#include <ai/types.h>
#include <http/transport.h>
#include <json/value.h>
#include <util/aliases.h>

// OpenAI chat-completions over SSE, as served by Ollama's /v1 endpoint
// (Pi: ai/src/api/openai-completions.ts — buildParams, convertMessages, the
// stream loop, mapStopReason; PiG: ai/openai.go).  Text + thinking only in M3;
// tool calls and tool results arrive in M5.

namespace pi::ai::providers {

inline constexpr strv kOpenAiCompletionsApi = "openai-completions";

// A Model for an Ollama-served model id: api "openai-completions", provider
// "ollama", name = id, base_url as given (trailing '/' removed), reasoning true
// (Ollama emits delta.reasoning for thinking models and nothing otherwise),
// context_window 32768, max_tokens 8192, compat = ollama_compat().
Model make_ollama_model(str id, str base_url = "http://localhost:11434/v1");

// The "messages" array (Pi: convertMessages):
//  - system prompt (if non-empty) first: {"role":"developer"} when
//    compat.supports_developer_role and model.reasoning, else {"role":"system"}
//  - UserMessage: content is a string when every block is text (joined with
//    "\n"), else an array of {"type":"text","text"} /
//    {"type":"image_url","image_url":{"url":"data:<mime>;base64,<data>"}}
//  - AssistantMessage: {"role":"assistant","content":text_of(message)}; thinking
//    is dropped; skipped entirely if it has no text (tool calls come in M5)
//  - ToolResultMessage: skipped for now (TODO(M5): {"role":"tool",...})
json::Value convert_messages(const Model& model, const Context& context);

// The full request body (Pi: buildParams), keys in this order:
//   "model", "messages", "stream": true,
//   "stream_options": {"include_usage": true}   (if compat.supports_usage_in_streaming)
//   <compat.max_tokens_field>: options.max_tokens, else model.max_tokens (omitted if neither > 0)
//   "temperature"                              (if options.temperature)
json::Value build_request(const Model& model, const Context& context, const StreamOptions& options);

// finish_reason → StopReason (Pi: mapStopReason): "stop" → stop, "length" →
// length, "tool_calls" / "function_call" → toolUse, anything else → error.
StopReason map_stop_reason(strv finish_reason);

// The chunk's "usage" object → Usage:
//   cache_read   = prompt_tokens_details.cached_tokens (0 if absent)
//   input        = prompt_tokens - cache_read
//   output       = completion_tokens
//   total_tokens = total_tokens if present, else input + output + cache_read
Usage parse_usage(const json::Value& usage);

// Turns SSE `data:` payloads into StreamBuilder calls.  One per request.
//
//  - "[DONE]" → finish with the remembered finish_reason (none → stop)
//  - other payloads are JSON chunks; unparsable JSON →
//    fail(error, "invalid chunk: <parse error>")
//  - {"error":{"message":...}} (servers report mid-stream errors this way) →
//    fail(error, message)
//  - choices[0].delta: "reasoning_content" / "reasoning" / "reasoning_text"
//    (first present, non-empty) → thinking_delta; then "content" → text_delta
//  - choices[0].finish_reason (non-null) is *remembered*, not acted on: the
//    usage chunk (with empty "choices") arrives after it
//  - "usage" (non-null) → set_usage(parse_usage(...))
//  - the first chunk triggers builder.start() if not yet started
// on_end() is called when the HTTP body ends: if nothing finished the stream
// yet, finish with the remembered finish_reason, or, without one,
// fail(error, "stream ended without finish_reason").
struct ChunkDecoder {
  explicit ChunkDecoder(StreamBuilder& builder);

  void on_data(strv payload);
  void on_end();

private:
  StreamBuilder& mBuilder;
  // TODO(M3): state, e.g.  opt<str> mFinishReason;
};

struct OpenAiCompletionsOptions {
  str               provider_id = "ollama";
  http::HttpHeaders extra_headers;  // sent with every request
};

// stream():
//  - POST <model.base_url>/chat/completions with build_request(...) serialized
//    by json::to_json, headers: Content-Type: application/json,
//    Accept: text/event-stream, Authorization: Bearer <api_key> (if set),
//    then extra_headers
//  - runs on a worker s::jthread adopted by the returned EventStream; the
//    worker owns a copy of the transport sptr, the model, the request and the
//    CancelToken, so the provider may be destroyed mid-stream
//  - body bytes → http::SseDecoder → ChunkDecoder::on_data
//  - transport errors: Cancelled → fail(aborted, "request aborted");
//    Status → fail(error, "HTTP <status>: <body>"); others → fail(error, message);
//    success → ChunkDecoder::on_end()
//  - message metadata: api kOpenAiCompletionsApi, provider = provider_id, model = model.id
struct OpenAiCompletionsProvider : Provider {
  explicit OpenAiCompletionsProvider(sptr<http::HttpTransport> transport, OpenAiCompletionsOptions options = {});

  strv id() const override;
  // Static until M7 queries GET /v1/models: empty.
  vec<Model> models() override;
  EventStream stream(
      const Model& model,
      const Context& context,
      const StreamOptions& options,
      util::CancelToken cancel) override;

private:
  sptr<http::HttpTransport> mTransport;
  OpenAiCompletionsOptions  mOptions;
};

}  // namespace pi::ai::providers

#endif  // AI_PROVIDERS_OPENAI_COMPLETIONS_H
