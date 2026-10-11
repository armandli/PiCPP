#ifndef JSON_VALUE_H
#define JSON_VALUE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

    bool   operator==(const Object&) const;
    // Defined below Value: libc++ needs Value complete to instantiate these.
    s::size_t size()  const;
    bool        empty() const;
};

// JSON array — a plain vector of Values.
using Array = vec<Value>;

// Owned, mutable JSON value.  Corresponds to TypeScript Json / PiG orderedjson.Value.
// Stores arrays and objects via heap pointers (raw owning) to break the recursive
// type dependency; copy/move are implemented explicitly in value.cpp.
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

    Value(const Value&);
    Value(Value&&) noexcept;
    Value& operator=(const Value&);
    Value& operator=(Value&&) noexcept;
    ~Value();

    // --- Type queries ---
    Kind kind()      const { return mKind; }
    bool is_null()   const { return mKind == Kind::Null;   }
    bool is_bool()   const { return mKind == Kind::Bool;   }
    bool is_int()    const { return mKind == Kind::Int;    }
    bool is_double() const { return mKind == Kind::Double; }
    bool is_number() const { return is_int() or is_double(); }
    bool is_string() const { return mKind == Kind::String; }
    bool is_array()  const { return mKind == Kind::Array;  }
    bool is_object() const { return mKind == Kind::Object; }

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
    s::size_t size()  const;  // entries for object, elements for array; 0 for scalars
    bool        empty() const;

    bool operator==(const Value&) const;

private:
    Kind mKind = Kind::Null;

    // Raw owning pointers break the recursive type dependency.
    // Exactly one member is active (determined by mKind); scalars use the value members.
    union {
        bool                   boolean;
        int64_t                integer;
        double                 real;
        str*           string;   // owned
        vec<Value>*    array;    // owned  (std::vector<Value> = Array)
        Object*                object;   // owned
    } mData{};

    void destroy();
    void copy_from(const Value&);
};

inline s::size_t Object::size()  const { return entries.size(); }
inline bool        Object::empty() const { return entries.empty(); }

// Convenience factories.
inline Value null_value()   { return Value{}; }
inline Value array_value()  { return Value{Array{}}; }
inline Value object_value() { return Value{Object{}}; }

}  // namespace pi::json

#endif  // JSON_VALUE_H
