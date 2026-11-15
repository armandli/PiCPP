#include <csignal>
#include <cstdlib>

#include <iostream>
#include <thread>

#include <modes/cli_args.h>
#include <modes/dispatch.h>
#include <util/aliases.h>
#include <util/cancel.h>

namespace {

volatile s::sig_atomic_t Interrupted = 0;

void on_sigint(int){
  Interrupted = 1;
}

} // namespace

int main(int argc, char** argv) {
  auto args = pi::modes::parse_cli_args(argc, argv);
  if (not args){
    const pi::modes::CliExit& exit = args.error();
    // --help / --version go to stdout; usage errors to stderr.
    s::ostream& os = exit.is_error() ? s::cerr : s::cout;
    os << exit.output;
    if (not exit.output.empty() and exit.output.back() != '\n') os << '\n';
    return exit.code;
  }

  // Cancelling from inside a signal handler isn't async-signal-safe, so a
  // watcher thread forwards the flag.  After the first Ctrl-C the default
  // handler is restored, so a second one kills the process outright.
  pi::util::CancelToken cancel;
  s::signal(SIGINT, on_sigint);
  s::jthread watcher([&cancel](s::stop_token stop){
    while (not stop.stop_requested() and not Interrupted)
      st::sleep_for(sc::milliseconds(50));
    if (Interrupted){
      cancel.cancel();
      s::signal(SIGINT, SIG_DFL);
    }
  });

  return pi::modes::dispatch(*args, s::cout, s::cerr, cancel);
}
