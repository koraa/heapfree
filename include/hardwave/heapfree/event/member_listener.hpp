#pragma once
#include <type_traits>
#include "hardwave/heapfree/meta.hpp"
#include "hardwave/heapfree/event.hpp"

namespace hardwave {
namespace heapfree {
namespace detail {

template<typename Claz, typename Meta, typename... Args>
class member_event_listener : public event<Args...>::chain_type::segment {
  using me_t_alias = member_event_listener<Claz, Meta, Args...>;
  HEAPFREE_DECLARE_ME_SUPER(me_t_alias, typename event<Args...>::chain_type::segment);

  bool is_relative;
public:
  member_event_listener(bool rel) : is_relative{rel} {
    super().value() = [](void *v, Args&&... args) {
      auto &inst = Meta::instance(v);
      auto memb = Meta::memb();
      (inst.*memb)( std::forward<Args>(args)... );
    };
    if (is_relative) {
      Meta::get_event(this).member_listeners.link_back(super());
    } else {
      Meta::get_event(this).listeners.link_back(super());
    }
  }

  // Just for template argument deduction below
  member_event_listener(Claz&, const Meta&, event<Args...>&);

  member_event_listener(const me_t &otr) : me_t{otr.is_relative} {}
  me_t& operator=(const me_t&) { return me(); }
  member_event_listener(me_t &&otr) : me_t{otr.is_relative} {}
  me_t& operator=(me_t&&) { return me(); }
};

} // namespace detail
} // namespace heapfree
} // namespace hardwave


#define HEAPFREE_MEMBER_EVENT_LISTENER_IMPL(event, handler, relative) \
  template<typename EventContainerMeta>                         \
  struct handler ## _listener_meta {                            \
    static constexpr auto memb() {                              \
      return &EventContainerMeta:: handler;                     \
    }                                                           \
    static constexpr auto& instance(void *listener) {           \
      constexpr size_t off =                                    \
        offsetof(EventContainerMeta, handler ## _listener);     \
      return *reinterpret_cast<EventContainerMeta*>(            \
        reinterpret_cast<unsigned char*>(listener) - off);      \
    }                                                           \
    static auto& get_event(void *listener) {                    \
      return instance(listener). handler ## _listener_event();  \
    }                                                           \
  };                                                            \
  auto& handler ## _listener_event() {                          \
    return (event);                                             \
  }                                                             \
  using handler ## _listener_type = decltype(                   \
    ::hardwave::heapfree::detail::member_event_listener{        \
      ::std::declval<me_t&>(),                                  \
      handler ## _listener_meta<me_t>{},                        \
      (event)});                                                \
  /* Actual variable */                                         \
  handler ## _listener_type handler ## _listener{relative};     \
#define HEAPFREE_MEMBER_EVENT_LISTENER(event, handler) \
  HEAPFREE_MEMBER_EVENT_LISTENER_IMPL(event, handler, false)

#define HEAPFREE_RELATIVE_MEMBER_EVENT_LISTENER(event, handler) \
  HEAPFREE_MEMBER_EVENT_LISTENER_IMPL(event, handler, true)
