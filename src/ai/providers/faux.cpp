#include <ai/providers/faux.h>

#include <ai/stream_builder.h>

namespace pi::ai::providers {

FauxResponse FauxResponse::text(str text) {
  // TODO(M3): one FauxText step; everything else default.
  (void)text;
  return {};
}

FauxProvider::FauxProvider(FauxOptions options)
  : mOptions(s::move(options)) {}

void FauxProvider::enqueue(FauxResponse response) {
  // TODO(M3): push to the back.
  (void)response;
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
  // TODO(M3): see faux.h.
  return {};
}

EventStream FauxProvider::stream(
    const Model& model,
    const Context& context,
    const StreamOptions& options,
    util::CancelToken cancel
){
  // TODO(M3): see faux.h.  Suggested shape:
  //   EventStream stream;
  //   StreamBuilder builder(stream, AssistantMessage{.api = "faux", ...});
  //   record context; pop the next response (none → builder.fail(...); return stream)
  //   builder.start();
  //   for each step: split into chunks (UTF-8 aware, see util/utf8.h decode_utf8)
  //     and before each chunk: if cancel.is_cancelled() → builder.fail(aborted, ...); return
  //   set_usage; response.error ? fail(error, *error) : finish(stop_reason or derived)
  //   return stream;
  (void)model; (void)context; (void)options; (void)cancel;
  return EventStream{};
}

}  // namespace pi::ai::providers
