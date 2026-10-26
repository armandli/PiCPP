#include <modes/dispatch.h>

#include <ai/providers/faux.h>
#include <ai/providers/openai_completions.h>
#include <http/curl_client.h>
#include <modes/print_mode.h>

namespace pi::modes {

uptr<ai::Provider> make_provider(const CliArgs& args) {
  // TODO(M3): see header.
  (void)args;
  return nullptr;
}

opt<ai::Model> resolve_model(const CliArgs& args) {
  // TODO(M3): see header.
  (void)args;
  return s::nullopt;
}

int dispatch(const CliArgs& args, s::ostream& out, s::ostream& err, util::CancelToken cancel) {
  // TODO(M3): see header.
  (void)args; (void)out; (void)err; (void)cancel;
  return 2;
}

}  // namespace pi::modes
