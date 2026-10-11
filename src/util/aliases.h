#ifndef UTIL_ALIASES_H
#define UTIL_ALIASES_H

// Project-wide short names for heavily used namespaces and types. Declared at
// global scope on purpose so every pi:: namespace and the tests can use them.

#include <chrono>  // IWYU pragma: export
#include <filesystem>  // IWYU pragma: export
#include <memory>  // IWYU pragma: export
#include <optional>  // IWYU pragma: export
#include <string>  // IWYU pragma: export
#include <string_view>  // IWYU pragma: export
#include <thread>  // IWYU pragma: export
#include <unordered_map>  // IWYU pragma: export
#include <unordered_set>  // IWYU pragma: export
#include <vector>  // IWYU pragma: export

// NOLINTBEGIN(misc-unused-alias-decls)
namespace s  = std;
namespace sc = std::chrono;
namespace sf = std::filesystem;
namespace st = std::this_thread;
// NOLINTEND(misc-unused-alias-decls)

using str  = s::string;
using strv = s::string_view;
template <typename T, typename A = s::allocator<T>> using vec = s::vector<T, A>;
template <typename T> using opt = s::optional<T>;

template <typename... Ts> using umap  = s::unordered_map<Ts...>;
template <typename... Ts> using uset  = s::unordered_set<Ts...>;
template <typename... Ts> using ummap = s::unordered_multimap<Ts...>;
template <typename... Ts> using umset = s::unordered_multiset<Ts...>;

template <typename T, typename D = s::default_delete<T>> using uptr = s::unique_ptr<T, D>;
template <typename T> using sptr = s::shared_ptr<T>;
template <typename T> using wptr = s::weak_ptr<T>;

#endif  // UTIL_ALIASES_H
