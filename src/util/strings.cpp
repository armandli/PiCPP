#include <util/strings.h>

#include <sstream>

#include <util/aliases.h>

namespace pi::util {

vec<strv> split(strv sv, char delim) {
  vec<strv> ret;
  size_t sidx = 0;
  size_t eidx = sv.find(delim, sidx);
  while (eidx != sv.npos){
    ret.emplace_back(sv.subview(sidx, eidx - sidx));

    sidx = eidx + 1;
    eidx = sv.find(delim, sidx);
  }
  ret.emplace_back(sv.subview(sidx));
  return ret;
}

vec<strv> split_lines(strv sv) {
  vec<strv> ret;
  size_t sidx = 0;
  size_t eidx = sv.find_first_of("\r\n", sidx);
  while (eidx != sv.npos){
    if (sv[eidx] == '\r' and (eidx >= sv.size()-1 or sv[eidx+1] != '\n')){
      eidx = sv.find_first_of("\r\n", eidx+1);
      continue;
    }

    ret.emplace_back(sv.subview(sidx, eidx - sidx));

    if (eidx < sv.size()-1 and sv[eidx] == '\r' and sv[eidx+1] == '\n')
      sidx = eidx + 2;
    else
      sidx = eidx + 1;
    eidx = sv.find_first_of("\r\n", sidx);
  }
  if (sv.subview(sidx).size() > 0)
    ret.emplace_back(sv.subview(sidx));
  return ret;
}

str join(const vec<strv>& parts, strv sep) {
  s::stringstream ss;
  bool is_first = true;
  for (const strv part : parts){
    if (is_first) is_first = false;
    else          ss << sep;
    ss << part;
  }
  return ss.str();
}

str join(const vec<str>& parts, strv sep) {
  s::stringstream ss;
  bool is_first = true;
  for (const str& part : parts){
    if (is_first) is_first = false;
    else          ss << sep;
    ss << part;
  }
  return ss.str();
}

strv trim(strv sv) {
  size_t sidx = sv.find_first_not_of("\t\n\r\f\v ");
  size_t eidx = sv.find_last_not_of("\t\n\r\f\v ");
  if (sidx == strv::npos) return strv{};
  return sv.subview(sidx, eidx - sidx + 1);
}

bool starts_with(strv sv, strv prefix) {
  return sv.starts_with(prefix);
}

bool ends_with(strv sv, strv suffix) {
  return sv.ends_with(suffix);
}

str replace_all(strv text, strv from, strv to) {
  if (from.empty()) return str(text);

  s::stringstream ss;
  size_t sidx = 0;
  size_t eidx = text.find(from);
  while (eidx != text.npos){
    ss << text.subview(sidx, eidx - sidx);
    ss << to;

    sidx = eidx + from.size();
    eidx = text.find(from, sidx);
  }
  ss << text.subview(sidx, text.size() - sidx);
  return ss.str();
}

}  // namespace pi::util
