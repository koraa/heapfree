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

#define HEAPFREE_ABORT ::hardwave::heapfree::detail::abort
#endif

#define HEAPFREE_ASSERT(b, ...)        \
  if (!(b))                             \
    HEAPFREE_ABORT(__VA_ARGS__, " (", __FILE__, ":", __LINE__, ")");
