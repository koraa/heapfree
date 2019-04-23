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

/// Macro that allows class members to be used as event listeners
///
/// The class using this and the event may be moved freely
///
/// If the event and the class are moved together – i.e. they are somehow
/// part of the same class – consider using HEAPFREE_RELATIVE_MEMBER_EVENT_LISTENER.
///
/// # Implementation
///
/// Making this work requires some rather complex template and macro metaprogramming
/// facilities unfortunately.
///
/// In order to call method, three pieces of information are needed:
///
/// * The class type
/// * The location of the class instance
/// * The method name
///
/// Unfortunately, event handlers only really have two pieces of information:
///
/// * The location of the listener (passed to them internally as a parameter)
/// * A single custom function pointer which is invoked by `fire()`
///
/// In order to supplant the missing bits of information, this macro generates
/// a C function (by using a lambda) which knows the name of the method and the
/// type of the class for each listener/class combination.
///
/// The third piece of information – the location of the class instance – is
/// reconstructed from the location of the listener: The function we generated
/// just subtracts the offset of the listener inside the class from the location
/// of the listener. This yields the class location.
///
/// This `offset of the listener` is a third piece of information stored in the
/// function we created; it is initially determined using `offsetof()`.
/// Note that this usage of offsetof is *conditionally supported* meaning our usage
/// of offsetof is not really portable/standard, but compilers must raise an error
/// on invalid usage; since compilers will yield an error if our usage is not supported
/// doing this is fine in practice.
/// GCC might warn about this usage, so we recommend adding -Wno-invalid-offsetof
/// to your compiler parameters.
///
/// # Example
///
/// ```c++
/// #include <iostream>
/// #include <utility>
/// #include "hardwave/heapfree/error.hpp"
/// #include "hardwave/heapfree/event.hpp"
/// #include "hardwave/heapfree/event/member_listener.hpp"
///
/// using namespace hardwave::heapfree;
///
/// event<int, int> my_event;
///
/// struct my_struct {
///   // Adding this is mandatory
///   HEAPFREE_DECLARE_ME(my_struct);
/// public:
///   const char *id;
///   int counter{0};
///   my_struct(const char *i) : id{i} {}
///
///   HEAPFREE_MEMBER_EVENT_LISTENER(my_event, my_listener)
///   void my_listener(int a, int b) {
///     std::cerr << "Method " << id << " listener called! #"
///       << counter++ << "; " << a << " | " << b << "\n";
///   }
/// };
///
/// int main() {
///   my_struct s{"whitemice"};
///   auto listener = on(my_event, [](int a, int b) {
///     std::cerr << "this too! " << a << " " << b << "\n";
///   });
///
///   fire(my_event, 1, 2);
///
///   // Can move my_struct; event listener will now be called on both!
///   my_struct s2{std::move(s)};
///   s.id = "grayrats";
///   fire(my_event, 5, 6);
///   HEAPFREE_ASSERT(s2.counter == 2, "");
///
///   // Moving the event
///   event<int, int> ev = std::move(my_event);
///   fire(ev, 3, 4);
///
///   return 0;
/// }
/// ```
///
/// Output:
///
/// ```
/// Method whitemice listener called! #0; 1 | 2
/// this too! 1 2
/// Method grayrats listener called! #1; 5 | 6
/// this too! 5 6
/// Method whitemice listener called! #1; 5 | 6
/// Method grayrats listener called! #2; 3 | 4
/// this too! 3 4
/// Method whitemice listener called! #2; 3 | 4
/// ```
#define HEAPFREE_MEMBER_EVENT_LISTENER(event, handler) \
  HEAPFREE_MEMBER_EVENT_LISTENER_IMPL(event, handler, false)

/// Similar to HEAPFREE_MEMBER_EVENT_LISTENER, but for classes which
/// contain both event listener and event itself.
///
/// # Example
///
/// ```c++
/// #include <iostream>
/// #include <utility>
/// #include "hardwave/heapfree/error.hpp"
/// #include "hardwave/heapfree/event.hpp"
/// #include "hardwave/heapfree/event/member_listener.hpp"
///
/// using namespace hardwave::heapfree;
///
/// struct my_struct {
///   // Adding this is mandatory
///   HEAPFREE_DECLARE_ME(my_struct);
/// public:
///   const char *id;
///   int counter{0};
///   my_struct(const char *i) : id{i} {}
///
///   event<int, int> my_event;
///
///   HEAPFREE_RELATIVE_MEMBER_EVENT_LISTENER(my_event, my_listener)
///   void my_listener(int a, int b) {
///     std::cerr << "Method " << id << " listener called! #"
///       << counter++ << "; " << a << " | " << b << "\n";
///   }
/// };
///
/// int main() {
///   my_struct s{"whitemice"};
///   auto listener = on(s.my_event, [](int a, int b) {
///     std::cerr << "this too! " << a << " " << b << "\n";
///   });
///
///   fire(s.my_event, 1, 2);
///
///   /// Member listener & event stay relative to each other
///   my_struct s2{"graymice"};
///   std::swap(s, s2);
///   fire(s2.my_event, 3, 4);
///
///   /// Can move the event, but this won't trigger any member event listeners
///   event<int, int> ev{std::move(s2.my_event)};
///   fire(ev, 5,6 );
///
///   return 0;
/// }
/// ```
///
/// Output:
///
/// ```
/// Method whitemice listener called! #0; 1 | 2
/// this too! 1 2
/// Method whitemice listener called! #1; 3 | 4
/// this too! 3 4
/// this too! 5 6
/// ```
#define HEAPFREE_RELATIVE_MEMBER_EVENT_LISTENER(event, handler) \
  HEAPFREE_MEMBER_EVENT_LISTENER_IMPL(event, handler, true)
