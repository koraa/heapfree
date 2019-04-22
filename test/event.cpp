#include <catch2/catch.hpp>
#include "hardwave/heapfree/event.hpp"

namespace hardwave::heapfree {

TEST_CASE("event can be registered & called with different lambdas") {
  event<int, int, int> ev;

  int ctr0{0};
  auto a = on(ev, [&ctr0](int x, int y, int z) {
    ctr0 += x + y + z;;
  });

  int ctr1{0}, ctr2{0};
  auto b = on(ev, [&ctr1, &ctr2](int x, int y, int z) {
    ctr1 -= (x + y + z);
    ctr2 += x;
  });

  long ctr3{0};
  auto c = on(ev, [&ctr3](int, int, int) {
    ctr3 += 1;
  });

  fire(ev, 1, 2, 3);
  REQUIRE(ctr0 == 6);
  REQUIRE(ctr1 == -6);
  REQUIRE(ctr2 == 1);
  REQUIRE(ctr3 == 1);

  fire(ev, 4, 5, 6);
  REQUIRE(ctr0 == 21);
  REQUIRE(ctr1 == -21);
  REQUIRE(ctr2 == 5);
  REQUIRE(ctr3 == 2);
}

TEST_CASE("fire() throws if there are no event listeners") {
  event<int> ev;
  try_fire(ev, 42); // Works & is no-op
  REQUIRE_THROWS(fire(ev, 42));
}

}
