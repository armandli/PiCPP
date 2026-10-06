#include <json/value.h>

#include <cassert>

namespace pi::json {

// ---------------------------------------------------------------------------
// Object
// ---------------------------------------------------------------------------

Value* Object::find(std::string_view key) {
    // TODO(M1): linear scan entries for key; return pointer to value or nullptr.
    (void)key;
    return nullptr;
}

const Value* Object::find(std::string_view key) const {
    // TODO(M1): linear scan entries for key; return const pointer or nullptr.
    (void)key;
    return nullptr;
}

Value& Object::operator[](std::string_view key) {
    // TODO(M1): find key; if absent, append {key, null} and return new value.
    (void)key;
    // Stub: crash loudly so the user knows this is not implemented.
    assert(false && "Object::operator[] not implemented");
    return entries.front().second;  // unreachable
}

void Object::set(std::string_view key, Value value) {
    // TODO(M1): find key and overwrite; if absent, append a new entry.
    (void)key; (void)value;
}

bool Object::operator==(const Object& other) const {
    // TODO(M1): entries must be equal in the same order.
    (void)other;
    return false;
}

// ---------------------------------------------------------------------------
// Value — private helpers
// ---------------------------------------------------------------------------

void Value::destroy() {
    switch (mKind) {
    case Kind::String: delete mData.string;  break;
    case Kind::Array:  delete mData.array;   break;
    case Kind::Object: delete mData.object;  break;
    default: break;
    }
    mKind   = Kind::Null;
    mData   = {};
}

void Value::copy_from(const Value& o) {
    mKind = o.mKind;
    switch (mKind) {
    case Kind::Null:   mData = {};                                          break;
    case Kind::Bool:   mData.boolean = o.mData.boolean;                    break;
    case Kind::Int:    mData.integer = o.mData.integer;                    break;
    case Kind::Double: mData.real    = o.mData.real;                       break;
    case Kind::String: mData.string  = new std::string(*o.mData.string);   break;
    case Kind::Array:  mData.array   = new Array(*o.mData.array);          break;
    case Kind::Object: mData.object  = new Object(*o.mData.object);        break;
    }
}

// ---------------------------------------------------------------------------
// Value — constructors / destructor / copy / move
// ---------------------------------------------------------------------------

Value::Value()
    : mKind(Kind::Null), mData{} {}

Value::Value(std::nullptr_t)
    : mKind(Kind::Null), mData{} {}

Value::Value(bool b)
    : mKind(Kind::Bool), mData{} {
    mData.boolean = b;
}

Value::Value(int64_t i)
    : mKind(Kind::Int), mData{} {
    mData.integer = i;
}

Value::Value(int i)
    : mKind(Kind::Int), mData{} {
    mData.integer = static_cast<int64_t>(i);
}

Value::Value(double d)
    : mKind(Kind::Double), mData{} {
    mData.real = d;
}

Value::Value(std::string s)
    : mKind(Kind::String), mData{} {
    mData.string = new std::string(std::move(s));
}

Value::Value(std::string_view s)
    : mKind(Kind::String), mData{} {
    mData.string = new std::string(s);
}

Value::Value(const char* s)
    : mKind(Kind::String), mData{} {
    mData.string = new std::string(s);
}

Value::Value(Array a)
    : mKind(Kind::Array), mData{} {
    mData.array = new Array(std::move(a));
}

Value::Value(Object o)
    : mKind(Kind::Object), mData{} {
    mData.object = new Object(std::move(o));
}

Value::Value(const Value& o)
    : mKind(Kind::Null), mData{} {
    copy_from(o);
}

Value::Value(Value&& o) noexcept
    : mKind(o.mKind), mData(o.mData) {
    o.mKind = Kind::Null;
    o.mData = {};
}

Value& Value::operator=(const Value& o) {
    if (this != &o) {
        destroy();
        copy_from(o);
    }
    return *this;
}

Value& Value::operator=(Value&& o) noexcept {
    if (this != &o) {
        destroy();
        mKind   = o.mKind;
        mData   = o.mData;
        o.mKind = Kind::Null;
        o.mData = {};
    }
    return *this;
}

Value::~Value() {
    destroy();
}

// ---------------------------------------------------------------------------
// Value — type extraction
// ---------------------------------------------------------------------------

std::optional<bool> Value::as_bool() const {
    // TODO(M1): return mData.boolean iff mKind == Kind::Bool.
    (void)this;
    return std::nullopt;
}

std::optional<int64_t> Value::as_int() const {
    // TODO(M1): return mData.integer iff mKind == Kind::Int.
    return std::nullopt;
}

std::optional<double> Value::as_double() const {
    // TODO(M1): return mData.real iff mKind == Kind::Double.
    return std::nullopt;
}

std::optional<std::string_view> Value::as_string() const {
    // TODO(M1): return *mData.string iff mKind == Kind::String.
    return std::nullopt;
}

Array* Value::as_array() {
    // TODO(M1): return mData.array iff mKind == Kind::Array.
    return nullptr;
}

const Array* Value::as_array() const {
    // TODO(M1): return mData.array iff mKind == Kind::Array.
    return nullptr;
}

Object* Value::as_object() {
    // TODO(M1): return mData.object iff mKind == Kind::Object.
    return nullptr;
}

const Object* Value::as_object() const {
    // TODO(M1): return mData.object iff mKind == Kind::Object.
    return nullptr;
}

// ---------------------------------------------------------------------------
// Value — object helpers
// ---------------------------------------------------------------------------

Value& Value::operator[](std::string_view key) {
    // TODO(M1): assert is_object(), delegate to mData.object->operator[](key).
    (void)key;
    assert(false && "Value::operator[](string_view) not implemented");
    return *this;  // unreachable
}

Value* Value::find(std::string_view key) {
    // TODO(M1): assert is_object(), delegate to mData.object->find(key).
    (void)key;
    return nullptr;
}

const Value* Value::find(std::string_view key) const {
    // TODO(M1): assert is_object(), delegate to mData.object->find(key).
    (void)key;
    return nullptr;
}

void Value::set(std::string_view key, Value val) {
    // TODO(M1): assert is_object(), delegate to mData.object->set(key, val).
    (void)key; (void)val;
}

// ---------------------------------------------------------------------------
// Value — array helpers
// ---------------------------------------------------------------------------

Value& Value::operator[](std::size_t index) {
    // TODO(M1): assert is_array(), return (*mData.array)[index].
    (void)index;
    assert(false && "Value::operator[](size_t) not implemented");
    return *this;  // unreachable
}

const Value& Value::operator[](std::size_t index) const {
    // TODO(M1): assert is_array(), return (*mData.array)[index].
    (void)index;
    assert(false && "Value::operator[](size_t) const not implemented");
    return *this;  // unreachable
}

void Value::push_back(Value val) {
    // TODO(M1): assert is_array(), mData.array->push_back(std::move(val)).
    (void)val;
}

// ---------------------------------------------------------------------------
// Value — common
// ---------------------------------------------------------------------------

std::size_t Value::size() const {
    // TODO(M1): array → mData.array->size(); object → mData.object->size(); else 0.
    return 0;
}

bool Value::empty() const {
    // TODO(M1): size() == 0.
    return true;
}

bool Value::operator==(const Value& other) const {
    // TODO(M1): compare mKind; if equal, compare the active union member.
    // For Array / Object, this is recursive.
    (void)other;
    return false;
}

}  // namespace pi::json
