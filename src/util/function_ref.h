#ifndef UTIL_FUNCTION_REF_H
#define UTIL_FUNCTION_REF_H

#include <functional>
#include <memory>
#include <type_traits>

#include <util/aliases.h>

namespace pi::util {

template <typename Sig> struct FunctionRef;

// Non-owning, type-erased reference to a callable; a stand-in for C++26
// std::function_ref until libc++ ships it.
//
// Two pointers wide, never allocates, cheap to copy: pass it by value.  It only
// borrows the callable, so the callable must outlive every call.  Binding a
// temporary is fine as a function argument (the temporary lives until the call
// returns) but dangles when stored:
//
//   transport.post_stream(..., [&](strv c){ body += c; }, cancel);  // ok
//   ChunkCallback cb = [&](strv c){ body += c; };                   // dangles
template <typename R, typename... Args>
struct FunctionRef<R(Args...)> {
  template <typename F>
    requires (not s::is_same_v<s::remove_cvref_t<F>, FunctionRef>)
         and (not s::is_function_v<s::remove_reference_t<F>>)
         and s::is_invocable_r_v<R, F&, Args...>
  FunctionRef(F&& f) noexcept  // NOLINT(google-explicit-constructor)
    : mCallee{.obj = const_cast<void*>(static_cast<const void*>(s::addressof(f)))}
    , mThunk([](Callee callee, Args... args) -> R {
        return s::invoke(*static_cast<s::add_pointer_t<F>>(callee.obj), s::forward<Args>(args)...);
      }) {}

  FunctionRef(R (*fn)(Args...)) noexcept  // NOLINT(google-explicit-constructor)
    : mCallee{.fn = reinterpret_cast<void (*)()>(fn)}
    , mThunk([](Callee callee, Args... args) -> R {
        return s::invoke(reinterpret_cast<R (*)(Args...)>(callee.fn), s::forward<Args>(args)...);
      }) {}

  R operator()(Args... args) const {
    return mThunk(mCallee, s::forward<Args>(args)...);
  }

private:
  // A function pointer may not round-trip through void*, so it gets its own slot.
  union Callee {
    void* obj;
    void (*fn)();
  };

  Callee mCallee;
  R (*mThunk)(Callee, Args...);
};

}  // namespace pi::util

#endif  // UTIL_FUNCTION_REF_H
