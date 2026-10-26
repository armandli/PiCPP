#include <http/sse.h>

#include <util/strings.h>

namespace pi::http {

namespace {
bool is_bom(unsigned char byte){
  switch (byte){
  break; case 0xEF: case 0xBB: case 0xBF: return true;
  break; default:                         return false;
  }
}
} // namespace

vec<SseEvent> SseDecoder::feed(strv bytes) {
  vec<SseEvent> ret;
  for (char byte : bytes){
    if (not mStarted and is_bom(byte)) continue;
    mStarted = true;

    switch (byte){
    break; case '\r': {
      mLastWasCR = true;
      s::optional<SseEvent> event = process_line();
      if (event.has_value()) ret.push_back(*event);
      mLine.clear();
    }      
    break; case '\n': {
      if (not mLastWasCR){
        s::optional<SseEvent> event = process_line();
        if (event.has_value()) ret.push_back(*event);
        mLine.clear();
      }
      mLastWasCR = false;
    }
    break; default:
      mLastWasCR = false;
      mLine.push_back(byte);
    }
  }
  return ret;
}

s::optional<SseEvent> SseDecoder::process_line(){
  if (mLine.empty()){
    if (mData.empty()){
      mEventType.clear();
      return s::nullopt;
    }

    SseEvent event;
    if (not mEventType.empty())
      event.event = s::move(mEventType);
    if (not mLastId.empty())
      event.id = mLastId;
    mData.pop_back();
    event.data = s::move(mData);
    mEventType.clear();
    mData.clear();
    return event;
  }
  if (mLine.starts_with(':')) return s::nullopt;

  size_t cpos = mLine.find_first_of(':');
  strv line = mLine;
  strv field = line.substr(0, cpos);
  strv value = cpos == mLine.npos ? strv{} : line.substr(cpos+1);
  if (value.starts_with(' ')) value.remove_prefix(1);

  if      (field == "event") mEventType = value;
  else if (field == "data")  { mData += value; mData += '\n'; }
  else if (field == "id" and value.find('\0') == strv::npos) mLastId = value;
  return s::nullopt;
}

}  // namespace pi::http
