#include <ai/stream_builder.h>

#include <util/strings.h>
#include <json/parse.h>

#include <cassert>

namespace pi::ai {

namespace {
// Raw tool call arguments to a JSON object: blank text and null are {}, anything
// else must parse to an object.  The error is the reason, without the tool's name.
s::expected<json::Value, str> parse_tool_args(strv raw) {
  if (util::trim(raw).empty()) return json::object_value();
  s::expected<json::Value, json::ParseError> args = json::parse(raw);
  if (not args.has_value()) return s::unexpected(args.error().message);
  if (args->is_null()) return json::object_value();
  if (not args->is_object()) return s::unexpected(str("arguments are not a JSON object"));
  return s::move(*args);
}
} // namespace

StreamBuilder::StreamBuilder(EventStream stream, AssistantMessage initial)
  : mStream(s::move(stream))
  , mMessage(s::move(initial)) {
  if (mMessage.timestamp == 0)
    mMessage.timestamp = now_ms();
  // TODO(M3): stamp mMessage.stop_reason = StopReason::pending (see header).
}

void StreamBuilder::start() {
  if (not mStarted and not mFinished){
    mStream.push(StartEvent());
    mStarted = true;
  }
}

void StreamBuilder::text_delta(strv delta) {
  if (not mStarted or mFinished) return;
  if (delta.empty()) return;

  switch (mOpen){
  break; case Open::None: {
    mMessage.content.push_back(TextContent{(str)delta});
    size_t idx = mMessage.content.size()-1;
    mStream.push(TextStartEvent{idx});
    mStream.push(TextDeltaEvent{idx, (str)delta});
  }
  break; case Open::Text: {
    s::get<TextContent>(mMessage.content.back()).text += delta;
    size_t idx = mMessage.content.size()-1;
    mStream.push(TextDeltaEvent{idx, (str)delta});
  }
  break; case Open::Thinking: {
    str prev_content = s::get<ThinkingContent>(mMessage.content.back()).thinking;
    mMessage.content.push_back(TextContent{(str)delta});
    size_t idx = mMessage.content.size()-1;
    mStream.push(ThinkingEndEvent{idx-1, prev_content});
    mStream.push(TextStartEvent{idx});
    mStream.push(TextDeltaEvent{idx, (str)delta});
  }
  break; case Open::ToolCall: {
    s::expected<json::Value, str> args = parse_tool_args(mToolArgs);
    if (not args.has_value()){
      fail(StopReason::error, "invalid tool call arguments for " + s::get<ToolCall>(mMessage.content.back()).name + ": " + args.error());
      return;
    }
    s::get<ToolCall>(mMessage.content.back()).arguments = *args;
    ToolCall prev_tc = s::get<ToolCall>(mMessage.content.back());
    mMessage.content.push_back(TextContent{(str)delta});
    size_t idx = mMessage.content.size()-1;
    mStream.push(ToolCallEndEvent{idx-1, prev_tc});
    mStream.push(TextStartEvent{idx});
    mStream.push(TextDeltaEvent{idx, (str)delta});
  }
  break; default: assert(false);
  }
  mOpen = Open::Text;
}

void StreamBuilder::thinking_delta(strv delta) {
  if (not mStarted or mFinished) return;
  if (delta.empty()) return;

  switch (mOpen){
  break; case Open::None: {
    mMessage.content.push_back(ThinkingContent{(str)delta});
    size_t idx = mMessage.content.size()-1;
    mStream.push(ThinkingStartEvent{idx});
    mStream.push(ThinkingDeltaEvent{idx, (str)delta});
  }
  break; case Open::Text: {
    str prev_content = s::get<TextContent>(mMessage.content.back()).text;
    mMessage.content.push_back(ThinkingContent{(str)delta});
    size_t idx = mMessage.content.size()-1;
    mStream.push(TextEndEvent{idx-1, prev_content});
    mStream.push(ThinkingStartEvent{idx});
    mStream.push(ThinkingDeltaEvent{idx, (str)delta});
  }
  break; case Open::Thinking: {
    s::get<ThinkingContent>(mMessage.content.back()).thinking += delta;
    size_t idx = mMessage.content.size()-1;
    mStream.push(ThinkingDeltaEvent{idx, (str)delta});
  }
  break; case Open::ToolCall: {
    s::expected<json::Value, str> args = parse_tool_args(mToolArgs);
    if (not args.has_value()){
      fail(StopReason::error, "invalid tool call arguments for " + s::get<ToolCall>(mMessage.content.back()).name + ": " + args.error());
      return;
    }
    s::get<ToolCall>(mMessage.content.back()).arguments = *args;
    ToolCall prev_tc = s::get<ToolCall>(mMessage.content.back());
    mMessage.content.push_back(ThinkingContent{(str)delta});
    size_t idx = mMessage.content.size()-1;
    mStream.push(ToolCallEndEvent{idx-1, prev_tc});
    mStream.push(ThinkingStartEvent{idx});
    mStream.push(ThinkingDeltaEvent{idx, (str)delta});
  }
  break; default: assert(false);
  }
  mOpen = Open::Thinking;
}

void StreamBuilder::toolcall_start(str id, str name) {
  if (not mStarted or mFinished) return;

  switch (mOpen){
  break; case Open::None:; // do nothing
  break; case Open::Text: {
    strv prev_content = s::get<TextContent>(mMessage.content.back()).text;
    size_t idx = mMessage.content.size()-1;
    mStream.push(TextEndEvent{idx, (str)prev_content});
  }
  break; case Open::Thinking: {
    strv prev_content = s::get<ThinkingContent>(mMessage.content.back()).thinking;
    size_t idx = mMessage.content.size()-1;
    mStream.push(ThinkingEndEvent{idx, (str)prev_content});
  }
  break; case Open::ToolCall: {
    s::expected<json::Value, str> args = parse_tool_args(mToolArgs);
    if (not args.has_value()){
      fail(StopReason::error, "invalid tool call arguments for " + s::get<ToolCall>(mMessage.content.back()).name + ": " + args.error());
      return;
    }
    s::get<ToolCall>(mMessage.content.back()).arguments = *args;
    ToolCall prev_tc = s::get<ToolCall>(mMessage.content.back());
    size_t idx = mMessage.content.size()-1;
    mStream.push(ToolCallEndEvent{idx, prev_tc});
  }
  break; default: assert(false);
  }
  mMessage.content.push_back(ToolCall{id, name});
  size_t idx = mMessage.content.size()-1;
  mStream.push(ToolCallStartEvent{idx, id, name});
  mToolArgs.clear();
  mOpen = Open::ToolCall;
}

void StreamBuilder::toolcall_delta(strv json_fragment) {
  if (not mStarted or mFinished) return;
  if (mOpen != Open::ToolCall) return;
  if (json_fragment.empty()) return;

  size_t idx = mMessage.content.size()-1;
  mToolArgs += json_fragment;
  mStream.push(ToolCallDeltaEvent{idx, (str)json_fragment});
}

void StreamBuilder::set_usage(const Usage& usage) {
  if (mFinished) return;
  mMessage.usage = usage;
}

void StreamBuilder::finish(StopReason reason) {
  if (mFinished) return;
  // TODO(M3): also route pending and deferred to fail() (as StopReason::error; see header).
  if (reason == StopReason::error or reason == StopReason::aborted){
    fail(reason, "stream finished with reason " + str(to_string(reason)));
    return;
  }

  switch (mOpen){
  break; case Open::None:; // do nothing
  break; case Open::Text: {
    strv prev_content = s::get<TextContent>(mMessage.content.back()).text;
    size_t idx = mMessage.content.size()-1;
    mStream.push(TextEndEvent{idx, (str)prev_content});
  }
  break; case Open::Thinking: {
    strv prev_content = s::get<ThinkingContent>(mMessage.content.back()).thinking;
    size_t idx = mMessage.content.size()-1;
    mStream.push(ThinkingEndEvent{idx, (str)prev_content});
  }
  break; case Open::ToolCall: {
    s::expected<json::Value, str> args = parse_tool_args(mToolArgs);
    if (not args.has_value()){
      fail(StopReason::error, "invalid tool call arguments for " + s::get<ToolCall>(mMessage.content.back()).name + ": " + args.error());
      return;
    }
    s::get<ToolCall>(mMessage.content.back()).arguments = *args;
    ToolCall prev_tc = s::get<ToolCall>(mMessage.content.back());
    size_t idx = mMessage.content.size()-1;
    mStream.push(ToolCallEndEvent{idx, prev_tc});
  }
  break; default: assert(false);
  }
  mFinished = true;
  mMessage.stop_reason = reason;
  mStream.push(DoneEvent{reason, mMessage});
}

void StreamBuilder::fail(StopReason reason, str message) {
  if (mFinished) return;

  mFinished = true;

  switch (reason){
  break; case StopReason::stop:
         case StopReason::length:
         case StopReason::toolUse:
         case StopReason::error:
         case StopReason::pending:
         case StopReason::deferred:
    mMessage.stop_reason = StopReason::error;
  break; case StopReason::aborted:
    mMessage.stop_reason = StopReason::aborted;
  }
  mMessage.error_message = message;
  mStream.push(ErrorEvent{mMessage.stop_reason, mMessage});
}

const AssistantMessage& StreamBuilder::message() const {
  return mMessage;
}

bool StreamBuilder::finished() const {
  return mFinished;
}

}  // namespace pi::ai
