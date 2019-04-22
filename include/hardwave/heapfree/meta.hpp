#pragma once

namespace hardwave {
namespace heapfree {

/// Just an alias that makes declaring a function pointer
/// slightly more readable
template<typename R, typename... Args>
using function_ptr = R (*)(Args...);

/// Automatically defines me_t, me() and me() const
/// Parameter must be either the type of *this or
/// a subclass if the class uses the curiously repeating
/// template pattern
#define HEAPFREE_DECLARE_ME(T)                        \
  using me_t = T;                                     \
  me_t& me() { return static_cast<me_t&>(*this); }    \
  const me_t& me() const { return static_cast<const me_t&>(*this); }

/// Defines super_t, super() and super() const
/// Internally also invokes HEAPFREE_DECLARE_ME
#define HEAPFREE_DECLARE_ME_SUPER(Tme, Tsuper)             \
  HEAPFREE_DECLARE_ME(Tme)                                 \
  using super_t = Tsuper;                                  \
  super_t& super() { return static_cast<super_t&>(me()); } \
  const super_t& super() const { return static_cast<const super_t&>(me()); }

} // namespace heapfree
} // namespace hardwave
