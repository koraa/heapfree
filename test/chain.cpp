#include <iterator>
#include <catch2/catch.hpp>
#include "hardwave/heapfree/chain.hpp"

namespace {
using namespace hardwave::heapfree;

struct test_struct {
  int a{99};
  bool b{true};
  char c{'x'};

  bool copied{false}, moved{false};

  test_struct() = default;
  test_struct(int av, bool bv, char cv)
    : a{av}, b{bv}, c{cv} {}

  test_struct(const test_struct &otr) { *this = otr; }
  test_struct& operator=(const test_struct &otr) {
    a = otr.a;
    b = otr.b;
    c = otr.c;
    copied = true;
    moved = false;
    return *this;
  }

  test_struct(test_struct &&otr) { *this = std::move(otr); }
  test_struct& operator=(test_struct &&otr) {
    a = otr.a;
    b = otr.b;
    c = otr.c;
    copied = false;
    moved = true;
    return *this;
  }
};

static chain<test_struct> ch;
using ch_segment = typename decltype(ch)::segment;

void ck_empty() {
  REQUIRE(std::size(ch) == 0);
  REQUIRE(std::empty(ch));
  REQUIRE(std::size(ch.segments()) == 0);
  REQUIRE(std::empty(ch.segments()));

  auto &ptrs = reinterpret_cast<detail::chain_ptr&>(ch);
  REQUIRE(ptrs.next == &ptrs);
  REQUIRE(ptrs.prev == &ptrs);
}

TEST_CASE("chain segment default construction") {
  ch_segment seg;
  REQUIRE(seg->a == 99);
  REQUIRE(seg->b == true);
  REQUIRE(seg->c == 'x');
  REQUIRE(!seg->copied);
  REQUIRE(!seg->moved);
  REQUIRE(!seg.is_linked());
}

TEST_CASE("chain segment value move construction") {
  ch_segment seg{test_struct{20, false, 'k'}};
  REQUIRE(seg->a == 20);
  REQUIRE(seg->b == false);
  REQUIRE(seg->c == 'k');
  REQUIRE(!seg->copied);
  REQUIRE(seg->moved);
  REQUIRE(!seg.is_linked());
}

TEST_CASE("chain segment value copy construction") {
  test_struct v{10, false, 'w'};
  ch_segment seg{v};
  REQUIRE(seg->a == 10);
  REQUIRE(seg->b == false);
  REQUIRE(seg->c == 'w');
  REQUIRE(seg->copied);
  REQUIRE(!seg->moved);
  REQUIRE(!seg.is_linked());
}

TEST_CASE("chain segment in place construction") {
  ch_segment seg{std::in_place, 5, true, 'm'};
  REQUIRE(seg->a == 5);
  REQUIRE(seg->b == true);
  REQUIRE(seg->c == 'm');
  REQUIRE(!seg->copied);
  REQUIRE(!seg->moved);
  REQUIRE(!seg.is_linked());
}

TEST_CASE("chain empty") {
  ck_empty();
}

TEST_CASE("chain link & unlink") {
  ck_empty();
  {
    ch_segment a{}, b{}, c{}, d{};
    auto ia = ch.link_front(a);
    [[maybe_unused]] auto ib = ch.link_front(b);
    auto ic = ch.link_back(c);
    auto id = ch.link(ic, d);

    REQUIRE(!std::empty(ch));
    REQUIRE(std::size(ch) == 4);

    REQUIRE(&ch[0] == &b.value());
    REQUIRE(&ch[1] == &a.value());
    REQUIRE(&ch[2] == &d.value());
    REQUIRE(&ch[3] == &c.value());

    // Iterators stay valid despite insertation
    REQUIRE(&std::next(ia).value() == &id.value());

    auto id2 = ch.unlink(ia);
    REQUIRE(&id2.value() == &id.value());
    REQUIRE(!std::empty(ch));
    REQUIRE(std::size(ch) == 3);
    REQUIRE(!a.is_linked());

    b.unlink();
    REQUIRE(std::size(ch) == 2);
    REQUIRE(!b.is_linked());

    REQUIRE_THROWS(b.unlink());
  }
  // Should be automatically removed when segments go out of scope
  ck_empty();
}

TEST_CASE("chain segment move construction") {
  ck_empty();

  ch_segment a{}, b{}, c{};
  ch.link_back(a);
  ch.link_back(b);
  REQUIRE(std::size(ch) == 2);
  REQUIRE(&ch.segments()[0] == &a);
  REQUIRE(&ch.segments()[1] == &b);

  c = std::move(a);
  REQUIRE(std::size(ch) == 2);
  REQUIRE(&ch.segments()[0] == &c);
  REQUIRE(&ch.segments()[1] == &b);
  REQUIRE(c.is_linked());
  REQUIRE(!a.is_linked());

  ch_segment d{std::move(b)};
  REQUIRE(std::size(ch) == 2);
  REQUIRE(&ch.segments()[0] == &c);
  REQUIRE(&ch.segments()[1] == &d);
  REQUIRE(d.is_linked());
  REQUIRE(!b.is_linked());

  c = std::move(d);
  REQUIRE(std::size(ch) == 1);
  REQUIRE(&ch.segments()[0] == &c);
  REQUIRE(!d.is_linked());
  REQUIRE(c.is_linked());

  c = std::move(b);
  REQUIRE(std::empty(ch));
  REQUIRE(!c.is_linked());
}

TEST_CASE("chain segment swap") {
  ch_segment a{}, b{}, c{};
  ch.link_back(a);
  ch.link_back(b);
  REQUIRE(std::size(ch) == 2);
  REQUIRE(&ch.segments()[0] == &a);
  REQUIRE(&ch.segments()[1] == &b);

  std::swap(a, b);
  REQUIRE(std::size(ch) == 2);
  REQUIRE(&ch.segments()[0] == &b);
  REQUIRE(&ch.segments()[1] == &a);

  std::swap(a, c);
  REQUIRE(std::size(ch) == 2);
  REQUIRE(&ch.segments()[0] == &b);
  REQUIRE(&ch.segments()[1] == &c);
  REQUIRE(c.is_linked());
  REQUIRE(!a.is_linked());
}

TEST_CASE("chain place") {
  test_struct v{600, true, 'b'};

  auto a = ch.place_front(); // default construction
  auto b = ch.place_front(test_struct{400, true, 'a'}); // move construction
  auto c = ch.place_back(v); // copy construction
  auto d = ch.place(unsafe_make_chain_it(ch, c), std::in_place, 800, false, 'c');
  REQUIRE(b->a == 400);
  REQUIRE(&ch.segments()[0] == &b);
  REQUIRE(a->a == 99);
  REQUIRE(&ch.segments()[1] == &a);
  REQUIRE(d->a == 800);
  REQUIRE(&ch.segments()[2] == &d);
  REQUIRE(c->a == 600);
  REQUIRE(&ch.segments()[3] == &c);
}

TEST_CASE("chain front & back & numeric access") {
  chain<int> ch2;

  auto a = ch2.place_back();
  auto b = ch2.place_back();
  REQUIRE(&ch2.front() == &a.value());
  REQUIRE(&ch2.back() == &b.value());
  REQUIRE(&ch2[0] == &a.value());
  REQUIRE(&ch2[1] == &b.value());

  ch2.front() = 200;
  ch2.back() = 600;
  REQUIRE(a.value() == 200);
  REQUIRE(b.value() == 600);

  ch2[0] = 700;
  ch2[1] = 900;
  REQUIRE(a.value() == 700);
  REQUIRE(b.value() == 900);
}

TEST_CASE("chain clear") {
  auto a = ch.place_back();
  auto b = ch.place_back();
  auto c = ch.place_back();
  REQUIRE(c.is_linked());
  REQUIRE(std::size(ch) == 3);

  ch.clear();
  REQUIRE(!c.is_linked());
  REQUIRE(std::empty(ch));
}

TEST_CASE("chain swap") {
  decltype(ch) ch2;
  auto a = ch.place_back();
  auto b = ch.place_back();
  auto c = ch.place_back();
  REQUIRE(std::size(ch) == 3);
  REQUIRE(std::empty(ch2));

  std::swap(ch, ch2);
  REQUIRE(std::size(ch2) == 3);
  REQUIRE(std::empty(ch));

  REQUIRE(&ch2.segments()[2] == &c);
}

TEST_CASE("chain move construct") {
  auto a = ch.place_back();
  auto b = ch.place_back();
  REQUIRE(std::size(ch) == 2);

  decltype(ch) ch2{std::move(ch)};
  REQUIRE(std::empty(ch));
  REQUIRE(std::size(ch2) == 2);
}

TEST_CASE("chain move assign") {
  decltype(ch) ch2;
  auto a = ch.place_back();
  auto b = ch.place_back();
  auto c = ch2.place_back();
  REQUIRE(std::size(ch) == 2);
  REQUIRE(std::size(ch2) == 1);

  ch2 = std::move(ch);
  REQUIRE(std::empty(ch));
  REQUIRE(std::size(ch2) == 2);
  REQUIRE(!c.is_linked());
}

TEST_CASE("chain segments()") {
  auto a = ch.place_back();
  auto b = ch.place_back();
  REQUIRE(std::size(ch.segments()) == 2);
  REQUIRE(!std::empty(ch.segments()));
  REQUIRE(&ch.segments()[0] == &a);
  REQUIRE(&ch.segments()[1] == &b);
}

TEST_CASE("chain iterator default constructor compares equal to itself") {
  using It = typename decltype(ch)::iterator;
  REQUIRE(It{} == It{});
  REQUIRE(It{} != ch.end());
}

TEST_CASE("chain iterator increment & dereference") {
  auto a = ch.place_back();
  auto b = ch.place_back();
  auto c = ch.place_back();

  auto it = std::begin(ch);
  REQUIRE(&(*it++) == &a.value());
  REQUIRE(&it->a == &b->a);
  REQUIRE(&it.value() == &b.value());
  REQUIRE(&it.segment() == &b);
  REQUIRE(&(*it) == &b.value());
  REQUIRE(&(*++it) == &c.value());
  REQUIRE(&(*it++) == &c.value());

  // End iterator ops
  REQUIRE_THROWS(*it);
  REQUIRE_THROWS(it->a);
  REQUIRE_THROWS(it.value());
  REQUIRE_THROWS(it.segment());
  REQUIRE_THROWS(it++);
  REQUIRE_THROWS(++it);
}

TEST_CASE("chain iterator decrement & dereference") {
  auto a = ch.place_back();
  auto b = ch.place_back();
  auto c = ch.place_back();

  auto it = ch.end();

  // End iterator ops
  REQUIRE_THROWS(*it);
  REQUIRE_THROWS(it->a);
  REQUIRE_THROWS(it.value());
  REQUIRE_THROWS(it.segment());

  REQUIRE(&(*--it) == &c.value());
  REQUIRE(&(*it--) == &c.value());
  REQUIRE(&(*it) == &b.value());
  REQUIRE(&it.value() == &b.value());

  it--;
  // Begin op can not be decremented
  REQUIRE_THROWS(it--); // Note that it behaviour is undefined at this point
  REQUIRE_THROWS(--ch.begin());
}

TEST_CASE("chain iterator bidirectional multipass") {
  auto a = ch.place_back();
  auto b = ch.place_back();
  auto c = ch.place_back();

  auto it = std::begin(ch);
  it++;
  it++;
  REQUIRE(&it.value() == &c.value());

  it--;
  REQUIRE(&it.value() == &b.value());

  auto it2{it};
  REQUIRE(&it2.value() == &b.value());
  it2--;
  REQUIRE(&it2.value() == &a.value());

  it2++;
  it2++;
  REQUIRE(&it2.value() == &c.value());
}

TEST_CASE("chain iterator comparison ") {
  REQUIRE(ch.begin() == ch.end());

  auto a = ch.place_back();
  REQUIRE(ch.begin() != ch.end());
  REQUIRE(ch.end() == ch.end());

  REQUIRE(unsafe_make_chain_it(ch, a) == ch.begin());
  REQUIRE(std::prev(ch.end()) == ch.begin());
}

TEST_CASE("chain iterator traits") {
  using traits = std::iterator_traits<typename chain<int>::iterator>;
  using const_traits = std::iterator_traits<typename chain<int>::const_iterator>;
  static_assert(std::is_same_v<std::ptrdiff_t, traits::difference_type>);
  static_assert(std::is_same_v<std::ptrdiff_t, const_traits::difference_type>);

  static_assert(std::is_same_v<int, traits::value_type>);
  static_assert(std::is_same_v<int, const_traits::value_type>);

  static_assert(std::is_same_v<int&, traits::reference>);
  static_assert(std::is_same_v<const int&, const_traits::reference>);
}

}
