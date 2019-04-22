#include <catch2/catch.hpp>
#include <memory>
#include <iterator>
#include <type_traits>
#include <algorithm>
#include <vector>
#include <list>
#include <utility>
#include "hardwave/heapfree/iterator_range.hpp"

namespace {
using namespace hardwave::heapfree;

// should work for forward, bidirectional iterators
static std::vector vec{42, 23, 99, 5, 20};
static std::list lst{42, 23, 99, 5, 20};

template<typename Fn>
void for_each_store(Fn fn) {
  fn(vec);
  fn(lst);

  // Copying
  iterator_range vr{vec};
  fn(vr);
  iterator_range lr{lst};
  fn(lr);
}

template<typename Fn>
void for_each_const_store(Fn fn) {
  for_each_store([&](auto &store) {
    const auto &cstore = store;
    fn(store);
    fn(cstore);
  });
}

TEST_CASE("iterator_range from range") {
  for_each_const_store([](auto &store) {
    iterator_range ir{store};
    REQUIRE(std::equal(
        std::begin(ir), std::end(ir),
        std::begin(store), std::end(store)));
  });
}

TEST_CASE("iterator_range from iterator") {
  for_each_const_store([](auto &store) {
    iterator_range ir{std::begin(store), std::prev(std::end(store))};
    REQUIRE(std::equal(
        std::begin(ir), std::end(ir),
        std::begin(store), std::prev(std::end(store))));
    REQUIRE(ir.back() == 5);
  });
}

TEST_CASE("iterator_range size & empty") {
  for_each_const_store([](auto &store) {
    iterator_range ir{store};
    REQUIRE(std::size(ir) == std::size(store));
    REQUIRE(std::empty(ir) == std::empty(store));

    decltype(ir) empty;
    REQUIRE(std::size(empty) == 0);
    REQUIRE(std::empty(empty));

    std::remove_reference_t<decltype(store)> empty_store{};
    iterator_range empty2{empty_store};
    REQUIRE(std::size(empty2) == 0);
    REQUIRE(std::empty(empty2));
  });
}

TEST_CASE("iterator_range front() & back()") {
  for_each_const_store([](auto &store) {
    iterator_range r{store};
    REQUIRE(r.front() == store.front());
    REQUIRE(r.back() == store.back());
  });

  int ctr = 1000;
  for_each_store([&](auto &store) {
    iterator_range it{store};
    auto i1 = it.front() = ctr++;
    auto i2 = it.back() = ctr++;
    REQUIRE(it.front() == store.front());
    REQUIRE(it.front() == i1);
    REQUIRE(it.back() == store.back());
    REQUIRE(it.back() == i2);
  });
}

TEST_CASE("iterator_range integer access") {
  for_each_const_store([](auto &store) {
    iterator_range it{store};
    for (size_t i=0; i < std::size(store); i++) {
      auto it2 = std::begin(store);
      std::advance(it2, i);
      REQUIRE(&it[i] == &*it2);
    }
  });

  int ctr = 2000;
  for_each_store([&](auto &store) {
    iterator_range it{store};
    for (size_t i=0; i < std::size(store); i++) {
      auto val = it[i] = ctr++;
      auto it2 = std::begin(store);
      std::advance(it2, i);
      REQUIRE(it[i] == *it2);
      REQUIRE(it[i] == val);
    }
  });
}

enum {
  empty_flags = 0,
  move_src = 1 << 0,
  copy_src = 1 << 1,
  move_target = 1 << 2,
  copy_target = 1 << 3
};

struct tstruct {
  unsigned int flags{empty_flags};

  tstruct() = default;

  tstruct(const tstruct &otr) { *this = otr; }
  tstruct& operator=(const tstruct &otr) {
    const_cast<tstruct&>(otr).flags |= copy_src;
    flags |= copy_target;
    return *this;
  }

  tstruct(tstruct &&otr) { *this = std::move(otr); }
  tstruct& operator=(tstruct &&otr) {
    otr.flags |= move_src;
    flags |= move_target;
    return *this;
  }
};

}
