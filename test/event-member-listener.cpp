#include <utility>
#include <catch2/catch.hpp>
#include "hardwave/heapfree/meta.hpp"
#include "hardwave/heapfree/event.hpp"
#include "hardwave/heapfree/event/member_listener.hpp"

namespace {
using namespace hardwave::heapfree;

event<int> ev_global;

struct event_owner {
  event<int> ev;
};

struct test_struct {
  HEAPFREE_DECLARE_ME(test_struct);

  int own_v{999};
  int glob_v{999};
  event_owner own;
  HEAPFREE_MEMBER_EVENT_LISTENER(ev_global, global_handler)
  HEAPFREE_RELATIVE_MEMBER_EVENT_LISTENER(own.ev, own_handler)

  void global_handler(int i) {
    glob_v = i;
  }

  void own_handler(int i) {
    own_v = i;
  }
};

TEST_CASE("Event member listener") {
  test_struct s{};
  REQUIRE(s.glob_v == 999);
  REQUIRE(s.own_v == 999);

  fire(ev_global, 42);
  REQUIRE(s.glob_v == 42);
  REQUIRE(s.own_v == 999);

  fire(s.own.ev, 23);
  REQUIRE(s.glob_v == 42);
  REQUIRE(s.own_v == 23);

  // Move
  {
    test_struct c{};
    s = std::move(c);
    REQUIRE(c.glob_v == 999);
    REQUIRE(c.own_v == 999);
    REQUIRE(s.glob_v == 999);
    REQUIRE(s.own_v == 999);

    fire(s.own.ev, 32);
    REQUIRE(s.own_v == 32);
    REQUIRE(c.own_v == 999);

    fire(ev_global, 55);
    REQUIRE(c.glob_v == 55);
    REQUIRE(s.glob_v == 55);
  }

  // Swap
  {
    test_struct c{};
    std::swap(s, c);
    REQUIRE(c.glob_v == 55);
    REQUIRE(c.own_v == 32);
    REQUIRE(s.glob_v == 999);
    REQUIRE(s.own_v == 999);

    fire(s.own.ev, 45);
    REQUIRE(s.own_v == 45);
    REQUIRE(c.own_v == 32);

    fire(ev_global, 77);
    REQUIRE(c.glob_v == 77);
    REQUIRE(s.glob_v == 77);
  }

  fire(ev_global, 5);
  fire(s.own.ev, 4);
  REQUIRE(s.glob_v == 5);
  REQUIRE(s.own_v == 4);
}

}
