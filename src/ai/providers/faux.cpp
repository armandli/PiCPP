#include <ai/providers/faux.h>

#include <ai/stream_builder.h>
#include <json/write.h>
#include <util/utf8.h>

namespace pi::ai::providers {

namespace {
// Splits text into chunks of at most chunk_size bytes, never splitting a UTF-8
// sequence (a chunk may run longer to finish one); 0 = one chunk.  An invalid
// byte counts as a character of its own.
vec<strv> split_chunks(strv text, s::size_t chunk_size) {
  if (chunk_size == 0) return {text};

  vec<strv> chunks;
  s::size_t pos = 0;
  while (pos < text.size()){
    s::size_t end = pos;
    while (end < text.size() and end - pos < chunk_size){
      opt<util::DecodedCodePoint> decoded = util::decode_utf8(text, end);
      end = decoded.has_value() ? decoded->next : end + 1;
    }
    chunks.push_back(text.substr(pos, end - pos));
    pos = end;
  }
  return chunks;
}
} // namespace

FauxResponse FauxResponse::text(str text) {
  return FauxResponse{ {FauxText{ text }}, s::nullopt, Usage(), s::nullopt };
}

FauxProvider::FauxProvider(FauxOptions options)
  : mOptions(s::move(options)) {}

void FauxProvider::enqueue(FauxResponse response) {
  mResponses.push_back(response);
}

s::size_t FauxProvider::pending() const {
  return mResponses.size();
}

const vec<Context>& FauxProvider::calls() const {
  return mCalls;
}

strv FauxProvider::id() const {
  return mOptions.provider_id;
}

vec<Model> FauxProvider::models() {
  return {Model{"faux", "Faux", "faux", mOptions.provider_id, "", false, 128000, 4096, OpenAiCompat() }};
}

EventStream FauxProvider::stream(
    const Model& model,
    const Context& context,
    const StreamOptions& options,
    util::CancelToken cancel
){
  (void)options;

  EventStream stream;
  AssistantMessage message;
  message.api = "faux";
  message.provider = mOptions.provider_id;
  message.model = model.id;
  StreamBuilder builder(stream, s::move(message));

  mCalls.push_back(context);
  if (mResponses.empty()){
    builder.fail(StopReason::error, "no faux response queued");
    return stream;
  }
  FauxResponse response = s::move(mResponses.front());
  mResponses.pop_front();

  builder.start();

  // Delivers one step's text through `delta`; false once the request is aborted.
  auto send = [&](strv text, auto delta) {
    for (strv chunk : split_chunks(text, mOptions.chunk_size)){
      if (cancel.is_cancelled()){
        builder.fail(StopReason::aborted, "request aborted");
        return false;
      }
      delta(chunk);
    }
    return true;
  };

  bool has_tool_call = false;
  for (const FauxStep& step : response.steps){
    bool sent = true;
    if (const FauxText* text = s::get_if<FauxText>(&step)){
      sent = send(text->text, [&](strv delta) { builder.text_delta(delta); });
    }
    else if (const FauxThinking* thinking = s::get_if<FauxThinking>(&step)){
      sent = send(thinking->thinking, [&](strv delta) { builder.thinking_delta(delta); });
    }
    else {
      const FauxToolCall& call = s::get<FauxToolCall>(step);
      has_tool_call = true;
      builder.toolcall_start(call.id, call.name);
      sent = send(json::to_json(call.arguments), [&](strv delta) { builder.toolcall_delta(delta); });
    }
    if (not sent) return stream;
  }

  builder.set_usage(response.usage);
  if (response.error.has_value())
    builder.fail(StopReason::error, *response.error);
  else
    builder.finish(response.stop_reason.value_or(has_tool_call ? StopReason::toolUse : StopReason::stop));
  return stream;
}

}  // namespace pi::ai::providers
