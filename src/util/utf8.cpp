#include <util/utf8.h>

#include <algorithm>
#include <iterator>

#include <util/aliases.h>
#include <util/width_tables.h>

namespace pi::util {

namespace {

// Width rules follow Markus Kuhn's wcwidth.c (http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c);
// the tables in width_tables.h are regenerated from current Unicode data.

using detail::Interval;

template <s::size_t N>
bool in_table(char32_t cp, const Interval (&table)[N]) {
  if (cp < table[0].first or cp > table[N - 1].last) return false;
  const Interval* it = s::lower_bound(s::begin(table), s::end(table), cp,
    [](const Interval& iv, char32_t c){ return iv.last < c; });
  return it != s::end(table) and it->first <= cp;
}

bool is_control(char32_t cp) {
  return cp < 0x20 or (cp >= 0x7F and cp < 0xA0); // C0/C1 control characters and DEL
}

bool is_regional_indicator(char32_t cp) {
  return cp >= 0x1F1E6 and cp <= 0x1F1FF;
}

bool is_skin_tone_modifier(char32_t cp) {
  return cp >= 0x1F3FB and cp <= 0x1F3FF;
}

constexpr char32_t ZWJ = 0x200D;  // ZERO WIDTH JOINER
constexpr char32_t VS16 = 0xFE0F; // VARIATION SELECTOR-16, requests emoji presentation

// number of terminal cells a single code point occupies, ignoring its neighbours
s::size_t code_point_width(char32_t cp) {
  if (is_control(cp)) return 0;
  if (in_table(cp, detail::ZERO_WIDTH)) return 0;
  if (in_table(cp, detail::WIDE)) return 2;
  return 1;
}

} // namespace

opt<DecodedCodePoint> decode_utf8(strv text, s::size_t pos) {
  if (pos >= text.size()) return s::nullopt;

  unsigned char c1 = text[pos] & 0b11111000;
  switch (c1){
  break; case 0b00000000: case 0b00001000: case 0b00010000: case 0b00011000:
         case 0b00100000: case 0b00101000: case 0b00110000: case 0b00111000:
         case 0b01000000: case 0b01001000: case 0b01010000: case 0b01011000:
         case 0b01100000: case 0b01101000: case 0b01110000: case 0b01111000: {
    return DecodedCodePoint{static_cast<unsigned char>(text[pos]), pos + 1};
  }
  break; case 0b10000000: case 0b10001000: case 0b10010000: case 0b10011000:
         case 0b10100000: case 0b10101000: case 0b10110000: case 0b10111000:
    return s::nullopt;
  break; case 0b11000000: case 0b11001000: case 0b11010000: case 0b11011000: {
    if (pos + 2 > text.size()) return s::nullopt;
    unsigned char c2 = text[pos+1] & 0b11000000;
    switch (c2){
    break; case 0b10000000:; // do nothing
    break; default: return s::nullopt;
    }
    c1 = text[pos];
    c2 = text[pos+1];
    char32_t val = ((c1 & 0b00011111) << 6) + (c2 & 0b00111111);
    if (val < 0x80) return s::nullopt; // overlong encoding
    return DecodedCodePoint{val, pos + 2};
  }
  break; case 0b11100000: case 0b11101000: {
    if (pos + 3 > text.size()) return s::nullopt;
    unsigned char c2 = text[pos+1] & 0b11000000;
    unsigned char c3 = (text[pos+2] & 0b11000000) >> 2;
    switch (c2 | c3){
    break; case 0b10100000:; // do nothing
    break; default: return s::nullopt;
    }
    c1 = text[pos];
    c2 = text[pos+1];
    c3 = text[pos+2];
    char32_t val = ((c1 & 0b00001111) << 12) + ((c2 & 0b00111111) << 6) + (c3 & 0b00111111);
    if (val < 0x800) return s::nullopt;                  // overlong encoding
    if (val >= 0xD800 and val <= 0xDFFF) return s::nullopt; // UTF-16 surrogate half
    return DecodedCodePoint{val, pos + 3};
  }
  break; case 0b11110000: {
    if (pos + 4 > text.size()) return s::nullopt;
    unsigned char c2 = text[pos+1] & 0b11000000;
    unsigned char c3 = (text[pos+2] & 0b11000000) >> 2;
    unsigned char c4 = (text[pos+3] & 0b11000000) >> 4;
    switch (c2 | c3 | c4){
    break; case 0b10101000:; // do nothing
    break; default: return s::nullopt;
    }
    c1 = text[pos];
    c2 = text[pos+1];
    c3 = text[pos+2];
    c4 = text[pos+3];
    char32_t val = ((c1 & 0b00000111) << 18) + ((c2 & 0b00111111) << 12) + ((c3 & 0b00111111) << 6) + (c4 & 0b00111111);
    if (val < 0x10000) return s::nullopt;  // overlong encoding
    if (val > 0x10FFFF) return s::nullopt; // beyond Unicode range
    return DecodedCodePoint{val, pos + 4};
  }
  break; case 0b11111000:
         default:
    return s::nullopt;
  }
}

bool is_valid_utf8(strv text) {
  size_t pos = 0;
  while (pos < text.size()){
    opt<DecodedCodePoint> d = decode_utf8(text, pos);
    if (not d.has_value()) return false;
    pos = d->next;
  }
  return true;
}

s::size_t count_code_points(strv text) {
  size_t count = 0;
  size_t pos = 0;
  while (pos < text.size()){
    opt<DecodedCodePoint> d = decode_utf8(text, pos);
    if (not d.has_value()) return count;
    pos = d->next;
    count++;
  }
  return count;
}

s::size_t display_width(strv text) {
  // A cluster is a base character plus whatever attaches to it (combining marks, VS16,
  // skin tones, ZWJ-joined emoji). The whole cluster is drawn in cluster_width cells.
  size_t width = 0;
  size_t pos = 0;
  size_t cluster_width = 0;  // cells already counted for the current cluster, 0 if none
  bool emoji = false;        // current cluster's base is an emoji
  bool joined = false;       // previous code point was a ZWJ inside an emoji cluster
  bool ri_pending = false;   // an unpaired regional indicator is waiting for its partner

  while (pos < text.size()){
    opt<DecodedCodePoint> d = decode_utf8(text, pos);
    if (not d.has_value()){
      // invalid byte: terminals render it as U+FFFD REPLACEMENT CHARACTER, 1 cell
      width += 1;
      pos += 1;
      cluster_width = 0; emoji = joined = ri_pending = false;
      continue;
    }
    char32_t cp = d->code_point;
    pos = d->next;

    if (is_control(cp)){
      cluster_width = 0; emoji = joined = ri_pending = false;
      continue;
    }
    // two regional indicators form one flag: 1 cell each, so a pair is 2 and a lone one is 1
    if (is_regional_indicator(cp)){
      width += 1;
      if (ri_pending){ ri_pending = false; cluster_width = 2; }
      else           { ri_pending = true;  cluster_width = 1; }
      emoji = true; joined = false;
      continue;
    }
    ri_pending = false;

    if (cp == VS16){
      // emoji presentation widens a narrow base, e.g. U+2764 heart or a keycap digit
      if (cluster_width == 1){ width += 1; cluster_width = 2; }
      continue;
    }
    if (emoji and is_skin_tone_modifier(cp)) continue;
    if (joined and in_table(cp, detail::EXTENDED_PICTOGRAPHIC)){
      joined = false;
      continue;
    }
    joined = false;

    size_t w = code_point_width(cp);
    if (w == 0){
      // zero-width code points attach to the current cluster
      if (cp == ZWJ) joined = emoji;
      continue;
    }
    width += w;
    cluster_width = w;
    emoji = in_table(cp, detail::EXTENDED_PICTOGRAPHIC);
  }
  return width;
}

}  // namespace pi::util
