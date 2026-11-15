#ifndef UTIL_LANGUAGE_H
#define UTIL_LANGUAGE_H

// Small core-language helpers shared across modules.

namespace pi::util {

// Builds one visitor out of several lambdas for s::visit:
//
//   s::visit(util::Overloaded{
//     [](const TextContent& c) { ... },
//     [](const auto&)          { ... },
//   }, content);
template <typename... Ts> struct Overloaded : Ts... { using Ts::operator()...; };

}  // namespace pi::util

#endif  // UTIL_LANGUAGE_H
