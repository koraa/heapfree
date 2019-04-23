#pragma once

#ifndef HEAPFREE_ABORT
#include <cstdlib>
#include <iostream>

namespace hardwave {
namespace heapfree {
namespace detail {

template<typename... Args>
[[noreturn]] void abort(Args&&... args) {
  ((std::cerr << "ERROR: ") << ... << std::forward<Args>(args)) << std::endl;
  std::abort();
}

} // namespace detail
} // namespace heapfree
} // namespace hardwave

/// Hook that is called to abort program execution
///
/// Implementations take the following form:
///
/// ```
/// template<typename... Args>
/// [[noreturn]] void abort(Args&&... args);
/// ```
///
/// Can be overwritten if the macro is defined before this file
#define HEAPFREE_ABORT ::hardwave::heapfree::detail::abort
#endif

/// Check if the given condition `b` is true.
/// If the condition evaluates to false, then HEAPFREE_ABORT
/// is called with the error message `...`, the current
/// file and line number
#define HEAPFREE_ASSERT(b, ...)        \
  if (!(b))                             \
    HEAPFREE_ABORT(__VA_ARGS__, " (", __FILE__, ":", __LINE__, ")");
