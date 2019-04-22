#pragma once
#include <stdexcept>
#include <sstream>

namespace hardwave {
namespace heapfree {
namespace detail {

template<typename... Args>
void abort_throw(Args&&... args) {
  std::ostringstream str;
  ((str << "ERROR: ") << ... << std::forward<Args>(args)) << std::endl;
  throw std::logic_error{str.str()};
}

}
}
}

// This is provided in our tests so we can test that an assertation
// aborts program execution
#define HEAPFREE_ABORT ::hardwave::heapfree::detail::abort_throw
