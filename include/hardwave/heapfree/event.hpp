#pragma once
#include "hardwave/heapfree/meta.hpp"
#include "hardwave/heapfree/chain.hpp"
#include "hardwave/heapfree/meta.hpp"

namespace hardwave {
namespace heapfree {

namespace detail {

template<typename Event, typename Lambda, typename... Args>
class lambda_event_handler : public Event::chain_type::segment {
  using me_t_alias = lambda_event_handler<Event, Lambda, Args...>;
  HEAPFREE_DECLARE_ME_SUPER(me_t_alias, typename Event::chain_type::segment)

  using fn_ptr_type = typename Event::chain_type::value_type;

  // WouldBeNice: We should have some optimization for lambdas without
  // contexts (lambdas that are convertible to function pointers…) that
  // are known at compile time?
  Lambda fn;
public:
  lambda_event_handler() = default;
  lambda_event_handler(Lambda f) : super_t{nullptr}, fn{f} {
    super().value() = [](void *sis, Args&&... args) {
      reinterpret_cast<me_t*>(sis)->fn(std::forward<Args>(args)...);
    };
  }

  lambda_event_handler(const me_t &) = default;
  lambda_event_handler& operator=(const me_t &) = default;

  lambda_event_handler(me_t&&) = default;
  lambda_event_handler& operator=(me_t&&) = default;

  void swap(me_t &otr) {
    std::swap(super(), otr.super());
    std::swap(fn, otr.fn);
  }
};

} // namespace detail

/// Events hold a list of event listeners.
/// The template parameters indicate the return type and the
/// parameters that listeners receive when called
template<typename... Args>
class event {
  HEAPFREE_DECLARE_ME(event<Args...>);

  template<typename, typename, typename...>
  friend class detail::lambda_event_handler;

public:
  using chain_type = chain<function_ptr<void, void*, Args&&...>>;
  chain_type member_listeners;
  chain_type listeners;

public:
  event() = default;

  event(const me_t&) = delete;
  me_t& operator=(const me_t&) = delete;

  event(me_t &&otr) : listeners{std::move(otr.listeners)} {};
  me_t& operator=(me_t &&otr) {
    listeners = std::move(otr.listeners);
    return me();
  }

  void swap(me_t &otr) {
    std::swap(listeners, otr.listeners);
  }
};

/// On can be used to register an event handler.
/// Returns the chain segment that actually stores the event handler;
/// this segment must be kept in scope as long as the handler shall stay
/// registered.
///
/// The segment type is constructed on the fly so various kinds of lambda
/// types can be stored. (This allows us to store lambdas with arbitrary context).
///
/// ```
/// event<int, int> my_event;
///
/// int counter=0;
/// on(my_event, [&](int a, int b) {
///   println("Event handler one was called for the ", counter, "nd time: ", a, ", ", b);
/// });
///
/// fire(my_event, 42, 23);
/// ```
///
/// Should output:
///
/// ```
/// Event handler one was called for the 1nd time: 23, 42
/// ```
template<typename Lambda, typename... Args>
[[nodiscard]] auto on(event<Args...> &ev, const Lambda &fn) {
  using EventType = event<Args...>;
  using HandlerType = detail::lambda_event_handler<
    EventType, Lambda, Args...>;

  HandlerType handler{fn};
  auto &seg = static_cast<typename EventType::chain_type::segment&>(handler);
  ev.listeners.link_back(seg);
  return handler;
}

/// Used to invoke all the event handlers of an event.
template<typename... Args>
bool try_fire(event<Args...> &ev, Args&&... args) {
  for (auto &handler : ev.member_listeners.segments())
    handler.value()((void*)&handler, std::forward<Args>(args)...);
  for (auto &handler : ev.listeners.segments())
    handler.value()((void*)&handler, std::forward<Args>(args)...);
  return std::size(ev.listeners) > 0 || std::size(ev.member_listeners) > 0;
}

template<typename... Args>
void fire(event<Args...> &ev, Args&&... args) {
  HEAPFREE_ASSERT(try_fire(ev, std::forward<Args>(args)...),
      "Could not fire event: No listeners");
}

} // namespace heapfree
} // namespace hardwave
