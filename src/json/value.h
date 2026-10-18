#ifndef JSON_VALUE_H
#define JSON_VALUE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <util/aliases.h>

namespace pi::json {

// Forward declaration so Object and Array can reference Value before it is defined.
struct Value;

// Insertion-order-preserving JSON object.
// Linear-scan find() is intentional: objects in Pi are small (tool args, messages).
struct Object {
  vec<s::pair<str, Value>> entries;

  // Find the value for key; returns nullptr if absent.
  Value*       find(strv key);
  const Value* find(strv key) const;

  // Access or insert (null default) by key.
  Value& operator[](strv key);

  // Insert or overwrite key with value.
  void set(strv key, Value value);

  bool operator==(const Object&) const;
  // Defined below Value: libc++ needs Value complete to instantiate these.
  s::size_t size() const;
  bool empty() const;
};

// JSON array — a plain vector of Values.
using Array = vec<Value>;

// Owned, mutable JSON value.  Corresponds to TypeScript Json / PiG orderedjson.Value.
// Payload is a std::variant; Array and Object are held by value (std::vector permits
// an incomplete element type), so copy/destruction are compiler-generated.
// Move is explicit so a moved-from Value is null.
struct Value {
  enum class Kind : uint8_t { Null, Bool, Int, Double, String, Array, Object };

  // --- Constructors ---
  Value();                              // null
  Value(s::nullptr_t);
  explicit Value(bool b);
  explicit Value(int64_t i);
  explicit Value(int i);
  explicit Value(double d);
  explicit Value(str text);
  explicit Value(strv text);
  explicit Value(const char* text);
  explicit Value(Array a);
  explicit Value(Object o);

  // Copy is compiler-generated; move leaves the source null.
  Value(const Value&)            = default;
  Value& operator=(const Value&) = default;
  Value(Value&& o) noexcept;
  Value& operator=(Value&& o) noexcept;
  ~Value()                       = default;

  // --- Type queries ---
  Kind kind()      const { return static_cast<Kind>(mData.index()); }
  bool is_null()   const { return kind() == Kind::Null;   }
  bool is_bool()   const { return kind() == Kind::Bool;   }
  bool is_int()    const { return kind() == Kind::Int;    }
  bool is_double() const { return kind() == Kind::Double; }
  bool is_number() const { return is_int() or is_double(); }
  bool is_string() const { return kind() == Kind::String; }
  bool is_array()  const { return kind() == Kind::Array;  }
  bool is_object() const { return kind() == Kind::Object; }

  // --- Value extraction (nullopt / nullptr when wrong type) ---
  opt<bool>             as_bool()   const;
  opt<int64_t>          as_int()    const;
  opt<double>           as_double() const;
  opt<strv> as_string() const;

  Array*        as_array();
  const Array*  as_array()  const;
  Object*       as_object();
  const Object* as_object() const;

  // --- Object helpers (only valid when is_object()) ---
  Value&       operator[](strv key);
  Value*       find(strv key);
  const Value* find(strv key) const;
  void         set(strv key, Value val);

  // --- Array helpers (only valid when is_array()) ---
  Value&       operator[](s::size_t index);
  const Value& operator[](s::size_t index) const;
  void         push_back(Value val);

  // --- Common ---
  s::size_t size() const;  // entries for object, elements for array; 0 for scalars
  bool empty() const;

  // Same alternative and equal payload; Array / Object compare recursively.
  bool operator==(const Value&) const = default;

private:
  // Alternative order must match Kind: kind() is derived from index().
  using Data = s::variant<s::monostate, bool, int64_t, double, str, Array, Object>;
  static_assert(s::variant_size_v<Data> == static_cast<s::size_t>(Kind::Object) + 1);

  Data mData;
};

inline s::size_t Object::size() const { return entries.size(); }
inline bool Object::empty() const { return entries.empty(); }

// Convenience factories.
inline Value null_value()   { return Value{}; }
inline Value array_value()  { return Value{Array{}}; }
inline Value object_value() { return Value{Object{}}; }

}  // namespace pi::json

#endif  // JSON_VALUE_H
