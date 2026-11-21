#include <ai/types.h>

#include <cassert>
#include <cmath>

#include <util/language.h>

namespace pi::ai {

int64_t now_ms() {
  return sc::duration_cast<sc::milliseconds>(sc::system_clock::now().time_since_epoch()).count();
}

strv to_string(StopReason reason) {
  switch (reason){
  break; case StopReason::stop:    return "stop";
  break; case StopReason::length:  return "length";
  break; case StopReason::toolUse: return "toolUse";
  break; case StopReason::error:   return "error";
  break; case StopReason::aborted: return "aborted";
  // TODO(M3): return "pending" / "deferred".
  break; case StopReason::pending:  return "";
  break; case StopReason::deferred: return "";
  }
  assert(false and "invalid StopReason");
  return "";
}

opt<StopReason> parse_stop_reason(strv text) {
  if (text == "stop")    return StopReason::stop;
  if (text == "length")  return StopReason::length;
  if (text == "toolUse") return StopReason::toolUse;
  if (text == "error")   return StopReason::error;
  if (text == "aborted") return StopReason::aborted;
  // TODO(M3): "pending" and "deferred".
  return s::nullopt;
}

OpenAiCompat ollama_compat() {
  return OpenAiCompat{
    /*supports_developer_role*/     false,
    /*supports_reasoning_effort*/   false,
    /*supports_usage_in_streaming*/ true,
    /*max_tokens_field*/            "max_tokens",
  };
}

str text_of(const AssistantMessage& message) {
  str ret;
  for (const AssistantContent& c : message.content)
    if (s::holds_alternative<TextContent>(c))
      ret += s::get<TextContent>(c).text;
  return ret;
}

// ---------------------------------------------------------------------------
// encode
//
// Shapes (see types.h for each struct's JSON comment):
//   TextContent      {"type":"text","text":...}
//   ThinkingContent  {"type":"thinking","thinking":...}
//   ImageContent     {"type":"image","data":...,"mimeType":...}
//   ToolCall         {"type":"toolCall","id":...,"name":...,"arguments":{...}}
//   Usage            {"input","output","cacheRead","cacheWrite","totalTokens",
//                     "cost":{"input","output","cacheRead","cacheWrite","total"}}
//   Messages         "role" first, then the fields in declaration order
//   StopReason       to_string(reason)
// Emit keys in a fixed order (json::Object preserves insertion order), so
// encode(x) is byte-stable and tests can compare serialized text.
// ---------------------------------------------------------------------------

json::Value encode(const Usage& usage) {
  json::Object cost_obj;
  cost_obj.set("input", json::Value(usage.cost.input));
  cost_obj.set("output", json::Value(usage.cost.output));
  cost_obj.set("cacheRead", json::Value(usage.cost.cache_read));
  cost_obj.set("cacheWrite", json::Value(usage.cost.cache_write));
  cost_obj.set("total", json::Value(usage.cost.total));

  json::Object usage_obj;
  usage_obj.set("input", json::Value(usage.input));
  usage_obj.set("output", json::Value(usage.output));
  usage_obj.set("cacheRead", json::Value(usage.cache_read));
  usage_obj.set("cacheWrite", json::Value(usage.cache_write));
  usage_obj.set("totalTokens", json::Value(usage.total_tokens));
  usage_obj.set("cost", json::Value(s::move(cost_obj)));

  json::Value usage_val(s::move(usage_obj));
  return usage_val;
}

json::Value encode(const UserContent& content) {
  return s::visit(util::Overloaded {
    [](const TextContent& c) -> json::Value {
      json::Object cobj;
      cobj.set("type", json::Value("text"));
      cobj.set("text", json::Value(c.text));
      return json::Value(s::move(cobj));
    },
    [](const ImageContent& c) -> json::Value {
      json::Object cobj;
      cobj.set("type", json::Value("image"));
      cobj.set("data", json::Value(c.data));
      cobj.set("mimeType", json::Value(c.mime_type));
      return json::Value(s::move(cobj));
    },
  }, content);
}

json::Value encode(const AssistantContent& content) {
  return s::visit(util::Overloaded {
    [](const TextContent& c) -> json::Value {
      json::Object cobj;
      cobj.set("type", json::Value("text"));
      cobj.set("text", json::Value(c.text));
      return json::Value(s::move(cobj));
    },
    [](const ThinkingContent& c) -> json::Value {
      json::Object cobj;
      cobj.set("type", json::Value("thinking"));
      cobj.set("thinking", json::Value(c.thinking));
      return json::Value(s::move(cobj));
    },
    [](const ToolCall& c) -> json::Value {
      json::Object cobj;
      cobj.set("type", json::Value("toolCall"));
      cobj.set("id", json::Value(c.id));
      cobj.set("name", json::Value(c.name));
      cobj.set("arguments", c.arguments);
      return json::Value(s::move(cobj));
    },
  }, content);
}

json::Value encode(const Message& message) {
  return s::visit(util::Overloaded {
    [](const UserMessage& m) -> json::Value {
      json::Object mobj;
      mobj.set("role", json::Value("user"));
      json::Array contents;
      for (const UserContent& c : m.content)
        contents.emplace_back(encode(c));
      mobj.set("content", json::Value(s::move(contents)));
      mobj.set("timestamp", json::Value(m.timestamp));
      return json::Value(s::move(mobj));
    },
    [](const AssistantMessage& m) -> json::Value {
      json::Object mobj;
      mobj.set("role", json::Value("assistant"));
      json::Array contents;
      for (const AssistantContent& c : m.content)
        contents.emplace_back(encode(c));
      mobj.set("content", json::Value(s::move(contents)));
      mobj.set("api", json::Value(m.api));
      mobj.set("provider", json::Value(m.provider));
      mobj.set("model", json::Value(m.model));
      mobj.set("usage", encode(m.usage));
      mobj.set("stopReason", json::Value(to_string(m.stop_reason)));
      if (not m.error_message.empty())
        mobj.set("errorMessage", json::Value(m.error_message));
      mobj.set("timestamp", json::Value(m.timestamp));
      return json::Value(s::move(mobj));
    },
    [](const ToolResultMessage& m) -> json::Value {
      json::Object mobj;
      mobj.set("role", json::Value("toolResult"));
      mobj.set("toolCallId", json::Value(m.tool_call_id));
      mobj.set("toolName", json::Value(m.tool_name));
      json::Array contents;
      for (const UserContent& c : m.content)
        contents.emplace_back(encode(c));
      mobj.set("content", json::Value(s::move(contents)));
      mobj.set("isError", json::Value(m.is_error));
      mobj.set("timestamp", json::Value(m.timestamp));
      return json::Value(s::move(mobj));
    },
    [](const SystemMessage& m) -> json::Value {
      // TODO(M3): see SystemMessage in types.h.  Keys in declaration order:
      // role, content (string), sections (object; null for nullopt; omitted
      // when empty), toolsAdded (encode(Tool) each; omitted when empty),
      // toolsRemoved ([{"name":...}]; omitted when empty), timestamp.
      (void)m;
      json::Object mobj;
      mobj.set("role", json::Value("system"));
      return json::Value(s::move(mobj));
    },
  }, message);
}

json::Value encode(const Model& model) {
  json::Object mobj;
  mobj.set("id", json::Value(model.id));
  mobj.set("name", json::Value(model.name));
  mobj.set("api", json::Value(model.api));
  mobj.set("provider", json::Value(model.provider));
  mobj.set("baseUrl", json::Value(model.base_url));
  mobj.set("reasoning", json::Value(model.reasoning));
  mobj.set("contextWindow", json::Value(model.context_window));
  mobj.set("maxTokens", json::Value(model.max_tokens));

  json::Object compat_obj;
  compat_obj.set("supportsDeveloperRole", json::Value(model.compat.supports_developer_role));
  compat_obj.set("supportsReasoningEffort", json::Value(model.compat.supports_reasoning_effort));
  compat_obj.set("supportsUsageInStreaming", json::Value(model.compat.supports_usage_in_streaming));
  compat_obj.set("maxTokensField", json::Value(model.compat.max_tokens_field));
  // TODO(M3): "supportsMidConvoSystemMessages" after maxTokensField (update the Encode.Model test).
  mobj.set("compat", json::Value(s::move(compat_obj)));

  return json::Value(s::move(mobj));
}

json::Value encode(const Tool& tool) {
  json::Object tobj;
  tobj.set("name", json::Value(tool.name));
  tobj.set("description", json::Value(tool.description));
  tobj.set("parameters", tool.parameters);
  return json::Value(s::move(tobj));
}

json::Value encode(const Context& context) {
  json::Object cobj;

  if (not context.system_prompt.empty())
    cobj.set("systemPrompt", json::Value(context.system_prompt));

  json::Array messages;
  for (const Message& m : context.messages)
    messages.emplace_back(encode(m));
  cobj.set("messages", json::Value(s::move(messages)));

  json::Array tools;
  for (const Tool& t : context.tools)
    tools.emplace_back(encode(t));
  if (tools.size() > 0)
    cobj.set("tools", json::Value(s::move(tools)));

  return json::Value(s::move(cobj));
}

// ---------------------------------------------------------------------------
// decode
//
// Rules:
//  - the input must be an object; dispatch on "type" (content) or "role" (message)
//  - required string fields: text/thinking/data/mimeType/id/name, role, toolCallId
//  - missing optional fields keep the struct default (e.g. no "usage" → Usage{})
//  - a wrong JSON type for a present field is an error ("usage.input: expected integer")
//  - UserMessage / ToolResultMessage "content" may be a string → one TextContent
//  - stopReason must parse with parse_stop_reason
//  - integers may arrive as doubles from other writers (e.g. 12.0); accept when integral
// Every error is "<path>: <problem>", where path is rooted at the decoded type
// ("message.content[1].text", "usage.cost.total") and decode_context() roots
// at its own fields ("messages[2]: unknown role \"bot\"").
// ---------------------------------------------------------------------------

namespace {
// Field readers return nullopt on success.  An absent key leaves `out`
// untouched unless the field is required.
using Fail = opt<DecodeError>;

strv kind_name(const json::Value& value) {
  switch (value.kind()){
  break; case json::Value::Kind::Null:   return "null";
  break; case json::Value::Kind::Bool:   return "boolean";
  break; case json::Value::Kind::Int:    return "integer";
  break; case json::Value::Kind::Double: return "number";
  break; case json::Value::Kind::String: return "string";
  break; case json::Value::Kind::Array:  return "array";
  break; case json::Value::Kind::Object: return "object";
  }
  return "unknown";
}

str join(strv path, strv key) {
  return path.empty() ? str(key) : s::format("{}.{}", path, key);
}

DecodeError wrong_type(strv path, strv expected, const json::Value& got) {
  return s::format("{}: expected {}, got {}", path, expected, kind_name(got));
}

DecodeError missing(strv path, strv expected) {
  return s::format("{}: missing required {}", path, expected);
}

Fail read_string(str& out, const json::Object& obj, strv key, strv path, bool required = false) {
  const json::Value* v = obj.find(key);
  if (v == nullptr) return required ? Fail(missing(join(path, key), "string")) : s::nullopt;
  if (not v->is_string()) return wrong_type(join(path, key), "string", *v);
  out = str(*v->as_string());
  return s::nullopt;
}

Fail read_int(int64_t& out, const json::Object& obj, strv key, strv path) {
  const json::Value* v = obj.find(key);
  if (v == nullptr) return s::nullopt;
  if (v->is_int()){
    out = *v->as_int();
    return s::nullopt;
  }
  if (v->is_double()){
    double d = *v->as_double();
    // [-2^63, 2^63) is exactly the range that converts to int64_t without UB.
    if (d == s::trunc(d) and d >= -0x1p63 and d < 0x1p63){
      out = static_cast<int64_t>(d);
      return s::nullopt;
    }
    return s::format("{}: expected integer, got {}", join(path, key), d);
  }
  return wrong_type(join(path, key), "integer", *v);
}

Fail read_double(double& out, const json::Object& obj, strv key, strv path) {
  const json::Value* v = obj.find(key);
  if (v == nullptr) return s::nullopt;
  if (v->is_int())         out = static_cast<double>(*v->as_int());
  else if (v->is_double()) out = *v->as_double();
  else                     return wrong_type(join(path, key), "number", *v);
  return s::nullopt;
}

Fail read_bool(bool& out, const json::Object& obj, strv key, strv path) {
  const json::Value* v = obj.find(key);
  if (v == nullptr) return s::nullopt;
  if (not v->is_bool()) return wrong_type(join(path, key), "boolean", *v);
  out = *v->as_bool();
  return s::nullopt;
}

// `out` stays nullptr when the key is absent.
Fail read_object(const json::Object*& out, const json::Object& obj, strv key, strv path) {
  const json::Value* v = obj.find(key);
  if (v == nullptr) return s::nullopt;
  if (not v->is_object()) return wrong_type(join(path, key), "object", *v);
  out = v->as_object();
  return s::nullopt;
}

Fail read_stop_reason(StopReason& out, const json::Object& obj, strv path) {
  str text;
  const json::Value* v = obj.find("stopReason");
  if (v == nullptr) return s::nullopt;
  if (auto e = read_string(text, obj, "stopReason", path)) return e;
  opt<StopReason> reason = parse_stop_reason(text);
  // TODO(M3): list pending and deferred in the expected values.
  if (not reason.has_value())
    return s::format("{}: unknown stop reason \"{}\" (expected stop, length, toolUse, error or aborted)",
                     join(path, "stopReason"), text);
  out = *reason;
  return s::nullopt;
}

// Decodes each element of the array at obj[key]; the first failure names its index.
template <typename T>
Fail read_array(vec<T>& out, const json::Object& obj, strv key, strv path,
                s::expected<T, DecodeError> (*decode)(const json::Value&, strv)) {
  const json::Value* v = obj.find(key);
  if (v == nullptr) return s::nullopt;
  str field = join(path, key);
  if (not v->is_array()) return wrong_type(field, "array", *v);
  const json::Array& items = *v->as_array();
  out.clear();
  for (s::size_t i = 0; i < items.size(); ++i){
    auto item = decode(items[i], s::format("{}[{}]", field, i));
    if (not item.has_value()) return item.error();
    out.emplace_back(s::move(*item));
  }
  return s::nullopt;
}

s::expected<Usage, DecodeError> usage_at(const json::Value& value, strv path) {
  if (not value.is_object()) return s::unexpected(wrong_type(path, "object", value));
  const json::Object& obj = *value.as_object();

  Usage usage;
  if (auto e = read_int(usage.input, obj, "input", path))              return s::unexpected(*e);
  if (auto e = read_int(usage.output, obj, "output", path))            return s::unexpected(*e);
  if (auto e = read_int(usage.cache_read, obj, "cacheRead", path))     return s::unexpected(*e);
  if (auto e = read_int(usage.cache_write, obj, "cacheWrite", path))   return s::unexpected(*e);
  if (auto e = read_int(usage.total_tokens, obj, "totalTokens", path)) return s::unexpected(*e);

  const json::Object* cost = nullptr;
  if (auto e = read_object(cost, obj, "cost", path)) return s::unexpected(*e);
  if (cost != nullptr){
    str cpath = join(path, "cost");
    if (auto e = read_double(usage.cost.input, *cost, "input", cpath))            return s::unexpected(*e);
    if (auto e = read_double(usage.cost.output, *cost, "output", cpath))          return s::unexpected(*e);
    if (auto e = read_double(usage.cost.cache_read, *cost, "cacheRead", cpath))   return s::unexpected(*e);
    if (auto e = read_double(usage.cost.cache_write, *cost, "cacheWrite", cpath)) return s::unexpected(*e);
    if (auto e = read_double(usage.cost.total, *cost, "total", cpath))            return s::unexpected(*e);
  }
  return usage;
}

s::expected<UserContent, DecodeError> user_content_at(const json::Value& value, strv path) {
  if (not value.is_object()) return s::unexpected(wrong_type(path, "object", value));
  const json::Object& obj = *value.as_object();

  str type;
  if (auto e = read_string(type, obj, "type", path, true)) return s::unexpected(*e);

  if (type == "text"){
    TextContent c;
    if (auto e = read_string(c.text, obj, "text", path, true)) return s::unexpected(*e);
    return c;
  }
  if (type == "image"){
    ImageContent c;
    if (auto e = read_string(c.data, obj, "data", path, true))          return s::unexpected(*e);
    if (auto e = read_string(c.mime_type, obj, "mimeType", path, true)) return s::unexpected(*e);
    return c;
  }
  return s::unexpected(s::format("{}: unknown content type \"{}\" (expected text or image)",
                                 join(path, "type"), type));
}

s::expected<AssistantContent, DecodeError> assistant_content_at(const json::Value& value, strv path) {
  if (not value.is_object()) return s::unexpected(wrong_type(path, "object", value));
  const json::Object& obj = *value.as_object();

  str type;
  if (auto e = read_string(type, obj, "type", path, true)) return s::unexpected(*e);

  if (type == "text"){
    TextContent c;
    if (auto e = read_string(c.text, obj, "text", path, true)) return s::unexpected(*e);
    return c;
  }
  if (type == "thinking"){
    ThinkingContent c;
    if (auto e = read_string(c.thinking, obj, "thinking", path, true)) return s::unexpected(*e);
    return c;
  }
  if (type == "toolCall"){
    ToolCall c;
    if (auto e = read_string(c.id, obj, "id", path, true))     return s::unexpected(*e);
    if (auto e = read_string(c.name, obj, "name", path, true)) return s::unexpected(*e);
    if (const json::Value* args = obj.find("arguments")) c.arguments = *args;
    return c;
  }
  return s::unexpected(s::format("{}: unknown content type \"{}\" (expected text, thinking or toolCall)",
                                 join(path, "type"), type));
}

// Like read_array, but Pi also allows "content" to be a plain string.
Fail read_user_content(vec<UserContent>& out, const json::Object& obj, strv path) {
  const json::Value* v = obj.find("content");
  if (v != nullptr and v->is_string()){
    out = {TextContent{str(*v->as_string())}};
    return s::nullopt;
  }
  if (v != nullptr and not v->is_array())
    return wrong_type(join(path, "content"), "string or array", *v);
  return read_array(out, obj, "content", path, user_content_at);
}

s::expected<Message, DecodeError> message_at(const json::Value& value, strv path) {
  if (not value.is_object()) return s::unexpected(wrong_type(path, "object", value));
  const json::Object& obj = *value.as_object();

  str role;
  if (auto e = read_string(role, obj, "role", path, true)) return s::unexpected(*e);

  if (role == "user"){
    UserMessage m;
    if (auto e = read_user_content(m.content, obj, path))         return s::unexpected(*e);
    if (auto e = read_int(m.timestamp, obj, "timestamp", path))   return s::unexpected(*e);
    return m;
  }
  if (role == "assistant"){
    AssistantMessage m;
    if (auto e = read_array(m.content, obj, "content", path, assistant_content_at)) return s::unexpected(*e);
    if (auto e = read_string(m.api, obj, "api", path))                              return s::unexpected(*e);
    if (auto e = read_string(m.provider, obj, "provider", path))                    return s::unexpected(*e);
    if (auto e = read_string(m.model, obj, "model", path))                          return s::unexpected(*e);
    if (const json::Value* usage = obj.find("usage")){
      auto u = usage_at(*usage, join(path, "usage"));
      if (not u.has_value()) return s::unexpected(u.error());
      m.usage = *u;
    }
    if (auto e = read_stop_reason(m.stop_reason, obj, path))                       return s::unexpected(*e);
    if (auto e = read_string(m.error_message, obj, "errorMessage", path))          return s::unexpected(*e);
    if (auto e = read_int(m.timestamp, obj, "timestamp", path))                    return s::unexpected(*e);
    return m;
  }
  if (role == "toolResult"){
    ToolResultMessage m;
    if (auto e = read_string(m.tool_call_id, obj, "toolCallId", path, true)) return s::unexpected(*e);
    if (auto e = read_string(m.tool_name, obj, "toolName", path))            return s::unexpected(*e);
    if (auto e = read_user_content(m.content, obj, path))                    return s::unexpected(*e);
    if (auto e = read_bool(m.is_error, obj, "isError", path))                return s::unexpected(*e);
    if (auto e = read_int(m.timestamp, obj, "timestamp", path))              return s::unexpected(*e);
    return m;
  }
  if (role == "system"){
    // TODO(M3): decode SystemMessage (shape in types.h).
    //  - content: string, or an array of {"type":"text"} blocks joined with "\n"
    //  - sections: object of string | null (null → nullopt), order kept
    //  - toolsAdded: read_array with tool_at (defined below: forward-declare it)
    //  - toolsRemoved: array of {"name": string (required)}
    //  - timestamp
    // Then add "system" to the unknown-role message below.
    return s::unexpected(s::format("{}: system messages are not implemented yet", path));
  }
  return s::unexpected(s::format("{}: unknown role \"{}\" (expected user, assistant or toolResult)", path, role));
}

s::expected<Model, DecodeError> model_at(const json::Value& value, strv path) {
  if (not value.is_object()) return s::unexpected(wrong_type(path, "object", value));
  const json::Object& obj = *value.as_object();

  Model m;
  if (auto e = read_string(m.id, obj, "id", path, true))                 return s::unexpected(*e);
  if (auto e = read_string(m.name, obj, "name", path))                   return s::unexpected(*e);
  if (auto e = read_string(m.api, obj, "api", path))                     return s::unexpected(*e);
  if (auto e = read_string(m.provider, obj, "provider", path))           return s::unexpected(*e);
  if (auto e = read_string(m.base_url, obj, "baseUrl", path))            return s::unexpected(*e);
  if (auto e = read_bool(m.reasoning, obj, "reasoning", path))           return s::unexpected(*e);
  if (auto e = read_int(m.context_window, obj, "contextWindow", path))   return s::unexpected(*e);
  if (auto e = read_int(m.max_tokens, obj, "maxTokens", path))           return s::unexpected(*e);

  const json::Object* compat = nullptr;
  if (auto e = read_object(compat, obj, "compat", path)) return s::unexpected(*e);
  if (compat != nullptr){
    str cpath = join(path, "compat");
    OpenAiCompat& c = m.compat;
    if (auto e = read_bool(c.supports_developer_role, *compat, "supportsDeveloperRole", cpath))         return s::unexpected(*e);
    if (auto e = read_bool(c.supports_reasoning_effort, *compat, "supportsReasoningEffort", cpath))     return s::unexpected(*e);
    if (auto e = read_bool(c.supports_usage_in_streaming, *compat, "supportsUsageInStreaming", cpath))  return s::unexpected(*e);
    if (auto e = read_string(c.max_tokens_field, *compat, "maxTokensField", cpath))                     return s::unexpected(*e);
    // TODO(M3): read_bool "supportsMidConvoSystemMessages" into supports_mid_convo_system_messages.
  }
  return m;
}

s::expected<Tool, DecodeError> tool_at(const json::Value& value, strv path) {
  if (not value.is_object()) return s::unexpected(wrong_type(path, "object", value));
  const json::Object& obj = *value.as_object();

  Tool t;
  if (auto e = read_string(t.name, obj, "name", path, true))         return s::unexpected(*e);
  if (auto e = read_string(t.description, obj, "description", path)) return s::unexpected(*e);
  if (const json::Value* params = obj.find("parameters")) t.parameters = *params;
  return t;
}

s::expected<Context, DecodeError> context_at(const json::Value& value, strv path) {
  if (not value.is_object())
    return s::unexpected(wrong_type(path.empty() ? "context" : path, "object", value));
  const json::Object& obj = *value.as_object();

  Context c;
  if (auto e = read_string(c.system_prompt, obj, "systemPrompt", path))   return s::unexpected(*e);
  if (auto e = read_array(c.messages, obj, "messages", path, message_at)) return s::unexpected(*e);
  if (auto e = read_array(c.tools, obj, "tools", path, tool_at))          return s::unexpected(*e);
  return c;
}
} // namespace

s::expected<Usage, DecodeError> decode_usage(const json::Value& value) {
  return usage_at(value, "usage");
}

s::expected<UserContent, DecodeError> decode_user_content(const json::Value& value) {
  return user_content_at(value, "content");
}

s::expected<AssistantContent, DecodeError> decode_assistant_content(const json::Value& value) {
  return assistant_content_at(value, "content");
}

s::expected<Message, DecodeError> decode_message(const json::Value& value) {
  return message_at(value, "message");
}

s::expected<Model, DecodeError> decode_model(const json::Value& value) {
  return model_at(value, "model");
}

s::expected<Tool, DecodeError> decode_tool(const json::Value& value) {
  return tool_at(value, "tool");
}

s::expected<Context, DecodeError> decode_context(const json::Value& value) {
  return context_at(value, "");
}

}  // namespace pi::ai
