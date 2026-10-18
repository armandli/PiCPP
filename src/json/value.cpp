#include <json/value.h>

#include <cassert>

#include <util/aliases.h>

namespace pi::json {

// ---------------------------------------------------------------------------
// Object
// ---------------------------------------------------------------------------

Value* Object::find(strv key) {
  return const_cast<Value*>(static_cast<const Object&>(*this).find(key));
}

// shallow search
const Value* Object::find(strv key) const {
  for (const auto& [k, v] : entries){
    if (key == k) return &v;
  }
  return nullptr;
}

Value& Object::operator[](strv key) {
  for (auto& [k, v] : entries){
    if (key == k) return v;
  }
  entries.emplace_back(str(key), Value());
  return entries.back().second;
}

void Object::set(strv key, Value value) {
  for (auto& [k, v] : entries)
    if (key == k){
      v = value;
      return;
    }
  entries.emplace_back(str(key), value);
}

bool Object::operator==(const Object& other) const {
  if (entries.size() != other.entries.size()) return false;
  for (size_t i = 0; i < entries.size(); ++i){
    const auto& [ka, va] = entries[i];
    const auto& [kb, vb] = other.entries[i];
    if (ka != kb or va != vb) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Value — constructors
// ---------------------------------------------------------------------------
// Alternatives are selected explicitly with in_place_type so the variant's
// converting constructor never picks an unintended one (e.g. const char* → bool).

Value::Value()
    : mData(s::in_place_type<s::monostate>) {}

Value::Value(s::nullptr_t)
    : mData(s::in_place_type<s::monostate>) {}

Value::Value(bool b)
    : mData(s::in_place_type<bool>, b) {}

Value::Value(int64_t i)
    : mData(s::in_place_type<int64_t>, i) {}

Value::Value(int i)
    : mData(s::in_place_type<int64_t>, i) {}

Value::Value(double d)
    : mData(s::in_place_type<double>, d) {}

Value::Value(str text)
    : mData(s::in_place_type<str>, s::move(text)) {}

Value::Value(strv text)
    : mData(s::in_place_type<str>, text) {}

Value::Value(const char* text)
    : mData(s::in_place_type<str>, text) {}

Value::Value(Array a)
    : mData(s::in_place_type<Array>, s::move(a)) {}

Value::Value(Object o)
    : mData(s::in_place_type<Object>, s::move(o)) {}

Value::Value(Value&& o) noexcept
    : mData(s::exchange(o.mData, Data{})) {}

Value& Value::operator=(Value&& o) noexcept {
  if (this != &o) {
    mData = s::exchange(o.mData, Data{});
  }
  return *this;
}

// ---------------------------------------------------------------------------
// Value — type extraction
// ---------------------------------------------------------------------------

opt<bool> Value::as_bool() const {
  if (kind() != Kind::Bool) return s::nullopt;
  return s::get<bool>(mData);
}

opt<int64_t> Value::as_int() const {
  if (kind() != Kind::Int) return s::nullopt;
  return s::get<int64_t>(mData);
}

opt<double> Value::as_double() const {
  if (kind() != Kind::Double) return s::nullopt;
  return s::get<double>(mData);
}

opt<strv> Value::as_string() const {
  if (kind() != Kind::String) return s::nullopt;
  return s::get<str>(mData);
}

Array* Value::as_array() {
  return const_cast<Array*>(static_cast<const Value&>(*this).as_array());
}

const Array* Value::as_array() const {
  if (kind() != Kind::Array) return nullptr;
  return &s::get<Array>(mData);
}

Object* Value::as_object() {
  return const_cast<Object*>(static_cast<const Value&>(*this).as_object());
}

const Object* Value::as_object() const {
  if (kind() != Kind::Object) return nullptr;
  return &s::get<Object>(mData);
}

// ---------------------------------------------------------------------------
// Value — object helpers
// ---------------------------------------------------------------------------

Value& Value::operator[](strv key) {
  assert(is_object());
  return s::get<Object>(mData).operator[](key);
}

Value* Value::find(strv key) {
  assert(is_object());
  return s::get<Object>(mData).find(key);
}

const Value* Value::find(strv key) const {
  assert(is_object());
  return s::get<Object>(mData).find(key);
}

void Value::set(strv key, Value val) {
  assert(is_object());
  return s::get<Object>(mData).set(key, val);
}

// ---------------------------------------------------------------------------
// Value — array helpers
// ---------------------------------------------------------------------------

Value& Value::operator[](s::size_t index) {
  assert(is_array());
  return s::get<Array>(mData)[index];
}

const Value& Value::operator[](s::size_t index) const {
  assert(is_array());
  return s::get<Array>(mData)[index];
}

void Value::push_back(Value val) {
  assert(is_array());
  return s::get<Array>(mData).push_back(s::move(val));
}

// ---------------------------------------------------------------------------
// Value — common
// ---------------------------------------------------------------------------

s::size_t Value::size() const {
  switch (kind()){
  break; case Kind::Null: case Kind::Bool: case Kind::Int: case Kind::Double:
         case Kind::String:
    return 0;
  break; case Kind::Array:
    return s::get<Array>(mData).size();
  break; case Kind::Object:
    return s::get<Object>(mData).size();
  break; default: assert(false);
  }
}

bool Value::empty() const {
  return size() == 0;
}

}  // namespace pi::json
