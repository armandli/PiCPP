# PiCPP — Implementation Roadmap

A hand-written C++26 reimplementation of the [Pi coding agent](https://github.com/earendil-works/pi)
(TypeScript), using [PiG](https://github.com/MichaelKinsy/PiG) (Go port) as a second reference.

Each milestone:

- builds on the previous ones only,
- has **unit tests** you write first or alongside (no network, no real model),
- ends with a **demo** you can run against local Ollama,
- lists **what you'll notice is missing**, which is what the next milestone fixes.

Check items off as you go. `M0` through `M2` are done. M3 is done except two items added for Pi v1.5.0's type
changes; their stubs are marked `TODO(M3)` in the code.

**Definition of done:** PiCPP reproduces everything `@earendil-works/pi-coding-agent@1.5.0` ships, except what
§0.3 lists as *Not ported*, and the M19 parity suite shows it. M0–M15 port the core agent, M16–M17 port the
remaining features (extensions are native C++, §0.3), M18 ports the most popular community extensions, and M19
checks the result against Pi itself.

---

## 0. Orientation

### 0.1 Reference checkouts

Clone both references next to this repo, **pinned to the same Pi release**, and keep them open while you work:

```sh
git clone --branch v1.5.0 https://github.com/earendil-works/pi   ~/src/pi
git clone --branch v0.7.0 https://github.com/MichaelKinsy/PiG    ~/src/pig
```

**Why pin:** Pi `main` changes constantly, so "full reproduction" of `main` is a moving target. PiG v0.7.0 pins Pi
1.5.0 (`pig:internal/coding/pigversion/pigversion.go`) and runs its parity suite against it, so these two tags
describe the same behaviour. This file was checked against Pi v1.5.0. Move the pin only after M19 passes: diff
Pi's changelog to the next tag, add the new work here as checklist items, and re-clone both references.
(Pi v1.6.0 was the latest release on 2026-11-20.)

In this document, `pi:` means `~/src/pi/packages/` and `pig:` means `~/src/pig/`.
Useful first reads:

- `pi:coding-agent/README.md` — what Pi does, as a user sees it
- `pi:ai/README.md` — the LLM API, including how to point it at Ollama (search for "Ollama")
- `pig:docs/pig-architecture.md` — the Pi → Go module mapping
- `pig:docs/typescript-to-go-porting.md` — rules for porting TS idioms (mostly applies to C++ too)

### 0.2 Architecture

Pi is layered. Each layer only knows the ones below it:

```
            ┌────────────────────────────────────────────────────┐
  modes     │ print · repl · interactive(TUI) · json · rpc       │  src/modes
            ├────────────────────────────────────────────────────┤
  tui       │ FTXUI app · editor · transcript · markdown · footer│  src/tui
            ├────────────────────────────────────────────────────┤
  coding    │ AgentSession · system prompt · AGENTS.md · settings│  src/coding
            │ sessions(JSONL) · compaction · skills · /commands  │
            ├────────────────────────────────────────────────────┤
  tools     │ read · write · edit · bash · ls · grep · find      │  src/tools
            ├────────────────────────────────────────────────────┤
  agent     │ agent loop · AgentTool · AgentEvent · queues       │  src/agent
            ├────────────────────────────────────────────────────┤
  ai        │ Message/Content types · event stream · providers   │  src/ai
            │ (faux, openai-completions→Ollama, anthropic, ...)  │
            ├──────────────────────────┬─────────────────────────┤
  json/http │ Value · simdjson · schema│ libcurl · SSE · NDJSON  │  src/json  src/http
            ├──────────────────────────┴─────────────────────────┤
  util      │ strings · utf8 · cancel · paths · subprocess       │  src/util
            └────────────────────────────────────────────────────┘
```

Every layer from `util` to `tui` builds into one shared library, `libpicpp` (M8), which the `pi` executable,
the tests and every extension link. Extensions (`src/ext`, M17) are native C++ shared libraries that plug into
**registries** owned by the layers above: tools and tool hooks (`agent`, M5), providers (`ai`, M7), slash
commands, keybindings, renderers and UI slots (`coding`/`tui`, M12). PiCPP's own built-in features register
through those same registries, so the extension API is exercised from day one.

### 0.3 Pi → PiCPP module map

| Pi package / file                        | PiG                         | PiCPP          |
|------------------------------------------|-----------------------------|----------------|
| `ai/src/types.ts`, `utils/event-stream.ts` | `ai/types.go`, `ai/event_stream.go` | `src/ai` |
| `ai/src/api/openai-completions.ts`       | `ai/openai.go`, `ai/sse.go` | `src/ai/providers`, `src/http` |
| `ai/src/utils/json-parse.ts`, `validation.ts`; `schema/src/{validator,coerce}.ts` | `ai/streaming_json.go`, `ai/validate_arguments.go` | `src/json` |
| `agent/src/agent-loop.ts`, `agent.ts`    | `agent/`                    | `src/agent`    |
| `coding-agent/src/core/tools/*`          | `internal/codingagent/tools/` | `src/tools`  |
| `coding-agent/src/core/*` (session, settings, prompt, compaction) | `coding/`, `internal/codingagent/` | `src/coding` |
| `tui/` + `coding-agent/src/modes/interactive/` | `tui/`, `internal/codingagent/interactive_*.go` | `src/tui` |
| `coding-agent/src/main.ts`, `cli/args.ts`, `modes/*` | `cmd/pig/`        | `src/modes`, `src/main.cpp` |
| `coding-agent/src/core/extensions/*`     | `coding/extension/`, `coding/extension/host/` | `src/ext` |
| `mcp/`, `coding-agent/src/extensions/mcp/` | `mcp/`                    | `src/mcp`      |
| `codemode/`, `coding-agent/src/extensions/codemode/` | `codemode/`   | `src/codemode` |

**In scope:** everything `pi-coding-agent@1.5.0` ships, which depends on Pi's `ai`, `agent-core`, `tui`,
`codemode` and `mcp` packages. Extensions, MCP, codemode, packages and OAuth logins come last, in M17.

**Extensions are native C++, by design.** PiCPP doesn't run Pi's TypeScript/JavaScript extensions or install npm
packages that contain them. Instead it has a C++ extension API with the same capabilities (M17), and M18
reimplements the most popular Pi extensions against it. Packages that only carry skills, prompt templates or
themes still work unchanged: those are Markdown and JSON, not code.

**Not ported:**
- `durable`, `chord`, `env`, `evals` and the `telemetry` package. The coding agent doesn't depend on them.
- Inside coding-agent, the parts that report to Earendil's services or only serve Pi's own distribution:
  analytics and install telemetry (`core/telemetry.ts`, `enableAnalytics`, `enableInstallTelemetry`), bug-report
  upload (`core/bug-report*.ts`, `/bug`), the Radius share destination, self-update and npm version checks, and
  the Bun single-binary build (`src/bun/`).
- Windows-only code: `tools/powershell.ts`, `utils/windows-self-update.ts`, `utils/wsl.ts`.
- The TypeScript extension runtime (`core/extensions/{loader,jiti-loader,jiti-static-loader,virtual-modules}.ts`)
  and `npm:` package sources; see above.

M19 logs each of these as a documented divergence.

### 0.4 TypeScript → C++ idioms

| TypeScript (Pi)                               | C++26 (PiCPP)                                                     |
|-----------------------------------------------|-------------------------------------------------------------------|
| Discriminated union `{type: "text"} \| {...}` | `std::variant<TextContent, ...>` + `std::visit`; JSON tag picks the alternative |
| `interface X { ... }` holding data            | plain `struct` (PiG lesson: a data interface is **not** a virtual interface) |
| Behaviour interface (`Provider`, `AgentTool`) | abstract `struct` with virtual methods, owned via `std::unique_ptr`/`shared_ptr` |
| `Promise`, `async/await`                      | worker `std::jthread` + blocking calls; or `std::future`          |
| `AbortSignal` / `AbortController`             | `std::stop_token` / `std::stop_source` (wrap as `util::CancelToken`) |
| `EventStream` (async iterator + `result()`)   | mutex + condition_variable queue; consumer pops until terminal event |
| `T \| undefined`                              | `std::optional<T>`                                                |
| thrown errors on expected failure paths       | `std::expected<T, Error>`; exceptions only for real bugs          |
| plain JS objects / `JSON.parse`               | `json::Value` (owned, **ordered** object) built from simdjson     |
| TypeBox schema for tool params                | JSON-schema as a `json::Value` literal + your own validator       |
| `Promise.all` over tool calls                 | one `std::jthread`/`std::async` per call, joined; results kept in call order |

### 0.5 Testing strategy

- **Faux provider** (`ai/providers/faux`): scripts responses as text / thinking / tool-call steps. Every
  agent-loop, tool and session test uses it. No network. (Pi: `ai/src/providers/faux.ts`, PiG: `ai/faux.go`.)
- **Canned HTTP transport** (`http/transport`): a fake `HttpTransport` that records the request body and
  replays recorded SSE bytes split at random points. Provider tests assert *the JSON you send* and *the events you
  produce*.
- **Temp-dir fixtures** for tools: each test makes its own directory under `std::filesystem::temp_directory_path()`.
- **Render snapshots** for TUI components: render an `ftxui::Element` onto a fixed-size `ftxui::Screen` and
  compare `screen.ToString()` (strip styles where needed).
- **Live tests** go in files named `*_live_test.cpp`. They get the ctest label `live`, are excluded from
  `make test`, and run with `make test-live`. Skip with `GTEST_SKIP()` when Ollama isn't reachable.
- Write tests for "poisoned" histories early: aborted turns, empty text, tool calls without results (PiG lesson).

### 0.6 Conventions

- Namespaces `pi::<module>`; headers `src/<module>/<file>.h` included as `#include <ai/types.h>`.
- Header guards named after the path (`AI_TYPES_H`). The path-based name avoids collisions such as
  `ai/events.h` vs `agent/events.h`.
- Follow the `format-cpp` skill (struct over class, `lower_snake_case` functions, `mPrivateMember`,
  `not`/`and`/`or`, explicit enum underlying types).
- A module is an INTERFACE target until its first `.cpp` exists. After adding one, just re-run `make`.
- Tests: `test/<module>/<thing>_test.cpp` (picked up automatically).
- Keep Pi's names (`StopReason::toolUse`, `tool_execution_start`, ...) so you can grep the reference.

### 0.7 Ollama notes

- Pi talks to Ollama through its **OpenAI-compatible** endpoint: `POST http://localhost:11434/v1/chat/completions`
  with `"stream": true`. The response is SSE (`data: {...}\n\n`, ending with `data: [DONE]`).
- Compat quirks Pi applies for Ollama: no `developer` role (use `system`), no `reasoning_effort`; thinking
  text arrives in `delta.reasoning`. See `detectCompat` in `pi:ai/src/api/openai-completions.ts` and
  `pig:ai/openai.go`.
- **Context window gotcha:** `/v1` cannot set `num_ctx`, so Ollama uses its default context length (often only a few
  thousand tokens) and silently truncates the prompt. Set `OLLAMA_CONTEXT_LENGTH=32768` (or larger) for
  `ollama serve`, or create a Modelfile with `PARAMETER num_ctx`. M15 adds the native `/api/chat` API as a
  better fix.
- Pick a model that supports tool calling (check `ollama show <model>` → Capabilities: `tools`).

---

## M0 — Toolchain ✅

- [x] CMake (C++26) + Makefile wrapper, FetchContent: FTXUI v7.0.3, simdjson v5.0.2, googletest v1.18.0; libcurl
- [x] Module skeleton `src/<module>/` with placeholder headers, `test/<module>/`
- [x] Smoke test (gtest + simdjson + FTXUI + curl link)

```sh
make            # configure + build (Debug)
make test       # unit tests
make run        # ./build/Debug/pi
make help       # all targets
```

---

## M1 — Foundations: util + JSON ✅

**Goal:** the value types everything else is built from. No agent yet; this is pure library work with lots of
small tests.

### util
- [x] `util/strings`: `split`, `split_lines` (handle `\r\n`), `join`, `trim`, `starts_with`/`ends_with`,
      `replace_all`
- [x] `util/utf8`: decode/validate UTF-8, iterate code points, `display_width` (wide East-Asian = 2 cells;
      FTXUI has `string_width` you may reuse in the TUI only)
- [x] `util/cancel`: `CancelToken` wrapping `std::stop_source`/`std::stop_token`; `cancel()`,
      `is_cancelled()`, `on_cancel(callback)` via `std::stop_callback`

### json
- [x] `json/value`: `Value` = null | bool | int64 | double | string | Array | Object.
      `Object` **preserves insertion order** (e.g. `std::vector<std::pair<std::string, Value>>` + lookup).
      Accessors: `is_*`, `as_*`, `operator[](key)`, `find(key)`, `push_back`, `set(key, v)`
- [x] `json/parse`: `std::expected<Value, ParseError> parse(std::string_view)` using **simdjson DOM**
      (`simdjson::dom::parser`), converting recursively into `Value`
- [x] `json/write`: `std::string to_json(const Value&)`, compact output, correct escaping (`"`, `\\`, control chars,
      `\u00XX`), shortest round-trip doubles (`std::to_chars`)
- [x] Tests: round-trip (`parse(to_json(v)) == v`), key order preserved, escapes, unicode, big ints,
      malformed input → error with position

Decisions made in M1 (documented in code comments under `src/json/` and in `test/json/json_test.cpp`):

- Integers outside `int64` parse as the nearest `double` (precision loss accepted, like `JSON.parse`); beyond
  `DBL_MAX` they become ±infinity. simdjson's `number_as_string(true)` avoids `BIGINT_ERROR`.
- NaN / ±infinity are written as `null` (like `JSON.stringify`); integral doubles get `.0` so they read back as
  doubles.
- `ParseError::offset` is the byte index of the first byte that can't start valid JSON (`input.size()` if
  truncated). simdjson doesn't report positions, so a validating scanner finds it on the error path only.
- A moved-from `Value` is null.

**Read:** `pig:internal/orderedjson`, `pig:docs/typescript-to-go-porting.md` ("insertion order is observable").

**Why simdjson needs a wrapper:** simdjson's DOM is read-only and tied to the parser's buffer. Messages and tool
arguments must be built, mutated, stored and re-serialized, so they need an owned value type.

**Demo:** none yet (`make test`).

---

## M2 — HTTP + SSE ✅

**Goal:** get raw bytes from Ollama while they stream, and be able to stop early.

- [x] `http/transport`: abstract `HttpTransport` — `post_stream(url, headers, body, on_chunk, cancel)` →
      `std::expected<HttpStatus, HttpError>`; plus `get(url)` for listing models
- [x] `http/curl_client`: libcurl implementation
  - `CURLOPT_WRITEFUNCTION` forwards each chunk to `on_chunk`
  - cancellation: `CURLOPT_XFERINFOFUNCTION` returns non-zero when `CancelToken` is cancelled
  - one `curl_global_init` per process (function-local static); non-2xx → read body as error text
- [x] `http/sse`: incremental SSE decoder. `feed(bytes)` emits `{event, data}` per blank-line-terminated block.
      Must handle a chunk boundary anywhere (mid-line, mid-`\r\n`, mid-UTF-8 sequence), multi-line `data:`,
      `:` comments
- [x] Tests: SSE decoder fed one byte at a time == fed all at once; fake transport records request
- [x] Live test `test/http/ollama_live_test.cpp`: POST a tiny chat completion, assert ≥1 SSE event

**Read:** `pig:ai/sse.go`; the SSE spec section "Interpreting an event stream".

**Demo:** add a temporary `--raw-chat "<prompt>"` flag in `main.cpp` that posts
`{"model":"<id>","stream":true,"messages":[{"role":"user","content":"..."}]}` and prints each `data:` payload.
You'll see the JSON chunk format the next milestone has to parse.

**Missing:** it's just bytes, with no notion of messages, text deltas or usage.

---

## M3 — AI layer (text only) + print mode

**Goal:** `pi -p "question"` streams a real answer from Ollama, built on the same abstractions as Pi.

### Types (`ai/types`) — read `pi:ai/src/types.ts` in full first
- [x] Content blocks: `TextContent`, `ThinkingContent`, `ImageContent`, `ToolCall{id, name, arguments: Value}`
- [x] Messages: `UserMessage`, `AssistantMessage{content, api, provider, model, usage, stop_reason,
      error_message, timestamp}`, `ToolResultMessage{tool_call_id, tool_name, content, is_error}`;
      `Message = std::variant<...>`
- [x] `Usage{input, output, cache_read, cache_write, total_tokens, cost}`, `StopReason` enum
      (`stop, length, toolUse, error, aborted`)
- [x] `Model{id, name, api, provider, base_url, reasoning, context_window, max_tokens, compat}`
- [x] `Context{system_prompt, messages, tools}`
- [x] JSON (de)serialization for all of the above (needed for request building now and sessions in M10)

### Events + stream
- [x] `ai/events`: `AssistantMessageEvent` variant — `start`, `text_start/delta/end`,
      `thinking_start/delta/end`, `toolcall_start/delta/end`, `done{reason, message}`, `error{reason, message}`;
      each carries `content_index`
- [x] `ai/event_stream`: thread-safe queue. Producer `push()`es, consumer blocks on `next()`.
      **Exactly one** terminal event (`done` or `error`). `result()` returns the final `AssistantMessage`.
      After the stream is handed out, errors are never thrown; they become the `error` event
- [x] Tests: ordering, terminal-event enforcement, producer on another thread, consumer cancellation

### Providers
- [x] `ai/provider`: `struct Provider { virtual EventStream stream(const Model&, const Context&,
      const StreamOptions&, CancelToken) = 0; virtual std::vector<Model> models() = 0; }`
- [x] `ai/providers/faux`: scripted responses (list of steps per call), chunked deltas
- [x] `ai/providers/openai_completions` (text + thinking only for now):
  - request: `model`, `messages` (system as `system` role for Ollama), `stream: true`,
    `stream_options: {include_usage: true}`, `max_tokens`
  - parse chunks: `choices[0].delta.content` → text events, `delta.reasoning` / `reasoning_content` → thinking
    events, `finish_reason` → stop reason (`stop`→stop, `length`→length, `tool_calls`→toolUse, other→error),
    `usage` → `Usage`
  - HTTP error / cancel → `error` event with `StopReason::error` / `aborted`
- [x] Tests: canned SSE (record one from M2's demo into `test/ai/fixtures/`) → expected event sequence;
      request-body assertions; abort mid-stream → `aborted`

### Print mode
- [x] `modes/cli_args`: `-p/--print <prompt>`, `--model <provider>/<id>`, `--version`, `--help`
- [x] `modes/print_mode`: build `Context`, stream, print text deltas as they arrive, print usage at the end
- [x] Replace the M0 banner in `main.cpp` with mode dispatch

### Pi v1.5.0 follow-ups (stubs and failing tests marked `TODO(M3)`)
- [ ] `StopReason` has 7 values in Pi v1.5.0 (`pi:ai/src/types.ts`):
  - `pending`: `StreamBuilder` stamps it on the partial message at construction, so
    `StreamBuilder::message()` reports `pending` until the terminal event sets the real reason.
    `finish(pending)` is an error, as in Pi, where a stream that ends still `pending` fails
  - `deferred`: only produced by `streamDeferred` polling, which belongs to the unported `durable` path. Parse
    and encode it, and never produce it; `finish(deferred)` is an error too
  - `to_string` / `parse_stop_reason` / decode error text cover both
- [ ] `ai/transcript`: in Pi v1.5.0 the prompt and tools live **in the transcript**
  (`pi:ai/src/utils/transcript.ts`):
  - `SystemMessage{content, sections, tools_added, tools_removed, timestamp}` is a `Message` alternative,
    with JSON `{"role":"system",...}`
  - `normalize_context(Context)` folds `system_prompt` and `tools` into a leading `SystemMessage`, producing a
    `TranscriptContext`. `Context` stays as the public shorthand, as in Pi
  - replay helpers: `get_current_tools`, `get_current_system_message`, `get_current_system_prompt`,
    `system_message_text`
  - models without `compat.supports_mid_convo_system_messages` (Ollama among them) get
    `collapse_system_messages`: one leading system message holding the replayed state, later ones dropped.
    `resolve_transcript` picks between the two
  - `convert_messages` reads the system prompt from the resolved transcript instead of `Context::system_prompt`
- [ ] Tests: `test/ai/transcript_test.cpp`; `StopReason` and `SystemMessage` cases in `types_test.cpp`; `pending`
      cases in `stream_builder_test.cpp`; transcript cases in `openai_completions_test.cpp`

**Read:** `pi:ai/src/api/openai-completions.ts` (`buildParams`, `convertMessages`, the stream loop, `mapStopReason`),
`pi:ai/src/utils/event-stream.ts`, `pig:ai/event_stream.go`, `pig:ai/stream_builder.go`, `pig:ai/openai.go`.
For the follow-ups: `pi:ai/src/utils/{transcript,text}.ts`, `pi:ai/README.md` ("Stop Reasons", and the
`SystemMessage` part of the message types).

**Demo:** `make run ARGS='-p "Explain RAII in 3 sentences" --model ollama/qwen3.8:27b-mlx'`

**Missing:** every call starts from zero, so it can't answer a follow-up. It also can't see your files.

---

## M4 — Multi-turn REPL

**Goal:** a conversation in plain stdin/stdout before any TUI work.

- [ ] `modes/repl_mode`: read a line, append `UserMessage`, stream the answer, append `AssistantMessage`, repeat;
      `/quit`, `/clear`
- [ ] Ollama model discovery: `GET /v1/models` → `std::vector<Model>`; `/models` lists them
- [ ] Default `context_window` / `max_tokens` when the server doesn't say (make them configurable later in M7)
- [ ] Tests: REPL loop driven by a faux provider and `std::istringstream`/`std::ostringstream`

`/clear` and `/models` are REPL-only conveniences, not Pi commands. M12's registry replaces them with Pi's `/new`
and `/model`.

**Demo:** `make run` → chat with the model; ask a follow-up question that depends on the previous answer.

**Missing:** ask it "what's in src/main.cpp?" and it can only guess. It has no way to act on the world, which is
the difference between a chatbot and an agent.

---

## M5 — Tool calling + the agent loop ⭐ (core concept)

**Goal:** the model can call `read` and `ls`, see the results, and keep going until it answers.

### Tool calls through the provider
- [ ] `Tool{name, description, parameters: Value /*JSON schema*/}` sent as
      `tools: [{type:"function", function:{name, description, parameters}}]`
- [ ] Streaming tool calls: merge `delta.tool_calls[i]` by `index`/`id`; accumulate `function.arguments` text;
      emit `toolcall_start/delta/end`
- [ ] `json/partial`: parse **incomplete** JSON (close open strings/arrays/objects) so the UI can show
      arguments while they stream (Pi: `parseStreamingJson`)
- [ ] Assistant messages with tool calls → `tool_calls` with arguments as a JSON *string*; tool results →
      `role: "tool", tool_call_id`
- [ ] Tests: canned SSE with a tool call split across many chunks; two parallel tool calls; partial JSON cases

### Validation
- [ ] `json/schema`: validate `arguments` against the tool schema (subset: `type`, `properties`, `required`,
      `enum`, `items`, `additionalProperties`, `minimum`/`maximum`) with Pi's coercions (`"5"`→5 for integers,
      `null`→absent for optional fields); errors carry a JSON path
- [ ] Tests: each keyword, coercions, nested paths in error messages

### Agent loop
- [ ] `agent/tool`: `AgentTool{name, label, description, parameters, execute(id, args, cancel, on_update) →
      ToolResult{content, details, is_error}}`
- [ ] `agent/events`: `agent_start/end`, `turn_start/end`, `message_start/update/end`,
      `tool_execution_start/update/end`
- [ ] `agent/loop` (port `runLoop` from `pi:agent/src/agent-loop.ts`), **sequential** tools for now:
  1. emit `agent_start`, `turn_start`, the prompt messages
  2. stream the assistant response (emit `message_start` / `message_update` / `message_end`)
  3. on `error`/`aborted` → `turn_end`, `agent_end`, return
  4. for each tool call: look up tool (unknown → error result), validate (invalid → error result), execute,
     emit `tool_execution_*`, append `ToolResultMessage`
  5. if `stop_reason == length` with tool calls: fail them all (arguments may be truncated)
  6. `turn_end`; loop again while there were tool calls
- [ ] `AgentLoopConfig` hooks (`pi:agent/src/types.ts`). The coding layer and extensions build on them:
  - `convert_to_llm`: `AgentMessage` (an LLM `Message` **or** an app-defined custom message) → the `Message`s sent
    to the provider. The custom roles (`bashExecution`, `compactionSummary`, `branchSummary`, `custom`) arrive in
    M10–M17
  - `transform_context(messages, cancel)`: rewrites the context before each model call
  - `before_tool_call` (may block the call → error result) and `after_tool_call` (may replace the result)
  - `get_api_key(provider)`, resolved on every call because keys can expire mid-run; `prepare_next_turn`
- [ ] **Registries** (the seams M17's extensions plug into; built-in code registers the same way):
  - `agent/tool_registry`: tools by name, which the loop looks up per call. Built-in tools register into it;
    registering a name again replaces the tool (Pi's `tool-override.ts` example). Tools can be added or removed
    while the agent is idle (Pi's `dynamic-tools.ts`)
  - tool hook chains: `before_tool_call` and `after_tool_call` hold **lists** of handlers, not one callback.
    They run in registration order, with Pi's composition rules (`docs/extensions.md`, "Handler Composition"):
    a `before_tool_call` handler can mutate the input in place for later handlers, the first one that blocks
    stops the call (a gate: fail closed), and each `after_tool_call` handler receives the result returned by the
    previous one (a chained transformation)
  - handles: registering returns a `Subscription` that unregisters on destruction or `reset()`, so `/reload` can
    drop everything an extension added
- [ ] Tests (faux provider): text-only turn; tool call → result → final text; unknown tool; invalid args;
      tool throws → `is_error` result; event order matches Pi exactly; blocked tool call; replaced tool result;
      custom message converted or dropped by `convert_to_llm`; two hook handlers chain in order and the first
      block wins; a replaced tool; a `Subscription` going away removes its handler

### First tools
- [ ] `tools/truncate`: `truncate_head` / `truncate_tail` — 2000 lines / 50 KB defaults, result metadata
      (`truncated`, `truncated_by`, `total_lines`, `output_lines`)
- [ ] `util/paths`: resolve relative to cwd, expand `~`, strip a leading `@`
- [ ] `tools/read {path, offset?, limit?}`: 1-indexed offset; Pi's exact continuation notices, e.g.
      `[Showing lines A-B of T. Use offset=B+1 to continue.]`; offset past EOF → error
- [ ] `tools/ls {path?, limit?=500}`: case-insensitive sort, `/` suffix for dirs, dotfiles included,
      `(empty directory)`
- [ ] Tests: temp-dir fixtures for each notice/limit branch

**Read:** `pi:agent/src/agent-loop.ts`, `pi:agent/src/types.ts`, `pi:ai/src/utils/json-parse.ts`,
`pi:ai/src/utils/validation.ts`, `pi:schema/src/{validator,coerce}.ts`,
`pi:coding-agent/src/core/tools/{read,ls,truncate}.ts`, `pig:agent/tool_execution.go`,
`pig:ai/validate_arguments.go`.

**Demo:** REPL with tools → "What does src/main.cpp do? Look at the file." Watch it call `ls`, then `read`,
then answer. Print each tool call and result in the REPL so you can see the loop.

**Missing:** it can look but not change anything. "Add a --help flag" can't be done.

---

## M6 — Coding tools: write, edit, bash

**Goal:** first real end-to-end coding task.

- [ ] `tools/write {path, content}`: create parent dirs, overwrite, `Successfully wrote to <path>`
- [ ] `tools/edit {path, edits:[{oldText, newText}]}` — port carefully, it's the most important tool:
  - strip BOM, detect CRLF, normalize to LF; restore both on write
  - **every `oldText` is matched against the original file**, not after earlier edits
  - exact match first; then fuzzy (NFKC-ish: trailing whitespace per line, smart quotes → ASCII,
    Unicode dashes → `-`, special spaces → space)
  - errors: empty `oldText`, not found, found more than once (report count), overlapping edits, no change
  - accept legacy shapes: top-level `oldText`/`newText`, `edits` as a JSON string
  - result `Successfully replaced N block(s) in <path>` + a unified diff in `details` (used by the TUI in M12)
- [ ] `util/subprocess`: `posix_spawn`/`fork+exec` `bash -c`, stdout+stderr merged through one pipe, new process
      group, streaming reads, kill the **group** on timeout/cancel
- [ ] `tools/bash {command, timeout?}`: `truncate_tail` the output; if truncated, write the full output to a temp
      file and mention it; append `Command exited with code N` (→ `is_error`), `Command timed out after N seconds`,
      `Command aborted`; empty → `(no output)`
- [ ] `tools/bash` options for the M7 settings `shellPath` (shell executable; leading `~` allowed) and
      `shellCommandPrefix` (prepended to every command; see `pi:coding-agent/docs/shell-aliases.md`)
- [ ] Tests: every edit error path; CRLF + BOM files; fuzzy match; bash exit codes, timeout, large output,
      process-group kill (spawn `sleep` children)

**Read:** `pi:coding-agent/src/core/tools/{edit,edit-diff,bash,write}.ts`,
`pig:internal/codingagent/tools/{edit,bash}.go`.

**Demo:** in a scratch git repo: "create a C++ hello world with a Makefile, build it and run it".
The agent writes files, runs `make`, reads the error output and fixes its own mistakes.

**Missing:**
- The model doesn't know the tool conventions or your project rules, and it tends to `cat` files with bash.
- Ctrl-C kills the whole program instead of stopping the turn.
- Every run needs `--model`.

---

## M7 — System prompt, context files, configuration

**Goal:** the agent behaves like Pi: it knows its tools, follows `AGENTS.md`, and remembers your defaults.

- [ ] `coding/system_prompt`: Pi's section-based prompt. Every section after the preamble renders as
      `<name>\n...\n</name>`, in this order:
      - preamble
      - `tools`: `- name: snippet` per visible tool, then `Additional tools may be available.`
      - `rules`: per-tool guidelines, then prompt guidelines, deduplicated
      - `docs`: where the harness docs live (point it at PiCPP's own docs)
      - `addendum`, `project_context`, `skills` (M13), `cwd`

      Copy the actual strings from `system-prompt.ts` and each tool's `promptSnippet`/guidelines
- [ ] Custom prompts: `SYSTEM.md` replaces the preamble and drops `tools`/`rules`/`docs`; `APPEND_SYSTEM.md`
      becomes `addendum`. The project `.pi/` file wins over the agent-dir file, but only in a trusted project.
      CLI: `--system-prompt`, `--append-system-prompt`
- [ ] Build the prompt as `SystemMessage` sections (M3's `ai/transcript`). `diff_system_prompt_sections`
      (`system-prompt.ts`): when a section changes mid-session (AGENTS.md edited, `/reload`, tools toggled),
      append a `SystemMessage` patch (`null` removes a section). Don't rewrite the first message
- [ ] `coding/context_files`: for the agent dir and **every ancestor directory from `/` down to cwd**, take the
      first of `AGENTS.override.md`, `AGENTS.md`, `AGENTS.MD`, `CLAUDE.md`, `CLAUDE.MD`.
      `--no-context-files/-nc` turns them off
- [ ] `coding/settings`: `~/.pi/agent/settings.json` (override dir with `PI_CODING_AGENT_DIR`) merged with
      project `.pi/settings.json`. Use `pi:coding-agent/docs/settings.md` as the checklist: parse every key in
      its tables now, and wire each one up in the milestone that needs it. Needed now: `defaultProvider`,
      `defaultModel`, `defaultTools`, `enabledModels`, `sessionDir`, `compaction.*`, `branchSummary.*`,
      `retry.*`, `shellPath`, `shellCommandPrefix`, `httpProxy`, `httpIdleTimeoutMs`. Some keys are agent-dir
      only (`defaultProjectTrust`, `httpProxy`, ...), so a project file must not be able to set them
- [ ] `coding/project_trust` (`pi:coding-agent/docs/security.md#understand-project-trust`): when cwd has protected
      resources, ask for a trust decision before loading them. Protected resources are `.pi/settings.json`,
      `.pi/mcp.json`, `.pi/{extensions,skills,prompts,themes}`, `.pi/SYSTEM.md`, `.pi/APPEND_SYSTEM.md`, and
      `.agents/skills` in cwd or an ancestor; a bare `.pi/` doesn't count. Remember decisions;
      `defaultProjectTrust` (`ask` | `always` | `never`) is the fallback; `--approve/-a` and `--no-approve/-na`
      decide for one run. `/trust` and its selector come in M12
- [ ] CLI flags for this layer (`pi:coding-agent/src/cli/args.ts`, `docs/cli.md`): `--provider`, `--api-key`
      (non-persistent override; needs `--model` or `--models`), `--tools/-t`, `--exclude-tools/-xt`,
      `--no-tools/-nt`, `--no-builtin-tools/-nbt`, `--list-models [search]`, `--offline`, `--verbose`
- [ ] Initial message (`cli/{file-processor,initial-message}.ts`): piped stdin, `@file` argument text and the
      first CLI message combine into one user message. Image `@file`s join in M16
- [ ] `coding/models_config`: `~/.pi/agent/models.json` custom providers, e.g.
      ```json
      {"providers":{"ollama":{"baseUrl":"http://localhost:11434/v1","api":"openai-completions",
        "apiKey":"ollama","models":[{"id":"qwen3.8:27b-mlx","contextWindow":32768}]}}}
      ```
      `apiKey` values: literal, `$ENV`/`${ENV}`, or `!shell command`
- [ ] `ai/registry`: providers + models by `provider/id`; merges static config with dynamic `/v1/models`.
      It's a registry in the M5 sense: built-in providers register into it, and M17 extensions add and remove
      providers and models (including virtual models) through the same calls
- [ ] Tests: prompt snapshot against a golden file; context-file discovery in a nested temp tree; settings merge
      precedence; agent-dir-only keys ignored in project settings; env/`!cmd` key resolution; trust decision
      matrix (protected resource × setting × flag); stdin + `@file` + message combination; section diff patches

**Read:** `pi:coding-agent/src/core/{system-prompt,resource-loader,settings-manager,model-registry}.ts`,
`pi:coding-agent/src/core/{project-trust,trust-manager}.ts`,
`pi:coding-agent/src/cli/{args,file-processor,initial-message}.ts`,
`pi:coding-agent/docs/{settings,models,cli,security}.md`.

**Demo:** put an `AGENTS.md` in a project saying "always write tests with GoogleTest". Ask for a feature and it
follows the rule. Run `pi` with no flags and it uses your default model.

**Missing:** you can't interrupt a runaway command, multiple tool calls run one at a time, and you can't type while
it works.

---

## M8 — Control: abort, parallel tools, queues, retry

**Goal:** the agent can be interrupted and steered, and recovers from transient errors.

- [ ] `agent/agent`: `Agent` owns state (`messages`, `model`, `tools`, `is_streaming`), runs the loop on a
      `std::jthread`, `prompt()` (rejects while running), `abort()`, `wait_for_idle()`, event subscribers
- [ ] Abort: SIGINT handler → `abort()` (first Ctrl-C stops the turn, second exits). Cancel propagates to curl and
      the bash process group. Aborted assistant messages stay in history with `stopReason: aborted`
- [ ] Parallel tool execution: validate/prepare **sequentially** (emit starts in order), execute **concurrently**,
      `tool_execution_end` in completion order, `ToolResultMessage`s appended in the **assistant's source order**;
      tools may declare `sequential` (e.g. edit/write on the same file → per-file mutation queue)
- [ ] `toolExecution` loop option: `parallel` (default) or `sequential` for a whole batch
- [ ] `agent/queues`: steering messages (injected after the current tool batch) and follow-up messages (after the
      agent would stop); modes `one-at-a-time` / `all`, from the `steeringMode` / `followUpMode` settings
- [ ] Retry with exponential backoff on retryable errors (429, 5xx, connection reset); context-overflow detection
      (Ollama: "prompt too long"; see `pi:ai/src/utils/overflow.ts`)
- [ ] Two retry layers, as in Pi: agent-level retries (`retry.enabled`, `retry.maxRetries`, `retry.baseDelayMs`,
      `retry.maxAgentDelayMs`) and provider-level retries (`retry.provider.{timeoutMs,maxRetries,maxRetryDelayMs}`,
      `pi:ai/src/utils/provider-retry.ts`); a pending retry can be cancelled (RPC `abort_retry` in M14)
- [ ] Tests: abort mid-stream and mid-tool; parallel results ordering with tools that sleep different amounts;
      steering message lands between turns; retry counts with a flaky fake transport

### Build: `libpicpp`
Do this before M9: the TUI adds many `.cpp` files, and M17's extensions need the core as a shared library to
link against.
- [ ] `cmake/PiModule.cmake`: the modules `util` → `tui` stop being separate STATIC libraries and build into one
      SHARED library, `libpicpp` (alias `pi::core`). `modes` and `main.cpp` stay in the `pi` executable, which
      links `pi::core`, and so do the tests. Keep the per-module source layout and the "may only depend on
      modules above it" rule (check it with a test that greps `#include`s, since CMake no longer enforces it)
- [ ] Symbol visibility: export the public API. Start with default visibility; move to `-fvisibility=hidden`
      plus a `PICPP_API` export macro only if load times or symbol clashes call for it
- [ ] Runtime lookup: `pi` and the test executables find `libpicpp` through an rpath (`@loader_path/...` on macOS,
      `$ORIGIN/...` on Linux), so `make run` and `make test` work without `DYLD_LIBRARY_PATH`
- [ ] Sanitizer builds (`PICPP_SANITIZE`) keep working: the shared library and everything linking it are built
      with the same flags
- [ ] Tests: the full suite passes unchanged against the shared library

**Read:** `pi:agent/src/agent.ts`, `pi:ai/src/utils/{retry,provider-retry,overflow}.ts`, `pig:agent/lifecycle.go`,
`pig:agent/tool_execution.go`.

**Demo:** "run `sleep 100`", then Ctrl-C. The turn stops and the REPL survives. Ask it to read 5 files and watch
them load concurrently.

**Missing:** the REPL is hard to read (no markdown, interleaved streams, no status), and quitting loses the
conversation.

---

## M9 — Interactive TUI v1 (FTXUI)

**Goal:** a Pi-like terminal UI. Use the `ftxui-guide-cpp` skill.

- [ ] **Decide the screen model first** and write down why:
  - *Fullscreen* (`ScreenInteractive::Fullscreen()` + your own scrollable transcript), like Pi's default
    `TuiAltScreen`; or
  - *Inline* (`FitComponent`/`TerminalOutput`): print finished messages to normal scrollback and keep only the
    live area (streaming message, editor, footer) in FTXUI. This is closer to Pi's `TuiMainScreen`.
  - Pi ships **both**, chosen by the `tuiMode` setting (`regular` | `fullscreen`, default `fullscreen`) and
    `--tui-mode`. Build one now; the other is an M12 item
- [ ] `coding/agent_session`: glue object the UI talks to — `prompt()`, `abort()`, current model, event fan-out.
      Modes now depend on this, not on `Agent` directly
- [ ] `tui/app`: agent runs on its worker thread; every agent event is marshalled to the UI thread with
      `screen.Post(...)` and followed by `screen.PostEvent(Event::Custom)` to redraw. **Never touch UI state
      from the agent thread**
- [ ] `tui/transcript`: user, assistant (text + dimmed thinking), and tool blocks (name, args, status,
      collapsed output); live-updates the streaming message
- [ ] `tui/editor`: multi-line input (Enter submits, Shift+Enter/Alt+Enter newline), history Up/Down
- [ ] `tui/footer`: cwd, model, ↑input ↓output tokens, context % (`last input tokens / context_window`)
- [ ] Esc aborts the running turn; spinner while streaming; Ctrl-C clears the editor and a second Ctrl-C exits;
      Ctrl-D on an empty editor exits
- [ ] Startup and status: startup header with version and loaded resources (hidden by `quietStartup`); first-time
      setup when no model is configured (`components/first-time-setup.ts`); retry countdown and compaction
      status (`components/{status-indicator,countdown-timer}.ts`)
- [ ] `modes/interactive_mode`: default when stdin and stdout are TTYs; print mode otherwise
- [ ] Tests: render each component to a fixed `ftxui::Screen` and snapshot `ToString()`; editor key handling via
      `component->OnEvent(Event::Character('a'))`

**Read:** `pi:coding-agent/src/modes/interactive/interactive-mode.ts` (skim; it's 7k lines),
`pi:coding-agent/src/modes/interactive/components/`, `pig:docs/pig-architecture.md` (TUI section).

**Demo:** `make run` → a real interactive coding agent UI.

**Missing:** close the terminal and the whole session is gone.

---

## M10 — Sessions

**Goal:** persist and resume conversations exactly like Pi (format-compatible if you want).

- [ ] `coding/session`: JSONL file at `~/.pi/agent/sessions/--<cwd with / replaced by ->--/<timestamp>_<uuid>.jsonl`
  - line 1 header `{"type":"session","version":3,"id","timestamp","cwd"}`, plus optional `parentSession`
  - entries `{type, id(8 hex), parentId, timestamp, ...}`: `message`, `model_change`, `thinking_level_change`,
    `compaction`, `branch_summary`, `session_info`, `label`, `custom`, `custom_message`, `context_edit`. They
    form a **tree**, and the current leaf is the position. Read and write all of them now, before their features
    exist, so that files written by Pi load
  - `context_edit {targetId, replacement}`: `null` hides the target from model context; otherwise its content is
    replaced. The latest edit on the active branch wins, and edits are branch-relative (`buildSessionProjection`
    in `session-format.md`)
  - append each entry as it happens (crash-safe); build `Context` by walking leaf → root
  - migrations: v1 and v2 files upgrade on load (`migrateV1ToV2`, `migrateV2ToV3` in `session-manager.ts`)
- [ ] Custom `AgentMessage` roles (`pi:coding-agent/src/core/messages.ts`): `bashExecution` (M13 `!cmd`),
      `compactionSummary` (M11), `branchSummary` (M16), `custom` (extensions, M17). `convert_to_llm` (M5) turns
      each one into LLM messages
- [ ] CLI: `--continue/-c` (latest for cwd), `--resume/-r` (picker), `--session <path|id>`, `--no-session`,
      `--session-dir`, `--session-id` (not combinable with `--session`/`-c`/`-r`), `--name/-n`; `sessionDir`
      setting
- [ ] Slash commands: `/new`, `/resume` (FTXUI list of sessions with first message + date), `/session` (info and
      stats), `/name`, `/import <file.jsonl>` (keeps the session's id and cwd)
- [ ] Tests: write → reload → identical `Context`; tree with two branches → correct path from each leaf;
      truncated last line (crash) still loads; v1/v2 files migrate; `context_edit` hides and replaces, per
      branch; a session file written by Pi v1.5.0 (fixture) loads

**Read:** `pi:coding-agent/docs/session-format.md`, `pi:coding-agent/src/core/session-manager.ts`.

**Demo:** quit mid-task, `pi -c`, continue where you left off.

**Missing:** run a long session on a local model and you hit the context window: errors, or Ollama silently
dropping the start of the conversation.

---

## M11 — Compaction

**Goal:** sessions can outlive the context window.

- [ ] Token estimate: `chars / 4` per message (plus last reported `usage.input` when available)
- [ ] Trigger: `context_tokens > context_window - reserveTokens` (default 16384, scale down for small local
      windows) — check after each tool batch and before each prompt; also on a context-overflow error
      (compact, then retry once)
- [ ] Cut point: walk back from the end keeping ~`keepRecentTokens` (default 20000); cut only at user/assistant
      messages, **never right before a tool result**
- [ ] Summarize the older part with Pi's summarization prompt (Goal / Constraints / Progress / Key Decisions /
      Next Steps / Critical Context); later compactions update the previous summary
- [ ] Write a `compaction` entry `{summary, firstKeptEntryId, tokensBefore}`; context rebuild = summary message +
      entries from `firstKeptEntryId`
- [ ] `/compact [instructions]`; footer shows compaction happening
- [ ] Settings: `compaction.enabled` (auto-compaction on/off; RPC `set_auto_compaction` in M14) and
      `compaction.modelOverrides` (per-model token settings keyed by `provider/modelId`). The summary reaches the
      model as a `compactionSummary` message (M10)
- [ ] Tests: cut-point selection on crafted histories (including split turns); context rebuild after 1 and 2
      compactions; overflow → compact → retry with the faux provider

**Read:** `pi:coding-agent/docs/compaction.md`, `pi:coding-agent/src/core/compaction/{compaction,utils}.ts`.

**Demo:** set a small `contextWindow` in models.json, work for a while, and watch it compact and keep going.

---

## M12 — TUI v2: polish

- [ ] `tui/markdown`: Markdown → FTXUI elements (headings, bold/italic/code spans, lists, fenced code blocks
      in a border, block quotes, tables optional). Either hand-write a block/inline parser (good exercise) or
      add md4c via FetchContent. Syntax-highlight fenced code (`utils/syntax-highlight.ts`), honour
      `markdown.codeBlockIndent`; mermaid diagrams (`markdown.mermaid`) are optional
- [ ] Edit-tool diff rendering (red/green lines from `details`), bash output tail with expand/collapse (Ctrl-O),
      thinking show/hide (Ctrl-T); per-tool renderers as in `core/tools/renderers/`
- [ ] **UI registries** (M17 extensions plug into these; built-ins register the same way):
  - `tui/renderer_registry`: tool name → renderer for its call and result (`ftxui::Element`), with a default for
    unknown tools. The built-in renderers from `core/tools/renderers/` register here, and a later registration
    for the same tool wins (Pi: `registerToolRenderer`)
  - message and entry renderers keyed by custom type, for custom messages and entries (Pi:
    `registerMessageRenderer`, `registerEntryRenderer`)
  - UI slots that extensions can fill: widgets above or below the editor, a status line, footer segments, the
    header, and overlays (Pi examples `widget-placement.ts`, `status-line.ts`, `custom-footer.ts`,
    `custom-header.ts`, `overlay-test.ts`)
- [ ] `coding/slash_commands` registry + dispatcher for all 24 Pi built-ins (`core/slash-commands.ts`). Built-in
      commands register through the same call extensions will use; a name collision is reported, not silently
      overwritten (Pi's resource collision diagnostics).
      In this milestone: `/model` selector (Ctrl-L), `/settings` (settings selector + submenus),
      `/scoped-models`, `/hotkeys`, `/copy`, `/changelog`, `/reload` (keybindings, skills, prompts, themes,
      context files; extensions after M17), `/trust` (trust selector), `/export`, `/share`, `/quit`.
      The rest land with their features: `/new`, `/resume`, `/session`, `/name`, `/import` (M10), `/compact`
      (M11), `/login`, `/logout` (M15), `/tree`, `/fork`, `/clone`, `/thinking` (M16). `/bug` is not ported (§0.3)
- [ ] Scoped models: `--models <patterns>`, `enabledModels`, the `/scoped-models` selector; Ctrl-P cycles through
      them (`app.model.cycleForward`)
- [ ] `/export` and `--export <input> [output]`: self-contained HTML of a session (`core/export-html/`, including
      ANSI → HTML for tool output). `/share` publishes that export as a GitHub gist (`sessionShareDestination`;
      Radius is not ported)
- [ ] `tui/selectors`: generic filterable list overlay (`Modal`) reused by model/session/settings/trust/theme
      pickers
- [ ] Autocomplete: `/command` names, `@path` fuzzy file search (port `pi:tui/src/fuzzy.ts`), Tab path
      completion; `autocompleteMaxVisible`
- [ ] Editor parity (`pi:tui/src/{editor-component,undo-stack,kill-ring,word-navigation}.ts`): undo, kill ring
      with yank (Ctrl-Y) and yank-pop (Alt-Y), word motion; Ctrl-G opens the draft in an external editor
      (`externalEditor` setting, then `$VISUAL`, then `$EDITOR`; `modes/interactive/external-editor.ts`)
- [ ] `tui/keybindings`: action → key registry with `~/.pi/agent/keybindings.json` overrides (no hard-coded keys).
      Port every action in `docs/keybindings.md` with its default key (e.g. `app.message.copy` Ctrl-X,
      `app.thinking.toggle` Ctrl-T, `app.tools.expand` Ctrl-O). Extensions register new actions and shortcuts
      here (Pi: `registerShortcut`); reserved keys can't be taken
- [ ] `tui/theme`: theme roles (accent, muted, error, diff add/remove, ...) → FTXUI decorators. Built-in dark and
      light themes, system light/dark detection, theme JSON files (`docs/themes.md`,
      `modes/interactive/theme/theme-schema.ts`) from the agent dir and `.pi/themes/` (trusted projects only),
      `themes` setting, `--theme`, `--no-themes`, theme picker in `/settings`
- [ ] Footer v2: session cost and cache read/write tokens (`core/{footer-data-provider,usage-totals}.ts`)
- [ ] The second screen model from M9 (`tuiMode`), plus fullscreen extras: scrollbar, mouse-wheel scrolling,
      copy-on-select (`fullscreen*` settings), transcript search (`pi:tui/src/alt-screen-search.ts`)
- [ ] Tests: markdown snapshots; fuzzy scorer; keybinding override parsing; kill-ring/undo sequences; theme file
      parsing; HTML export snapshot

**Read:** `pi:tui/src/components/{editor,markdown}.ts`, `pi:tui/src/{autocomplete,fuzzy}.ts`,
`pi:coding-agent/src/core/{slash-commands,keybindings}.ts`, `pi:coding-agent/docs/{keybindings,themes,tui}.md`,
`pi:coding-agent/src/modes/interactive/components/{settings-selector,scoped-models-selector}.ts`.

---

## M13 — Search tools + resources

- [ ] `tools/grep {pattern, path?, glob?, ignoreCase?, literal?, context?, limit?=100}` via
      `rg --json --line-number --color=never --hidden`; lines cut at 500 chars
- [ ] `tools/find {pattern, path?, limit?=1000}` via `fd --glob --color=never --hidden`
- [ ] Getting `rg`/`fd`: Pi downloads them when they're missing (`utils/tools-manager.ts`). Port that, or require
      them on `PATH` and log the divergence (M19)
- [ ] `defaultTools` setting (`+grep`, `-bash`) and the read-only tool set
- [ ] `coding/skills`: discover `skills/**/SKILL.md` (frontmatter `name`, `description`) and list them in the
      system prompt's `skills` section as `<available_skills>`, with a hint naming the tool that reads them (`read`,
      otherwise `bash`). `/skill:name` forces one (`enableSkillCommands`). Sources: agent dir, `.pi/skills` and
      `.agents/skills` (trusted only), `skills` setting, `--skill <path>`; `--no-skills/-ns` turns off discovered
      skills but explicit `--skill` paths still load
- [ ] `coding/prompt_templates`: `prompts/*.md` → `/name args` with `$1`, `$@`, `$ARGUMENTS`, `${1:-default}`;
      `prompts` setting, `--prompt-template <path>`, `--no-prompt-templates/-np`
- [ ] `!cmd` (run, show, send output to the model) and `!!cmd` (run, don't send). Store the run as a
      `bashExecution` message (M10), and make it cancellable (RPC `abort_bash` in M14)
- [ ] Tests: rg/fd output parsing from fixtures; frontmatter parsing; template substitution table

**Read:** `pi:coding-agent/src/core/tools/{grep,find}.ts`, `pi:coding-agent/src/core/{skills,prompt-templates}.ts`,
`pi:coding-agent/docs/skills.md`.

**Demo:** drop this repo's `.claude/skills/format-cpp` into `~/.pi/agent/skills/` and ask PiCPP to format a file.

---

## M14 — JSON and RPC modes

- [ ] `--mode json` (`pi:coding-agent/docs/json.md`): one session header line, then every session event as one
      JSON line. Strict LF framing (strip an optional `\r`; U+2028/U+2029 are not record breaks). Stdout carries
      only JSONL; diagnostics go to stderr. `--mode text` is the default
- [ ] `--mode rpc`: JSONL commands on stdin, responses `{type:"response", id, command, success, data}` + events on
      stdout; split on `\n` only; no session header. Port every command in
      `pi:coding-agent/docs/rpc-commands.md`:
  - prompting: `prompt`, `steer`, `follow_up`, `abort`, `abort_retry`, `clear_queue`, `bash`, `abort_bash`
  - state: `get_state`, `get_messages`, `get_entries`, `get_last_assistant_text`, `get_session_stats`,
    `get_commands`
  - models: `get_available_models`, `set_model`, `cycle_model`, `get_available_thinking_levels`,
    `set_thinking_level`, `cycle_thinking_level`
  - behaviour: `set_steering_mode`, `set_follow_up_mode`, `set_auto_compaction`, `set_auto_retry`, `compact`
  - sessions: `new_session`, `switch_session`, `set_session_name`, `fork`, `clone`, `get_fork_messages`,
    `get_tree`, `export_html`
  - extension UI: `extension_ui_request` / `extension_ui_response`, after M17 (`docs/rpc-extension-ui.md`)
- [ ] End-to-end tests: spawn `pi --mode rpc` with a faux model (e.g. `PI_TEST_FAUX=1`) and script a whole
      conversation (PiG's `test-faux` idea); one test per command group

**Read:** `pi:coding-agent/docs/{json,rpc,rpc-commands}.md`, `pi:coding-agent/src/modes/{json-event.ts,rpc/}`.

---

## M15 — More providers

- [ ] `ai/providers/ollama_native`: `POST /api/chat` with NDJSON streaming (`http/ndjson`), `options.num_ctx`
      from the model's `contextWindow`, `think`, native `tool_calls`. This is a **deliberate deviation** from
      Pi; document why (context length control)
- [ ] `ai/transform`: cross-provider replay — drop or convert foreign thinking blocks, synthesize
      `"No result provided"` results for orphaned tool calls, skip errored/aborted assistant messages
- [ ] `ai/providers/anthropic_messages` (SSE, content blocks, thinking signatures, cache control)
- [ ] `ai/providers/openai_responses` (OpenAI Responses API; Azure and Codex variants build on it)
- [ ] `ai/catalog`: Pi's built-in model catalog (`pi:ai/src/models.generated.ts`, `providers/*.models.ts`).
      Generate a data file from the pinned tag with a script: id, name, api, provider, base URL, context window,
      max tokens, cost, compat flags. `ai/cost`, `--list-models`, default context windows and compat detection
      read it
- [ ] Provider table: one entry per `pi:ai/src/providers/*.ts` (base URL, API, auth env var, extra headers).
      Most providers are `openai-completions` with a different base URL. Port the remaining chat APIs in
      `pi:ai/src/api/`: `google-generative-ai`, `google-vertex`, `mistral-conversations`,
      `bedrock-converse-stream`, `azure-openai-responses`, and `openai-codex-responses` (it needs M17 OAuth)
- [ ] Compat flags from the catalog shape each request (`pi:ai/src/compat.ts`): developer vs system role,
      reasoning parameters, `supportsMidConvoSystemMessages` (M3's `collapse_system_messages`), mid-conversation
      tool changes
- [ ] `coding/auth`: `~/.pi/agent/auth.json` API keys + per-provider env vars (`pi:ai/src/env-api-keys.ts`);
      `/login` with an API key, `/logout`. OAuth comes in M17
- [ ] `ai/cost`: per-model $/Mtok → `usage.cost`, shown in the footer
- [ ] Tests: canned SSE/NDJSON per provider; switching models mid-session replays history correctly; catalog
      lookup and cost; env-var key resolution per provider

**Read:** `pi:ai/src/api/{anthropic-messages,openai-responses,transform-messages}.ts`, `pi:ai/src/models.ts`
(`calculateCost`), `pi:ai/src/{compat,env-api-keys}.ts`, `pi:coding-agent/src/core/auth-storage.ts`,
`pi:coding-agent/docs/{providers,custom-provider}.md`.

**Demo:** start a task with a local model, `/model` → a cloud model mid-session, and continue.

---

## M16 — Session tree, thinking levels, images

- [ ] `/tree`: navigate the session tree (`components/tree-selector.ts`). Filters from `treeFilterMode`
      (`default`, `no-tools`, `user-only`, `labeled-only`, `all`); `label` entries mark points. Double Escape on
      an empty editor runs `doubleEscapeAction` (`tree` | `fork` | `none`)
- [ ] `/fork` and `--fork <path|id>` (new session from an earlier message; header `parentSession`; not combinable
      with `--session`/`-c`/`-r`/`--no-session`), `/clone`
- [ ] Branch summaries: when `/tree` switches branches, offer to summarize the branch being left into a
      `branch_summary` entry (`core/compaction/branch-summarization.ts`; `branchSummary.reserveTokens`,
      `branchSummary.skipPrompt`)
- [ ] Thinking levels `off`, `minimal`, `low`, `medium`, `high`, `xhigh`, `max`, mapped per provider
      (`thinkingBudgets`). Also `defaultThinkingLevel`, `modelThinkingLevels`, `--thinking`, the `/thinking`
      selector, and Shift+Tab to cycle (`app.thinking.cycle`). Changes are stored as `thinking_level_change`
- [ ] Images: `read` on png/jpg returns `ImageContent` (resize per `images.autoResize`, fix EXIF orientation,
      `images.blockImages`); image `@file` CLI arguments; paste images from the clipboard
      (`utils/clipboard-image.ts`)
- [ ] Terminal image display: Kitty / iTerm2 protocols (`pi:tui/src/terminal-image.ts`; `terminal.showImages`,
      `terminal.imageWidthCells`)
- [ ] Tests: tree filters and label entries; fork header and flag conflicts; branch summary entry; thinking-level
      mapping per provider; image resize and EXIF orientation

**Read:** `pi:coding-agent/docs/{sessions,compaction}.md`, `pi:coding-agent/src/core/compaction/branch-summarization.ts`,
`pi:coding-agent/src/modes/interactive/components/{tree-selector,thinking-selector}.ts`, `pi:tui/src/terminal-image.ts`.

---

## M17 — Extensibility: native C++ extensions, MCP, codemode, packages, OAuth

**Goal:** the rest of what `pi-coding-agent@1.5.0` ships. Extensions are C++ shared libraries written against a
C++ version of Pi's `ExtensionAPI`, built with the same toolchain as PiCPP ("build key" model: one compiler,
standard library, FTXUI and PiCPP version; anything else is refused). Running Pi's TypeScript extensions is out of
scope (§0.3).

### Extension API
- [ ] `src/ext/api.h`: `pi::ext::ExtensionApi`, the C++ mirror of Pi's `ExtensionAPI` and `ExtensionContext`
      (`pi:coding-agent/src/core/extensions/types.ts`, `docs/extensions.md`). Go through every `pi.*` method,
      every `ctx.*` member and every event in that doc, map each one, and keep a table in `src/ext/README.md`
      of anything that has no C++ equivalent. The main groups:
  - registration: `register_tool` (into M5's tool registry; same name replaces), `register_command`,
    `register_shortcut`, `register_flag`, `register_provider` / `unregister_provider`, virtual models,
    `register_mcp_server`, `register_tool_renderer`, `register_message_renderer`, `register_entry_renderer`,
    `register_markdown_transformer`
  - events (`on(...)`): lifecycle, resource, session, agent, model, tool (`tool_call` → M5 `before_tool_call`,
    `tool_result` → `after_tool_call`), user-bash and input events, with Pi's composition rules (notification,
    in-place mutation, chained transformation, gate, last override)
  - actions: `send_message`, `send_user_message`, `append_entry`, `set_session_name`, `set_label`, `set_model`,
    thinking level, `exec`, `get_commands`, `get_settings`, an inter-extension event bus (`pi.events`)
  - context: `ui()` (notify, confirm, select, input, editor, overlays, widgets, status, footer), `mode()`,
    `has_ui()`, `cwd()`, `is_project_trusted()`, session manager, model registry, the cancel token, `is_idle()`,
    `abort()`, `compact()`, `get_context_usage()`, `get_system_prompt()`; command-only context: `new_session`,
    `fork`, `navigate_tree`, `switch_session`, `reload`, `wait_for_idle`
- [ ] Renderers and UI return real **FTXUI** values (`ftxui::Element`, `ftxui::Component`): no serialization,
      because extensions are built against the same FTXUI
- [ ] Entry points: every extension library exports exactly two symbols, `extern "C" const char*
      pi_extension_build_key()` and `extern "C" void pi_extension_register(pi::ext::ExtensionApi&)`. A
      `PI_EXTENSION(name)` macro defines both; extensions build with `-fvisibility=hidden`
- [ ] Build key: compiler id and version, standard library, C++ standard, PiCPP version, `pi::ext::API_VERSION`,
      FTXUI version, and the ABI-affecting flags (sanitizers, `_GLIBCXX_DEBUG`/libc++ hardening mode). Generated at
      configure time into a header both the host and the extension see; a mismatch refuses to load and names the
      first differing field

### Host and loading
- [ ] `src/ext/host`: implements `ExtensionApi` on top of the M5/M7/M12 registries. Each extension gets its own
      `ExtensionApi` instance that records its `Subscription`s, so unloading an extension is dropping that instance
- [ ] Two ways in, one path: built-in extensions are compiled into `pi` and listed in one explicit table in
      `src/ext/builtins.cpp` (no static-initializer self-registration: static libraries drop unreferenced
      objects); dynamic ones are loaded with `dlopen`. Both call the same `pi_extension_register`
- [ ] Loader: `dlopen(path, RTLD_NOW | RTLD_LOCAL)`, `dlsym` both entry points, compare build keys, register.
      **Never `dlclose`**: code from the library may still be referenced by `std::function`s, vtables or
      `thread_local`s
- [ ] Discovery: `~/.pi/agent/extensions/` (`*.so` / `*.dylib`, or a directory holding one),
      `.pi/extensions/` **only in trusted projects** (loading a library runs its code), the `extensions` setting,
      `-e/--extension <path>`, `--no-extensions/-ne`. Load order follows Pi's, since handler order depends on it
- [ ] Boundary rules, enforced by the host:
  - every call into extension code is wrapped in `try`/`catch`: a throw becomes an `is_error` tool result or a
    logged extension error, never a crash; gate handlers that throw count as "block" (fail closed)
  - extension handlers run on the agent thread; `ui()` calls marshal to the UI thread with `screen.Post` and block
    on a future (M9's rule); no extension touches FTXUI state from another thread
  - every handler gets the turn's `util::CancelToken`; cancellation is cooperative
- [ ] `/reload`: drop every extension's registrations and call `pi_extension_register` again on a fresh
      `ExtensionApi`, so in-memory state resets as Pi's does. To pick up a **rebuilt** library, copy it to a
      unique temporary path and `dlopen` the copy (the old one stays mapped). Log this as a divergence: code reload
      is for development
- [ ] RPC `extension_ui_request` / `extension_ui_response` (`docs/rpc-extension-ui.md`), so `ui()` works in
      `--mode rpc`; with no UI at all (`-p`, json mode), `has_ui()` is false and `ui()` calls fail fast
- [ ] SDK for extension authors: installed headers plus a CMake package (`find_package(PiCPP)`) providing
      `pi_add_extension(<name> <sources>...)`, which sets visibility, the build key and the output name. In-tree
      extensions live in `extensions/<name>/` and build with `make`
- [ ] Test fixtures: port the Pi examples that exercise the API to C++ under `test/ext/fixtures/` and load them
      with `dlopen` in tests: `hello`, `commands`, `confirm-destructive`, `dynamic-tools`, `tool-override`,
      `input-transform`, `message-renderer`, `entry-renderer`, `widget-placement`, `status-line`, `event-bus`,
      `send-user-message`, `reload-runtime`
- [ ] Tests: build-key mismatch refused with the differing field named; throwing handler → error result;
      throwing gate blocks; blocking `tool_call`; chained `tool_result`; `ui()` called from the agent thread;
      `/reload` resets extension state; `.pi/extensions/` ignored in an untrusted project; load order decides
      handler order

### Built-in extensions (`pi:coding-agent/src/extensions/`), written against `pi::ext` and linked into `pi`
- [ ] MCP: port `pi:mcp` and `extensions/mcp`. Server config in `mcp.json` (`docs/mcp.md`); stdio and streamable
      HTTP transports (legacy SSE is unsupported); MCP tools as `AgentTool`s; resources; OAuth for HTTP servers;
      `--no-mcp`
- [ ] Codemode: port `pi:codemode` and `extensions/codemode`. The model still writes JavaScript, so this still
      embeds a QuickJS sandbox (quickjs-ng via FetchContent) that calls the agent's tools. Settings
      `codemode.mode`, `codemode.inlineBudget`; it can also call non-chat models such as classifiers and image
      models (`docs/codemode.md`)
- [ ] `tool_search`: registered inactive; when activated (`--tools`, `defaultTools`), the model can find and enable
      tools on demand (`extensions/tool-search/`)
- [ ] `llama`: llama.cpp provider with Hugging Face model downloads (`extensions/llama/`, `docs/llama-cpp.md`)

### Packages, OAuth, SDK
- [ ] Packages (`package-manager-cli.ts`, `docs/packages.md`): `pi install|remove|update|list` from `git:` or
      local sources. Skills, prompt templates and themes install as-is; C++ extensions in a package are **built
      from source** against the installed SDK (`find_package(PiCPP)`), so installing one needs a C++ toolchain.
      `npm:` sources are not supported (§0.3). `packages` setting
- [ ] OAuth subscription logins in `/login` (`pi:ai/src/auth/oauth/`: PKCE with a local callback server, device
      code), for every provider listed there (Anthropic, GitHub Copilot, OpenAI Codex/ChatGPT, ...). Tokens
      refresh through `get_api_key` (M5); this unlocks `openai-codex-responses` (M15)
- [ ] SDK: `libpicpp` plus `coding::AgentSession` and resource loading as an embeddable library (`docs/sdk.md`,
      `core/sdk.ts`)
- [ ] Tests: an MCP stdio server fixture; codemode sandbox limits; package install from a local directory that
      contains a skill and a C++ extension; OAuth PKCE with a fake callback

**Read:** `pi:coding-agent/docs/{extensions,mcp,codemode,packages,sdk,virtual-models}.md`,
`pi:coding-agent/src/core/extensions/{types,runner}.ts`, `pi:coding-agent/examples/extensions/`.

**Demo:** build `test/ext/fixtures/confirm_destructive` out of tree with `find_package(PiCPP)`, drop the library
into `~/.pi/agent/extensions/`, ask the agent to `rm -rf build`, and answer the confirmation dialog. Then add an
MCP server and call its tools.

---

## M18 — Popular extensions, in C++

**Goal:** PiCPP ships C++ versions of the extensions Pi users install most, each giving the same functionality
as the Pi package it is modelled on. They live in `extensions/<name>/`, build as loadable libraries by default
(`PICPP_BUNDLE_EXTENSIONS=ON` links them into `pi` through the built-in table instead), and each has faux-provider
tests in `test/extensions/<name>/`.

**How this list was chosen** (2026-11-20): npm monthly downloads of `pi-package`-tagged packages (the same figure
the [pi.dev/packages](https://pi.dev/packages) gallery ranks by), Composio's
["Top 10 Pi Agent Extensions"](https://composio.dev/blog/top-pi-extensions) (Aug 2026), and the 🔥 marks in
[awesome-pi](https://github.com/BubblePtr/awesome-pi). Packages were merged by capability, and Pi's own examples
(`pi:coding-agent/examples/extensions/`) were added where they cover the same feature. Download counts are noisy:
re-check them before starting.

**Reimplement, don't copy:** port from each package's documented behaviour. Check its license before reusing any
code or prompt text.

### Tier 1 — the workflow staples
- [ ] `permissions`, modelled on **cc-safety-net** (197K/mo) and **@gotgenes/pi-permission-system** (49K/mo, 🔥),
      plus Pi's `permission-gate.ts`, `protected-paths.ts`, `confirm-destructive.ts`, `dirty-repo-guard.ts`:
      allow / deny / ask rules per tool, path glob and bash command pattern, from `~/.pi/agent/permissions.json`
      and `.pi/permissions.json` (trusted projects only); built-in detection of destructive commands
      (`rm -rf`, `git reset --hard`, `git push --force`, ...) and secret files (`.env`, private keys); `ask`
      uses `ui().confirm`, and with no UI it fails closed; an audit log as custom entries.
      *Exercises:* `tool_call` gate, `ui()`, custom entries
- [ ] `ask_user`, modelled on **@juicesharp/rpiv-ask-user-question** (182K/mo, Composio, 🔥) and Pi's
      `question.ts` / `questionnaire.ts`: a tool the model calls with structured questions (options,
      multi-select, free-text "other"), shown as an FTXUI dialog, answers returned as the tool result; with no UI
      it returns an error telling the model to state its assumption and continue.
      *Exercises:* tool + custom overlay + renderer
- [ ] `todo`, modelled on **@juicesharp/rpiv-todo** (89K/mo, Composio, 🔥) and Pi's `todo.ts`: a `todo` tool
      (add, update, complete, remove, list) whose state is stored as custom entries, so it survives `/reload`,
      compaction and resume, and follows the current branch of the session tree; a live widget above the editor.
      *Exercises:* `append_entry`, session events, widget slot
- [ ] `plan_mode`, modelled on **@narumitw/pi-plan-mode** (38K/mo, 🔥), **@plannotator/pi-extension** (99K/mo,
      Composio) and Pi's `plan-mode/`: `/plan` switches to read-only (the gate allows `read`, `ls`, `grep`,
      `find` and read-only bash), adds a planning section to the system prompt as an M7 `SystemMessage` patch,
      saves the plan to a file, and leaving the mode sends the plan as the next prompt. Plannotator's browser
      annotation UI is out of scope.
      *Exercises:* command, gate, system prompt patch, status indicator
- [ ] `subagents`, modelled on **pi-subagents** (1.1M/mo, Composio, 🔥), **@tintinweb/pi-subagents** (85K/mo, 🔥)
      and Pi's `subagent/`: a `subagent` tool that runs a child agent with its own context, tools and model on
      a task; agent definitions as Markdown files with frontmatter (name, description, tools, model) in
      `~/.pi/agent/agents/` and `.pi/agents/`; single, parallel and chained runs; optional git worktree
      isolation; live progress through `tool_execution_update`; Ctrl-C aborts the children. Pi's example spawns
      `pi --mode json -p --no-session` per child; an in-process child `AgentSession` on its own thread is the
      C++ alternative. Pick one and record why.
      *Exercises:* streaming tool updates, cancellation, renderers
- [ ] `web_access`, modelled on **pi-web-access** (1.5M/mo, Composio, 🔥), **@juicesharp/rpiv-web-tools** (🔥) and
      **@ollama/pi-web-search** (🔥): `web_search` with pluggable backends (self-hosted SearXNG, Ollama's web
      search API, Brave, Tavily), `web_fetch` (HTML → readable text/Markdown, truncated like `read`), PDF text
      extraction (via `pdftotext` when installed), and a shallow `github_clone` into a cache directory that the
      normal tools then read. YouTube understanding is out of scope.
      *Exercises:* `http` module, settings, tool registration
- [ ] `mcp_proxy`, modelled on **pi-mcp-adapter** (2.3M/mo, Composio, 🔥): an alternative mode for M17's MCP
      client that exposes **one** `mcp` tool (search servers and tools, describe, call) instead of every
      server's tools, and starts servers lazily on first use, so MCP costs almost no context until it's needed.
      *Exercises:* dynamic tools, M17 MCP
- [ ] `lens`, modelled on **pi-lens** (115K/mo, Composio, 🔥) and **@narumitw/pi-lsp** (🔥): after every `write` or
      `edit`, collect diagnostics and append them to the tool result, so the agent sees its own errors at once.
      An LSP client over stdio (JSON-RPC with `Content-Length` framing, not JSONL) with per-language server
      config (clangd for this repo), plus optional formatter and linter commands.
      *Exercises:* chained `tool_result`, `util/subprocess`

### Tier 2 — quality of life
- [ ] `btw`, modelled on **pi-btw** (37K/mo, 🔥): `/btw <question>` answers a side question using the current
      context and shows it in an overlay; nothing is added to the session
- [ ] `status_line`, modelled on **pi-powerline-footer** (35K/mo, 🔥), **@narumitw/pi-statusline** (🔥) and Pi's
      `status-line.ts` / `custom-footer.ts` / `model-status.ts`: a powerline footer with cwd, git branch and
      dirty state, model, thinking level, tokens, cost and context %
- [ ] `notify`, modelled on Pi's `notify.ts` (and awesome-pi's `notify`): a terminal notification when the agent
      finishes and waits for input (OSC 777 for Ghostty / iTerm2 / WezTerm, OSC 99 for Kitty). The smallest
      extension here, so a good first one to write
- [ ] `git_checkpoint`, modelled on Pi's `git-checkpoint.ts`: a git stash checkpoint at each turn, so `/fork`
      can offer to restore the code to that point in history
- [ ] `memory`, modelled on **pi-memory** (94K/mo) and **pi-hermes-memory** (🔥): `memory_write` /
      `memory_search` tools over Markdown files in `~/.pi/agent/memory/` and `.pi/memory/`, indexed with SQLite
      FTS5 (add the SQLite amalgamation via FetchContent), with a short index injected as a system prompt section
- [ ] `context_store`, modelled on **context-mode** (213K/mo, Composio, 🔥) and **pi-context-prune**: tool outputs
      above a size threshold are stored in the FTS5 store from `memory` and replaced in context by a summary and a
      handle; a `context_search` tool pulls slices back. Measure tokens per session with and without it on the
      same M19 scenario

### Tier 3 — larger, optional
- [ ] `interactive_shell`, modelled on **pi-interactive-shell** (🔥) and Pi's `interactive-shell.ts`: run
      interactive programs (vim, psql, ssh) in an overlay. Needs a PTY (`forkpty`) and a terminal emulator that
      draws the screen grid into FTXUI (consider libvterm). The hardest UI work in this list
- [ ] `goal`, modelled on **pi-goal-x** (36K/mo) and **@narumitw/pi-goal**: `/goal <objective>` keeps queuing
      follow-ups (M8) until the model reports the goal met or a turn / token / time budget runs out; progress is
      kept in custom entries
- [ ] Browser automation (**pi-agent-browser-native**, **pi-chrome** 🔥, **@onkernel/browser-loop** 194K/mo):
      covered by connecting a browser-automation MCP server through M17. A native Chrome DevTools Protocol
      extension needs a WebSocket client; build it only if MCP falls short

### Deliberately not ported
- Vendor integrations (**@postman/postman-plugin**, **@datadog/pi-plugin**, **@langchain/langsmith-pi-extension**,
  **@braintrust/pi-extension**, Composio): connect the vendor's MCP server through M17, or install their skills
  as-is where the package is skills-only
- **pi-claude-bridge**: it is built on the TypeScript Claude Agent SDK
- **billion-context**: the highest download count in the gallery (3.9M/mo) but only 4 dependents, so the number
  is probably inflated; read what it actually does before deciding
- **openwiki**, **glimpseui**: standalone tools rather than Pi extensions; **@juicesharp/rpiv-i18n**: localization
  for the rpiv skills

**Demo:** in a fresh project, `/plan` a feature, approve it, watch `subagents` split the work while `lens` feeds
clangd errors back into each edit and `permissions` stops a `git push --force`.

---

## M19 — Parity verification

**Goal:** prove the reproduction instead of claiming it.

- [ ] Scenario suite: scripted faux-model conversations (text, tool calls, parallel tools, abort, steering, retry,
      compaction, fork, tree) run through `--mode rpc` on both Pi v1.5.0 and PiCPP, comparing the JSONL after
      normalizing ids, timestamps and paths. On the Pi side, load the faux model as an extension
      (`pig:test/parity/testdata/test-faux-provider.ts`); on PiCPP, use the built-in faux behind
      `PI_TEST_FAUX`. Start from PiG's scenario families in `pig:test/parity/scenarios/`
- [ ] Extension parity: for each Pi example ported in M17 and each extension in M18, a scenario run on Pi (with
      the TypeScript original) and PiCPP (with the C++ port) that compares the observable behaviour: tool
      results, blocked calls, entries written, messages sent
- [ ] Golden outputs taken from Pi itself: system prompt text, every tool's output on shared fixtures, `--help`,
      the settings and keybinding tables
- [ ] Session files work both ways: Pi resumes a PiCPP session and PiCPP resumes a Pi one
- [ ] `docs/divergences.md`: every intentional difference, with its reason (native C++ extensions, Ollama native
      API, how `rg`/`fd` are found, extension code reload, everything §0.3 doesn't port)
- [ ] `docs/port_map.md`: every Pi v1.5.0 source file → its PiCPP file or "not ported" (model:
      `pig:docs/parity/PORT_MAP.md`, `pig:test/parity/coverage.md`)
- [ ] Bump procedure: diff Pi's changelog to the next tag, add the new work to this file, re-pin both references
      (§0.1); re-check the M18 popularity list at the same time

**Read:** `pig:test/parity/README.md`, `pig:test/parity/AGENTS.md`.

---

## Appendix — Milestone dependency summary

```
M1 ─► M2 ─► M3 ─► M4 ─► M5 ─► M6 ─► M7 ─► M8 ─► M9 ─► M10 ─► M11 ─► M12 ─► M16 ─► M17 ─► M18 ─► M19
                               │                  └──► M14     └──► M13
                               └──────────────────────────────────► M15 (any time after M8)
```

M8 ends with the `libpicpp` split, which M9 onward builds on. M17 needs M12–M15 as well (registries, commands,
skills, RPC, providers). M18's extensions are independent of each other; `context_store` builds on `memory`,
`mcp_proxy` on M17's MCP client. M19 can start as soon as M14 exists: add scenarios as each feature lands, rather
than all at the end.
