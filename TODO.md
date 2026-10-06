# PiCPP — Implementation Roadmap

A hand-written C++26 reimplementation of the [Pi coding agent](https://github.com/earendil-works/pi)
(TypeScript), using [PiG](https://github.com/MichaelKinsy/PiG) (Go port) as a second reference.

Each milestone:

- builds on the previous ones only,
- has **unit tests** you write first or alongside (no network, no real model),
- ends with a **demo** you can run against local Ollama,
- lists **what you'll notice is missing**, which is what the next milestone fixes.

Check items off as you go. `M0` is done.

---

## 0. Orientation

### 0.1 Reference checkouts

Clone both references next to this repo and keep them open while you work:

```sh
git clone https://github.com/earendil-works/pi      ~/src/pi
git clone https://github.com/MichaelKinsy/PiG       ~/src/pig
```

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

### 0.3 Pi → PiCPP module map

| Pi package / file                        | PiG                         | PiCPP          |
|------------------------------------------|-----------------------------|----------------|
| `ai/src/types.ts`, `utils/event-stream.ts` | `ai/types.go`, `ai/event_stream.go` | `src/ai` |
| `ai/src/api/openai-completions.ts`       | `ai/openai.go`, `ai/sse.go` | `src/ai/providers`, `src/http` |
| `ai/src/utils/json-parse.ts`, `validation.ts` | `ai/streaming_json.go`, `agent/validate*.go` | `src/json` |
| `agent/src/agent-loop.ts`, `agent.ts`    | `agent/`                    | `src/agent`    |
| `coding-agent/src/core/tools/*`          | `internal/codingagent/tools/` | `src/tools`  |
| `coding-agent/src/core/*` (session, settings, prompt, compaction) | `coding/`, `internal/codingagent/` | `src/coding` |
| `tui/` + `coding-agent/src/modes/interactive/` | `tui/`, `internal/codingagent/interactive_*.go` | `src/tui` |
| `coding-agent/src/main.ts`, `cli/args.ts`, `modes/*` | `cmd/pig/`        | `src/modes`, `src/main.cpp` |

**Not ported** (not needed to understand or use a coding agent): `mcp`, `codemode`, `durable`, `chord`,
`server`, `client`, `protocol`, `env`, `telemetry`, `evals`, TypeScript extensions/packages, OAuth logins.
Some come back as stretch goals in M16.

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

## M1 — Foundations: util + JSON

**Goal:** the value types everything else is built from. No agent yet; this is pure library work with lots of
small tests.

### util
- [ ] `util/strings`: `split`, `split_lines` (handle `\r\n`), `join`, `trim`, `starts_with`/`ends_with`,
      `replace_all`
- [ ] `util/utf8`: decode/validate UTF-8, iterate code points, `display_width` (wide East-Asian = 2 cells;
      FTXUI has `string_width` you may reuse in the TUI only)
- [ ] `util/cancel`: `CancelToken` wrapping `std::stop_source`/`std::stop_token`; `cancel()`,
      `is_cancelled()`, `on_cancel(callback)` via `std::stop_callback`

### json
- [ ] `json/value`: `Value` = null | bool | int64 | double | string | Array | Object.
      `Object` **preserves insertion order** (e.g. `std::vector<std::pair<std::string, Value>>` + lookup).
      Accessors: `is_*`, `as_*`, `operator[](key)`, `find(key)`, `push_back`, `set(key, v)`
- [ ] `json/parse`: `std::expected<Value, ParseError> parse(std::string_view)` using **simdjson DOM**
      (`simdjson::dom::parser`), converting recursively into `Value`
- [ ] `json/write`: `std::string to_json(const Value&)`, compact output, correct escaping (`"`, `\\`, control chars,
      `\u00XX`), shortest round-trip doubles (`std::to_chars`)
- [ ] Tests: round-trip (`parse(to_json(v)) == v`), key order preserved, escapes, unicode, big ints,
      malformed input → error with position

**Read:** `pig:internal/orderedjson`, `pig:docs/typescript-to-go-porting.md` ("insertion order is observable").

**Why simdjson needs a wrapper:** simdjson's DOM is read-only and tied to the parser's buffer. Messages and tool
arguments must be built, mutated, stored and re-serialized, so they need an owned value type.

**Demo:** none yet (`make test`).

---

## M2 — HTTP + SSE

**Goal:** get raw bytes from Ollama while they stream, and be able to stop early.

- [ ] `http/transport`: abstract `HttpTransport` — `post_stream(url, headers, body, on_chunk, cancel)` →
      `std::expected<HttpStatus, HttpError>`; plus `get(url)` for listing models
- [ ] `http/curl_client`: libcurl implementation
  - `CURLOPT_WRITEFUNCTION` forwards each chunk to `on_chunk`
  - cancellation: `CURLOPT_XFERINFOFUNCTION` returns non-zero when `CancelToken` is cancelled
  - one `curl_global_init` per process (function-local static); non-2xx → read body as error text
- [ ] `http/sse`: incremental SSE decoder. `feed(bytes)` emits `{event, data}` per blank-line-terminated block.
      Must handle a chunk boundary anywhere (mid-line, mid-`\r\n`, mid-UTF-8 sequence), multi-line `data:`,
      `:` comments
- [ ] Tests: SSE decoder fed one byte at a time == fed all at once; fake transport records request
- [ ] Live test `test/http/ollama_live_test.cpp`: POST a tiny chat completion, assert ≥1 SSE event

**Read:** `pig:ai/sse.go`; the SSE spec section "Interpreting an event stream".

**Demo:** add a temporary `--raw-chat "<prompt>"` flag in `main.cpp` that posts
`{"model":"<id>","stream":true,"messages":[{"role":"user","content":"..."}]}` and prints each `data:` payload.
You'll see the JSON chunk format the next milestone has to parse.

**Missing:** it's just bytes, with no notion of messages, text deltas or usage.

---

## M3 — AI layer (text only) + print mode

**Goal:** `pi -p "question"` streams a real answer from Ollama, built on the same abstractions as Pi.

### Types (`ai/types`) — read `pi:ai/src/types.ts` in full first
- [ ] Content blocks: `TextContent`, `ThinkingContent`, `ImageContent`, `ToolCall{id, name, arguments: Value}`
- [ ] Messages: `UserMessage`, `AssistantMessage{content, api, provider, model, usage, stop_reason,
      error_message, timestamp}`, `ToolResultMessage{tool_call_id, tool_name, content, is_error}`;
      `Message = std::variant<...>`
- [ ] `Usage{input, output, cache_read, cache_write, total_tokens, cost}`, `StopReason` enum
      (`stop, length, toolUse, error, aborted`)
- [ ] `Model{id, name, api, provider, base_url, reasoning, context_window, max_tokens, compat}`
- [ ] `Context{system_prompt, messages, tools}`
- [ ] JSON (de)serialization for all of the above (needed for request building now and sessions in M10)

### Events + stream
- [ ] `ai/events`: `AssistantMessageEvent` variant — `start`, `text_start/delta/end`,
      `thinking_start/delta/end`, `toolcall_start/delta/end`, `done{reason, message}`, `error{reason, message}`;
      each carries `content_index`
- [ ] `ai/event_stream`: thread-safe queue. Producer `push()`es, consumer blocks on `next()`.
      **Exactly one** terminal event (`done` or `error`). `result()` returns the final `AssistantMessage`.
      After the stream is handed out, errors are never thrown; they become the `error` event
- [ ] Tests: ordering, terminal-event enforcement, producer on another thread, consumer cancellation

### Providers
- [ ] `ai/provider`: `struct Provider { virtual EventStream stream(const Model&, const Context&,
      const StreamOptions&, CancelToken) = 0; virtual std::vector<Model> models() = 0; }`
- [ ] `ai/providers/faux`: scripted responses (list of steps per call), chunked deltas
- [ ] `ai/providers/openai_completions` (text + thinking only for now):
  - request: `model`, `messages` (system as `system` role for Ollama), `stream: true`,
    `stream_options: {include_usage: true}`, `max_tokens`
  - parse chunks: `choices[0].delta.content` → text events, `delta.reasoning` / `reasoning_content` → thinking
    events, `finish_reason` → stop reason (`stop`→stop, `length`→length, `tool_calls`→toolUse, other→error),
    `usage` → `Usage`
  - HTTP error / cancel → `error` event with `StopReason::error` / `aborted`
- [ ] Tests: canned SSE (record one from M2's demo into `test/ai/fixtures/`) → expected event sequence;
      request-body assertions; abort mid-stream → `aborted`

### Print mode
- [ ] `modes/cli_args`: `-p/--print <prompt>`, `--model <provider>/<id>`, `--version`, `--help`
- [ ] `modes/print_mode`: build `Context`, stream, print text deltas as they arrive, print usage at the end
- [ ] Replace the M0 banner in `main.cpp` with mode dispatch

**Read:** `pi:ai/src/api/openai-completions.ts` (`buildParams`, `convertMessages`, the stream loop, `mapStopReason`),
`pi:ai/src/utils/event-stream.ts`, `pig:ai/event_stream.go`, `pig:ai/stream_builder.go`, `pig:ai/openai.go`.

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
- [ ] Tests (faux provider): text-only turn; tool call → result → final text; unknown tool; invalid args;
      tool throws → `is_error` result; event order matches Pi exactly

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
`pi:ai/src/utils/validation.ts`, `pi:coding-agent/src/core/tools/{read,ls,truncate}.ts`,
`pig:agent/tool_execution.go`, `pig:agent/validate*.go`.

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

- [ ] `coding/system_prompt`: Pi's section-based prompt — preamble, `<tools>` (one line per tool),
      `<rules>` (per-tool guidelines, deduplicated), `<project_context>`, `<cwd>`; `SYSTEM.md` replaces and
      `APPEND_SYSTEM.md` appends. Copy the actual strings from `system-prompt.ts` and each tool's
      `promptSnippet`/guidelines
- [ ] `coding/context_files`: for the agent dir and **every ancestor directory from `/` down to cwd**, take the
      first of `AGENTS.override.md`, `AGENTS.md`, `CLAUDE.md`
- [ ] `coding/settings`: `~/.pi/agent/settings.json` (override dir with `PI_CODING_AGENT_DIR`) merged with
      project `.pi/settings.json`: `defaultProvider`, `defaultModel`, `defaultTools`, `compaction.*`, `retry.*`
- [ ] `coding/models_config`: `~/.pi/agent/models.json` custom providers, e.g.
      ```json
      {"providers":{"ollama":{"baseUrl":"http://localhost:11434/v1","api":"openai-completions",
        "apiKey":"ollama","models":[{"id":"qwen3.8:27b-mlx","contextWindow":32768}]}}}
      ```
      `apiKey` values: literal, `$ENV`/`${ENV}`, or `!shell command`
- [ ] `ai/registry`: providers + models by `provider/id`; merges static config with dynamic `/v1/models`
- [ ] Tests: prompt snapshot against a golden file; context-file discovery in a nested temp tree; settings merge
      precedence; env/`!cmd` key resolution

**Read:** `pi:coding-agent/src/core/{system-prompt,resource-loader,settings-manager,model-registry}.ts`,
`pi:coding-agent/docs/{settings,models}.md`.

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
- [ ] `agent/queues`: steering messages (injected after the current tool batch) and follow-up messages (after the
      agent would stop); modes `one-at-a-time` / `all`
- [ ] Retry with exponential backoff on retryable errors (429, 5xx, connection reset); context-overflow detection
      (Ollama: "prompt too long"; see `pi:ai/src/utils/overflow.ts`)
- [ ] Tests: abort mid-stream and mid-tool; parallel results ordering with tools that sleep different amounts;
      steering message lands between turns; retry counts with a flaky fake transport

**Read:** `pi:agent/src/agent.ts`, `pi:ai/src/utils/{retry,overflow}.ts`, `pig:agent/lifecycle.go`,
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
- [ ] `coding/agent_session`: glue object the UI talks to — `prompt()`, `abort()`, current model, event fan-out.
      Modes now depend on this, not on `Agent` directly
- [ ] `tui/app`: agent runs on its worker thread; every agent event is marshalled to the UI thread with
      `screen.Post(...)` and followed by `screen.PostEvent(Event::Custom)` to redraw. **Never touch UI state
      from the agent thread**
- [ ] `tui/transcript`: user, assistant (text + dimmed thinking), and tool blocks (name, args, status,
      collapsed output); live-updates the streaming message
- [ ] `tui/editor`: multi-line input (Enter submits, Shift+Enter/Alt+Enter newline), history Up/Down
- [ ] `tui/footer`: cwd, model, ↑input ↓output tokens, context % (`last input tokens / context_window`)
- [ ] Esc aborts the running turn; spinner while streaming; Ctrl-C twice / Ctrl-D on empty editor exits
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
  - line 1 header `{"type":"session","version":3,"id","timestamp","cwd"}`
  - entries `{type, id(8 hex), parentId, timestamp, ...}`: `message`, `model_change`, `thinking_level_change`,
    `compaction`, `session_info`; this makes a **tree**, and the current leaf is the position
  - append each entry as it happens (crash-safe); build `Context` by walking leaf → root
- [ ] CLI: `--continue/-c` (latest for cwd), `--resume/-r` (picker), `--session <path|id>`, `--no-session`
- [ ] Slash commands: `/new`, `/resume` (FTXUI list of sessions with first message + date), `/session` (info),
      `/name`
- [ ] Tests: write → reload → identical `Context`; tree with two branches → correct path from each leaf;
      truncated last line (crash) still loads

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
- [ ] Tests: cut-point selection on crafted histories (including split turns); context rebuild after 1 and 2
      compactions; overflow → compact → retry with the faux provider

**Read:** `pi:coding-agent/docs/compaction.md`, `pi:coding-agent/src/core/compaction/{compaction,utils}.ts`.

**Demo:** set a small `contextWindow` in models.json, work for a while, and watch it compact and keep going.

---

## M12 — TUI v2: polish

- [ ] `tui/markdown`: Markdown → FTXUI elements (headings, bold/italic/code spans, lists, fenced code blocks
      in a border, block quotes, tables optional). Either hand-write a block/inline parser (good exercise) or
      add md4c via FetchContent
- [ ] Edit-tool diff rendering (red/green lines from `details`), bash output tail with expand/collapse (Ctrl-O),
      thinking show/hide (Ctrl-T)
- [ ] `coding/slash_commands` registry + dispatcher; `/model` selector (Ctrl-L), `/hotkeys`, `/copy`, `/export`
- [ ] `tui/selectors`: generic filterable list overlay (`Modal`) reused by model/session pickers
- [ ] Autocomplete: `/command` names, `@path` fuzzy file search (port `pi:tui/src/fuzzy.ts`), Tab path
      completion
- [ ] `tui/keybindings`: action → key registry with `~/.pi/agent/keybindings.json` overrides (no hard-coded keys)
- [ ] `tui/theme`: theme roles (accent, muted, error, diff add/remove, ...) → FTXUI decorators; dark/light
- [ ] Tests: markdown snapshots; fuzzy scorer; keybinding override parsing

**Read:** `pi:tui/src/components/{editor,markdown}.ts`, `pi:tui/src/{autocomplete,fuzzy}.ts`,
`pi:coding-agent/src/core/{slash-commands,keybindings}.ts`, `pi:coding-agent/docs/keybindings.md`.

---

## M13 — Search tools + resources

- [ ] `tools/grep {pattern, path?, glob?, ignoreCase?, literal?, context?, limit?=100}` via
      `rg --json --line-number --color=never --hidden`; lines cut at 500 chars
- [ ] `tools/find {pattern, path?, limit?=1000}` via `fd --glob --color=never --hidden`
- [ ] `defaultTools` setting (`+grep`, `-bash`) and the read-only tool set
- [ ] `coding/skills`: discover `skills/**/SKILL.md` (frontmatter `name`, `description`), list them in the system
      prompt as `<available_skills>`; `/skill:name` forces one
- [ ] `coding/prompt_templates`: `prompts/*.md` → `/name args` with `$1`, `$@`, `$ARGUMENTS`, `${1:-default}`
- [ ] `!cmd` (run, show, send output to the model) and `!!cmd` (run, don't send)
- [ ] Tests: rg/fd output parsing from fixtures; frontmatter parsing; template substitution table

**Read:** `pi:coding-agent/src/core/tools/{grep,find}.ts`, `pi:coding-agent/src/core/{skills,prompt-templates}.ts`,
`pi:coding-agent/docs/skills.md`.

**Demo:** drop this repo's `.claude/skills/format-cpp` into `~/.pi/agent/skills/` and ask PiCPP to format a file.

---

## M14 — JSON and RPC modes

- [ ] `--mode json`: every agent event as one JSON line on stdout (`pi:coding-agent/docs/json.md`)
- [ ] `--mode rpc`: JSONL commands on stdin (`prompt`, `abort`, `get_state`, `set_model`, ...), responses
      `{type:"response", id, command, success, data}` + events on stdout; split on `\n` only
- [ ] End-to-end tests: spawn `pi --mode rpc` with a faux model (e.g. `PI_TEST_FAUX=1`) and script a whole
      conversation (PiG's `test-faux` idea)

**Read:** `pi:coding-agent/docs/{json,rpc}.md`, `pi:coding-agent/src/modes/{json-event.ts,rpc/}`.

---

## M15 — More providers

- [ ] `ai/providers/ollama_native`: `POST /api/chat` with NDJSON streaming (`http/ndjson`), `options.num_ctx`
      from the model's `contextWindow`, `think`, native `tool_calls`. This is a **deliberate deviation** from
      Pi; document why (context length control)
- [ ] `ai/transform`: cross-provider replay — drop or convert foreign thinking blocks, synthesize
      `"No result provided"` results for orphaned tool calls, skip errored/aborted assistant messages
- [ ] `ai/providers/anthropic_messages` (SSE, content blocks, thinking signatures, cache control)
- [ ] `openai-responses` API (optional)
- [ ] `coding/auth`: `~/.pi/agent/auth.json` API keys + env vars (`ANTHROPIC_API_KEY`, `OPENAI_API_KEY`);
      `/login` with an API key (skip OAuth)
- [ ] `ai/cost`: per-model $/Mtok → `usage.cost`, shown in the footer
- [ ] Tests: canned SSE/NDJSON per provider; switching models mid-session replays history correctly

**Read:** `pi:ai/src/api/{anthropic-messages,transform-messages}.ts`, `pi:ai/src/models.ts` (`calculateCost`),
`pi:coding-agent/src/core/auth-storage.ts`.

**Demo:** start a task with a local model, `/model` → a cloud model mid-session, and continue.

---

## M16 — Stretch goals

- [ ] `/tree` navigation inside a session, `/fork` (new session from an earlier message), `/clone`, branch summaries
- [ ] Thinking levels (`off`…`high`) mapped per provider; Shift+Tab cycles them
- [ ] Images: `read` on png/jpg returns `ImageContent`; paste images in the editor
- [ ] Extensions as **subprocesses** speaking JSON-RPC (PiG's approach, `pig:coding/extension/host/subprocess/`):
      register tools and commands from any language
- [ ] MCP client (stdio transport) exposing MCP tools as `AgentTool`s
- [ ] Sub-agents / plan mode as extensions (Pi deliberately leaves them out of the core; build them to see why)

---

## Appendix — Milestone dependency summary

```
M1 ─► M2 ─► M3 ─► M4 ─► M5 ─► M6 ─► M7 ─► M8 ─► M9 ─► M10 ─► M11 ─► M12
                               │                  └──► M14     └──► M13
                               └──────────────────────────────────► M15 (any time after M8)
```
