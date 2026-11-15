#include <ai/providers/openai_completions.h>

#include <algorithm>
#include <cassert>

#include <http/sse.h>
#include <json/parse.h>
#include <json/write.h>
#include <util/language.h>

namespace pi::ai::providers {

Model make_ollama_model(str id, str base_url, const ModelDefaults& defaults) {
  // TODO(M4): take context_window / max_tokens from `defaults`.
  (void)defaults;
  //TODO: is there a better way to strip trailing slash ?
  if (base_url.size() > 0 and base_url.back() == '/')
    base_url = base_url.substr(0, base_url.size()-1);
  return Model{
    /*id*/id,
    /*name*/id,
    /*api*/"openai-completions",
    /*provider*/"ollama",
    base_url,
    /*reasoning*/true,
    /*context_window*/32768,
    /*max_tokens*/8192,
    ollama_compat()
  };
}

s::expected<vec<Model>, str> parse_models_response(
    strv body,
    strv base_url,
    const ModelDefaults& defaults
){
  // TODO(M4): see header.
  (void)body; (void)base_url; (void)defaults;
  return s::unexpected(str("parse_models_response: not implemented"));
}

s::expected<vec<Model>, str> list_models(
    http::HttpTransport& transport,
    strv base_url,
    const ModelDefaults& defaults,
    const util::CancelToken& cancel,
    const http::HttpHeaders& extra_headers
){
  // TODO(M4): see header.
  (void)transport; (void)base_url; (void)defaults; (void)cancel; (void)extra_headers;
  return s::unexpected(str("list_models: not implemented"));
}

json::Value convert_messages(const Model& model, const Context& context) {
  json::Array array;

  if (not context.system_prompt.empty()){
    if (model.compat.supports_developer_role and model.reasoning)
      array.push_back(json::Value(json::Object{{
          s::pair<str,json::Value>{"role", "developer"},
          s::pair<str,json::Value>{"content", context.system_prompt},
      }}));
    else
      array.push_back(json::Value(json::Object{{
          s::pair<str,json::Value>{"role", "system"},
          s::pair<str,json::Value>{"content", context.system_prompt},
      }}));
  }
  for (const Message& msg : context.messages){
    s::visit(util::Overloaded {
      [&array](const UserMessage& m){
        json::Object obj;
        obj.set("role", json::Value("user"));
        bool all_text = s::all_of(m.content.begin(), m.content.end(), [](const UserContent& c){
          return s::holds_alternative<TextContent>(c);
        });
        if (all_text){
          str text;
          for (size_t i = 0; i < m.content.size(); ++i){
            if (i > 0) text += "\n";
            text += s::get<TextContent>(m.content[i]).text;
          }
          obj.set("content", json::Value(text));
          array.push_back(json::Value(obj));
          return;
        }
        json::Array carray;
        for (const UserContent& content : m.content){
          carray.push_back(s::visit(util::Overloaded {
            [](const TextContent& c) -> json::Value {
              return json::Value(json::Object{{
                s::pair<str,json::Value>{"type", "text"},
                s::pair<str,json::Value>{"text", c.text},
              }});
            },
            [](const ImageContent& c) -> json::Value {
              return json::Value(json::Object{{
                s::pair<str,json::Value>{"type", "image_url"},
                s::pair<str,json::Value>{"image_url", json::Value(json::Object{{
                    s::pair<str,json::Value>{"url", s::format("data:{};base64,{}", c.mime_type, c.data)}
                }})},
              }});
            },
          }, content));
        }
        obj.set("content", json::Value(carray));
        array.push_back(json::Value(obj));
      },
      [&array](const AssistantMessage& m){
        // thinking is dropped; tool calls come in M5
        str text = text_of(m);
        if (text.empty()) return;
        json::Object obj;
        obj.set("role", json::Value("assistant"));
        obj.set("content", json::Value(text));
        array.push_back(json::Value(obj));
      },
      [&array](const ToolResultMessage&){/*TODO*/(void)array;},
    }, msg);
  }
  return json::Value(array);
}

json::Value build_request(const Model& model, const Context& context, const StreamOptions& options) {
  json::Object obj;
  obj.set("model", json::Value(model.id));
  obj.set("messages", convert_messages(model, context));
  obj.set("stream", json::Value(true));

  if (model.compat.supports_usage_in_streaming){
    json::Object opt;
    opt.set("include_usage", json::Value(true));
    obj.set("stream_options", json::Value(opt));
  }
  // compat.max_tokens_field names the key: "max_tokens" (Ollama) or "max_completion_tokens" (OpenAI)
  int64_t max_tokens = options.max_tokens.value_or(0) > 0 ? *options.max_tokens : model.max_tokens;
  if (max_tokens > 0)
    obj.set(model.compat.max_tokens_field, json::Value(max_tokens));
  if (options.temperature.has_value())
    obj.set("temperature", json::Value(*options.temperature));

  return json::Value(obj);
}

StopReason map_stop_reason(strv finish_reason) {
  if (finish_reason == "stop")          return StopReason::stop;
  if (finish_reason == "length")        return StopReason::length;
  if (finish_reason == "tool_calls")    return StopReason::toolUse;
  if (finish_reason == "function_call") return StopReason::toolUse;
  return StopReason::error;
}

namespace {
// integer token count; integral doubles (e.g. 12.0) are accepted, anything else is 0
int64_t token_count(const json::Value* value) {
  if (value == nullptr) return 0;
  if (value->is_int()) return *value->as_int();
  if (value->is_double()){
    double d = *value->as_double();
    if (d == static_cast<double>(static_cast<int64_t>(d))) return static_cast<int64_t>(d);
  }
  return 0;
}
} // namespace

Usage parse_usage(const json::Value& usage) {
  if (not usage.is_object()) return Usage();
  const json::Object& obj = *usage.as_object();
  const json::Value* prompt_tokens_v = obj.find("prompt_tokens");
  const json::Value* completion_tokens_v = obj.find("completion_tokens");
  const json::Value* total_tokens_v = obj.find("total_tokens");
  const json::Value* cached_tokens_v = nullptr;
  const json::Value* prompt_tokens_details_v = obj.find("prompt_tokens_details");
  if (prompt_tokens_details_v and prompt_tokens_details_v->is_object())
    cached_tokens_v = prompt_tokens_details_v->as_object()->find("cached_tokens");

  int64_t cache_read = token_count(cached_tokens_v);
  int64_t input = token_count(prompt_tokens_v) - cache_read;
  int64_t output = token_count(completion_tokens_v);
  int64_t total_tokens = total_tokens_v and total_tokens_v->is_number() ?
    token_count(total_tokens_v) :
    input + output + cache_read;
  return Usage{
    /*input*/input,
    /*output*/output,
    /*cache_read*/cache_read,
    /*cache_write*/0,
    /*total*/total_tokens,
    /*cost*/Cost(),
  };
}

ChunkDecoder::ChunkDecoder(StreamBuilder& builder)
  : mBuilder(builder), mFinishReason(s::nullopt), mStarted(false) {}

void ChunkDecoder::on_data(strv payload) {
  if (not mStarted){
    mBuilder.start();
    mStarted = true;
  }

  if (mBuilder.finished())
    return;

  if (payload == "[DONE]"){
    mBuilder.finish(mFinishReason.has_value() ? map_stop_reason(*mFinishReason) : StopReason::stop);
    return;
  }

  auto choices = json::parse(payload);
  if (not choices.has_value()){
    mBuilder.fail(StopReason::error, s::format("invalid chunk: {}", choices.error().message));
    return;
  }

  const json::Object* choice_obj = (*choices).as_object();
  if (choice_obj == nullptr){
    mBuilder.fail(StopReason::error, "invalid chunk: json format");
    return;
  }

  const json::Value* error_val = (*choice_obj).find("error");
  if (error_val != nullptr){
    const json::Object* error_obj = (*error_val).as_object();
    if (error_obj == nullptr){
      mBuilder.fail(StopReason::error, "invalid chunk: error json format");
      return;
    }
    const json::Value* message_val = (*error_obj).find("message");
    if (message_val == nullptr){
      mBuilder.fail(StopReason::error, "invalid chunk: error json format");
      return;
    }
    opt<strv> message = (*message_val).as_string();
    mBuilder.fail(StopReason::error, message.has_value() ? str(*message) : "");
    return;
  }

  // OpenAI servers send "usage": null on every chunk; only the final one is an object
  const json::Value* usage_val = (*choice_obj).find("usage");
  if (usage_val != nullptr and not (*usage_val).is_null())
    mBuilder.set_usage(parse_usage(*usage_val));

  const json::Value* choices_val = (*choice_obj).find("choices");
  if (choices_val == nullptr or not (*choices_val).is_array()){
    mBuilder.fail(StopReason::error, "invalid chunk: choices json format");
    return;
  }
  const json::Array* choices_array = (*choices_val).as_array();
  if ((*choices_array).size() == 0 or not (*choices_array)[0].is_object())
    return;
  const json::Object* choice0 = (*choices_array)[0].as_object();

  const json::Value* fr_val = (*choice0).find("finish_reason");
  if (fr_val != nullptr and (*fr_val).is_string() and not (*(*fr_val).as_string()).empty())
    mFinishReason = str(*(*fr_val).as_string());

  const json::Value* delta_val = (*choice0).find("delta");
  if (delta_val == nullptr or not (*delta_val).is_object())
    return;
  const json::Object* delta_obj = (*delta_val).as_object();

  // first present, non-empty reasoning field → thinking; then content → text
  for (strv key : {"reasoning_content", "reasoning", "reasoning_text"}){
    const json::Value* val = (*delta_obj).find(key);
    if (val == nullptr or not (*val).is_string() or (*(*val).as_string()).empty()) continue;
    mBuilder.thinking_delta(*(*val).as_string());
    break;
  }
  if (const json::Value* val = (*delta_obj).find("content"); val != nullptr and (*val).is_string())
    mBuilder.text_delta(*(*val).as_string());
}

void ChunkDecoder::on_end() {
  if (not mStarted) return;
  if (not mBuilder.finished()){
    if (mFinishReason.has_value())
      mBuilder.finish(map_stop_reason(*mFinishReason));
    else
      mBuilder.fail(StopReason::error, "stream ended without finish_reason");
  }
}

OpenAiCompletionsProvider::OpenAiCompletionsProvider(sptr<http::HttpTransport> transport, OpenAiCompletionsOptions options)
  : mTransport(s::move(transport))
  , mOptions(s::move(options)) {}

strv OpenAiCompletionsProvider::id() const {
  return mOptions.provider_id;
}

vec<Model> OpenAiCompletionsProvider::models() {
  // TODO(M4): see header.
  return {};
}

EventStream OpenAiCompletionsProvider::stream(
    const Model& model,
    const Context& context,
    const StreamOptions& options,
    util::CancelToken cancel
){
  EventStream stream;
  str url = s::format("{}/chat/completions", model.base_url);
  str body = json::to_json(build_request(model, context, options));
  http::HttpHeaders headers = {
    http::HttpHeader{"Content-Type", "application/json"},
    http::HttpHeader{"Accept", "text/event-stream"},
  };
  if (not options.api_key.empty())
    headers.emplace_back("Authorization", s::format("Bearer {}", options.api_key));
  for (const http::HttpHeader& header : mOptions.extra_headers){
    headers.push_back(header);
  }

  stream.adopt_worker(s::jthread([stream, model, cancel, url, headers, body, transport=mTransport, options=mOptions]() mutable {
    // worker's own copy keeps the stream alive
    AssistantMessage msg;
    msg.api = OPENAI_COMPLETIONS_API;
    msg.provider = options.provider_id;
    msg.model = model.id;
    StreamBuilder builder(stream, s::move(msg));
    ChunkDecoder chunks(builder);
    http::SseDecoder sse;
    auto status = transport->post_stream(url, headers, body, [&](strv bytes){
      for (const http::SseEvent& event : sse.feed(bytes))
        chunks.on_data(event.data);
    }, cancel);
    if (not status.has_value()){
      switch (status.error().kind){
      break; case http::HttpErrorKind::Cancelled:
        builder.fail(StopReason::aborted, "request aborted");
      break; case http::HttpErrorKind::Status:
        builder.fail(StopReason::error, s::format("HTTP {}: {}", status.error().status, status.error().message));
      break; case http::HttpErrorKind::InvalidRequest:
             case http::HttpErrorKind::Connection:
             case http::HttpErrorKind::Timeout:
             case http::HttpErrorKind::Other:
        builder.fail(StopReason::error, status.error().message);
      break; default: assert(false);
      }
    }
    chunks.on_end();
  }));
  return stream;
}

}  // namespace pi::ai::providers
