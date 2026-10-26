#include <ai/providers/openai_completions.h>

#include <http/sse.h>
#include <json/parse.h>
#include <json/write.h>

namespace pi::ai::providers {

Model make_ollama_model(str id, str base_url) {
  // TODO(M3): see header.
  (void)id; (void)base_url;
  return {};
}

json::Value convert_messages(const Model& model, const Context& context) {
  // TODO(M3): see header.  s::visit over Context::messages.
  (void)model; (void)context;
  return json::array_value();
}

json::Value build_request(const Model& model, const Context& context, const StreamOptions& options) {
  // TODO(M3): see header for keys and their order.
  (void)model; (void)context; (void)options;
  return json::object_value();
}

StopReason map_stop_reason(strv finish_reason) {
  // TODO(M3): see header.
  (void)finish_reason;
  return StopReason::error;
}

Usage parse_usage(const json::Value& usage) {
  // TODO(M3): see header.  Missing / non-integer fields count as 0.
  (void)usage;
  return {};
}

ChunkDecoder::ChunkDecoder(StreamBuilder& builder)
  : mBuilder(builder) {}

void ChunkDecoder::on_data(strv payload) {
  // TODO(M3): see header.  Ignore everything once mBuilder.finished().
  (void)payload;
}

void ChunkDecoder::on_end() {
  // TODO(M3): see header.
  (void)mBuilder;
}

OpenAiCompletionsProvider::OpenAiCompletionsProvider(sptr<http::HttpTransport> transport, OpenAiCompletionsOptions options)
  : mTransport(s::move(transport))
  , mOptions(s::move(options)) {}

strv OpenAiCompletionsProvider::id() const {
  return mOptions.provider_id;
}

vec<Model> OpenAiCompletionsProvider::models() {
  return {};
}

EventStream OpenAiCompletionsProvider::stream(
    const Model& model,
    const Context& context,
    const StreamOptions& options,
    util::CancelToken cancel
){
  // TODO(M3): see header.  Suggested shape:
  //   EventStream stream;
  //   str url = model.base_url + "/chat/completions";
  //   str body = json::to_json(build_request(model, context, options));
  //   http::HttpHeaders headers = ...;
  //   stream.adopt_worker(s::jthread([stream, transport = mTransport, ...]() mutable {
  //     // The worker's own copy keeps the stream alive until the request ends,
  //     // so the caller stops a request through `cancel`, not by dropping it.
  //     StreamBuilder builder(stream, AssistantMessage{...});
  //     builder.start();
  //     ChunkDecoder chunks(builder);
  //     http::SseDecoder sse;
  //     auto status = transport->post_stream(url, headers, body, [&](strv bytes){
  //       for (const http::SseEvent& event : sse.feed(bytes)) chunks.on_data(event.data);
  //     }, cancel);
  //     ... map errors / on_end() as described in the header
  //   }));
  //   return stream;
  (void)model; (void)context; (void)options; (void)cancel;
  return EventStream{};
}

}  // namespace pi::ai::providers
