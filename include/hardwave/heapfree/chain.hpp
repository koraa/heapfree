#pragma once
#include <utility>
#include <iterator>
#include <type_traits>
#include "hardwave/heapfree/meta.hpp"
#include "hardwave/heapfree/error.hpp"
#include "hardwave/heapfree/iterator_range.hpp"

namespace hardwave {
namespace heapfree {

namespace detail {

enum class chain_iterator_mode {
  values, segments, ptrs
};

template<typename, bool, chain_iterator_mode Mode>
class chain_iterator;

struct chain_ptr {
  chain_ptr *next{nullptr}, *prev{nullptr};

  chain_ptr& ptrs() { return *this; }
  const chain_ptr& ptrs() const { return *this; }

  void swap(chain_ptr &otr) {
    std::swap(next, otr.next);
    std::swap(prev, otr.prev);
  }
};

/// This type stores the actual data contained in chains.
/// The type for a specific chain can be accessed through
/// `decltype(chain)::segment`. The user constructs and
/// allocates this type themselves and links it *into* a
/// chain.
/// This type acts like a pointer. The value can be accessed through * and ->.
///
/// Chain segments are unlinked from their chain, when they go out of scope.
template<typename Chain>
class chain_segment : private chain_ptr {
  HEAPFREE_DECLARE_ME_SUPER(chain_segment<Chain>, chain_ptr)

  typename Chain::value_type payload;

  void fix_foreign_links() {
    if (!is_linked()) return;
    next->prev = &ptrs();
    prev->next = &ptrs();
  }
public:
  using chain_type = Chain;

  template<typename, bool, detail::chain_iterator_mode>
  friend class detail::chain_iterator;
  friend chain_type;
  friend typename chain_type::iterator;

  chain_segment() = default;

  chain_segment(const me_t&) = default;
  chain_segment& operator=(const me_t&) = default;

  /// Chain segments can be move constructed.
  /// In this case the payload AND the links are moved,
  /// the source segment is UNLINKED.
  chain_segment(me_t &&otr) : super_t{otr.super()}, payload{std::move(otr.payload)} {
    otr.next = otr.prev = nullptr;
    fix_foreign_links();
  }
  chain_segment& operator=(me_t &&otr) {
    if (is_linked())
      unlink();
    super() = otr.super();
    payload = std::move(otr.payload);
    otr.next = otr.prev = nullptr;
    fix_foreign_links();
    return me();
  }

  chain_segment(typename Chain::const_reference v) : payload{v} {}
  typename Chain::reference& operator=(typename Chain::const_reference v) {
    payload = v;
    return payload;
  }

  chain_segment(typename Chain::value_type &&v) : payload{std::move(v)} {}
  typename Chain::reference operator=(typename Chain::value_type &&v) {
    payload = std::move(v);
    return payload;
  }

  template<typename... Args>
  chain_segment(std::in_place_t, Args&&... args)
    : payload{std::forward<Args>(args)...} {}

  ~chain_segment() {
    if (is_linked())
      unlink();
  }

  /// Swapping swaps both the payload AND the links
  void swap(me_t &otr) {
    std::swap(super(), otr.super());
    std::swap(payload, otr.payload);
    fix_foreign_links();
    otr.fix_foreign_links();
  }

  /// Return the value stored in this segment
  typename Chain::reference value() { return payload; }
  typename Chain::const_reference value() const { return payload; }

  typename Chain::reference operator*() { return payload; }
  typename Chain::const_reference operator*() const { return payload; }

  typename Chain::pointer operator->() { return &payload; }
  typename Chain::const_pointer operator->() const { return &payload; }

  /// Check if this segment is part of some chain
  bool is_linked() const {
    return next != nullptr;
  }

  void unlink() {
    HEAPFREE_ASSERT(is_linked(), "Cannot unlink a segment that is not linked.");
    next->prev = prev;
    prev->next = next;
    next = prev = nullptr;
  }
};

struct chain_iterator_no_tag {};

/// Iterator over chains.
/// Returned by begin()/end() and segments().begin()/end()
/// This is a bidirectional iterator.
///
/// The iterator stays valid, as long as the segment exists and
/// is part of the given chain.
/// Even if the segment is unlinked from the chain, and then linked
/// in some other place again, the iterator becomes valid again.
template<typename Chain, bool Const, chain_iterator_mode Mode = chain_iterator_mode::values>
class chain_iterator :
      public std::bidirectional_iterator_tag,
      public std::conditional_t<Const,
        std::output_iterator_tag, chain_iterator_no_tag> {
  using me_alias = chain_iterator<Chain, Const, Mode>;
  HEAPFREE_DECLARE_ME(me_alias);

  template<typename, bool, detail::chain_iterator_mode>
  friend class detail::chain_iterator;
  friend Chain;
  friend typename Chain::segment;

  using chain_t = std::conditional_t<Const, const Chain, Chain>;
  using seg_t = std::conditional_t<Const, const typename chain_t::segment, typename chain_t::segment>;
  using ptr_t = std::conditional_t<Const, const chain_ptr, chain_ptr>;

  chain_t *ch{nullptr};
  ptr_t *pt{nullptr}; // Make sure thid

  chain_iterator(chain_t &c, ptr_t &p) : ch{&c}, pt{&p} {}

  static me_t create_end(chain_t &ch) {
    return chain_iterator{ch, ch.ptrs()};
  }

  bool chain_is_valid() {
    if (pt->next == nullptr)
      return false;
    for (chain_ptr *p{pt->next}; true; p = p->next) {
      if (p == pt) return false;
      if (p == &ch->ptrs()) return true;
    }
  }

  void assert_nonull(std::string_view activity) const {
    HEAPFREE_ASSERT(pt != nullptr && ch != nullptr,
        "Cannot ", activity, " a null chain operator");
  }

  chain_t& chain() { return *ch; }
  const chain_t& chain() const { return *ch; }

public:
  using difference_type = std::ptrdiff_t;
  using value_type =
    std::conditional_t<Mode == chain_iterator_mode::values, typename chain_t::value_type,
      std::conditional_t<Mode == chain_iterator_mode::segments, typename chain_t::segment,
        chain_ptr>>;
  using pointer = std::conditional_t<Const, const value_type*, value_type*>;
  using reference = std::conditional_t<Const, const value_type&, value_type&>;
  using iterator_category = std::bidirectional_iterator_tag;

  chain_iterator() = default;

  /// When creating an iterator from a segment, the constructor
  /// checks that the segment is actually part of the given chain.
  /// This is accomplished in O(N).
  /// If you are absolutely sure, the segment IS part of that chain,
  /// and the check is not necessary, unsafe_create can be used.
  chain_iterator(chain_t &c, seg_t &seg) : me_t{c, seg.ptrs()} {
    HEAPFREE_ASSERT(chain_is_valid(), "Trying to create a chain ",
        "iterator with a segment that is not part of that chain?");
  }

  /// Create a iterator to a chain, without making sure that the
  /// segment is REALLY part of that chain.
  /// Note that, if the segment is not part of the chain, the
  /// result is undefined/broken (do not do this).
  static me_t unsafe_create(chain_t &c, seg_t &seg) {
    return {c, seg.ptrs()};
  }

  template<bool Const2>
  chain_iterator(const chain_iterator<Chain, Const2, Mode> &otr) {
    me() = otr;
  }

  template<bool Const2>
  me_t& operator=(const chain_iterator<Chain, Const2, Mode> &otr) {
    static_assert(Const || Const == Const2, "Cannot copy a const chain iterator "
        "to one that is not const.");
    ch = otr.ch;
    pt = otr.pt;
    return me();
  }

  void swap(me_t &otr) {
    std::swap(ch, otr.ch);
    std::swap(pt, otr.pt);
  }

  bool is_end() const {
    assert_nonull("call is_end()");
    return &ch->ptrs() == &ptrs();
  }

private:
  ptr_t& ptrs() {
    assert_nonull("dereference");
    return *pt;
  }
  const ptr_t& ptrs() const {
    return const_cast<me_t&>(me()).ptrs();
  }

public:
  /// Return the segment this iterator points to, even if the iterator is
  /// a chain-value iterator
  seg_t& segment() {
    assert_nonull("dereference");
    HEAPFREE_ASSERT(!is_end(), "Cannot dereference chain iterator: its at the end");
    return static_cast<seg_t&>(ptrs());
  }
  const seg_t& segment() const {
    return const_cast<me_t&>(me()).segment();
  }

  /// Return the value this iterator points to, even if the iterator is
  /// a chain-segment iterator
  typename chain_t::reference value() { return segment().value(); }
  typename chain_t::const_reference value() const { return segment().value(); }

  auto& operator*() {
    if constexpr(Mode == chain_iterator_mode::values)
      return value();
    else if constexpr(Mode == chain_iterator_mode::segments)
      return segment();
    else if constexpr(Mode == chain_iterator_mode::ptrs)
      return ptrs();
  }

  const auto& operator*() const {
    return *const_cast<me_t&>(me());
  }

  auto* operator->() { return &*me(); }
  const auto* operator->() const { return &*me(); }

  me_t& operator--() {
    assert_nonull("decrement");
    pt = pt->prev;
    HEAPFREE_ASSERT(!is_end(), "Can not decrement begin() iterator.");
    return me();
  }
  me_t& operator++() {
    assert_nonull("increment");
    HEAPFREE_ASSERT(!is_end(), "Can not increment end() iterator.");
    pt = pt->next;
    return me();
  }

  me_t operator--(int) {
    me_t r{me()};
    --me();
    return r;
  }

  me_t operator++(int) {
    me_t r{me()};
    ++me();
    return r;
  }

  template<bool Const2>
  bool operator==(const chain_iterator<Chain, Const2, Mode> &otr) const {
    return otr.ch == ch && otr.pt == pt;
  }

  template<typename T>
  bool operator!=(const T &otr) const {
    return !(me() == otr);
  }
};

} // namespace detail

/// A chain is a linked list that does not manage it's own memory,
/// instead the caller allocates the memory for the element they want
/// to store themselves (using the segment type) and then just link the
/// segment into the chain.
///
/// This is - in essence - a way to give the user a maximum of control
/// about how the memory is allocated; the segments may be stored on the
/// heap, stack or be globally allocated - it's up to the user of the chain.
///
/// This is useful because we can not use heap allocation in our environment;
/// using a chain allows us to have arbitrarily large chains (stored on the
/// stack probably), without preallocating a fixed number of elements as an
/// array<> somewhere.
///
/// On destruction all segments are detached from the chain.
///
/// # Example
///
/// ```c++
/// #include <iostream>
/// #include "hardwave/heapfree/chain.hpp"
///
/// using namespace hardwave::heapfree;
///
/// int main() {
///   chain<int> my_chain;
///
///   auto elm1 = my_chain.place_back(42);
///   auto elm2 = my_chain.place_back(5);
///   auto elm3 = my_chain.place_back(13);
///
///   std::cerr << "Second elm: " << my_chain[1] << "\n";
///
///   my_chain[1] = 10;
///
///   std::cerr << "Chain elms: ";
///   for (const auto &v : my_chain)
///     std::cerr << v << ", ";
///   std::cerr << "\n";
///
///   return 0;
/// }
/// ```
///
/// Outputs:
///
/// ```
/// Second elm: 5
/// Chain elms: 42, 10, 13,
/// ```
///
/// # Iterators & pointers
///
/// Iterators are valid as long as the segment pointed to is part of the
/// iterator's chain. Even if the element is unlinked from the chain and linked
/// into it again, the iterator stays valid.
///
/// Pointers to values & segments stay valid until the segment goes out of scope.
///
/// # Type erasure using chains
///
/// An added advantage of this structure is that each segment could
/// contain a different type, without needing to support actual runtime polymorphism (vtables).
/// The event class uses is to store various
/// lambda types in a chain of event listeners.
/// The technique is to store some kind of header (in the case of event this
/// is a function pointer but it could be an enum too) and then store the custom
/// data just past the header:
///
/// ```
/// enum class segment_type { longs, chars, strs };
/// chain<segment_type> my_chain;
/// using segment = typename decltype(my_chain)::segment;
///
/// template<typename T, segment_type HeaderVal>
/// struct custom_seg : segment {
///   T payload;
///   custom_seg() : segment{HeaderVal} {}
/// };
///
/// using long_seg = custom_seg<long, segment_type::longs>;
/// using int_seg = custom_seg<char, segment_type::chars>;
/// using str_seg = custom_seg<char[], segment_type::strs>;
///
/// int main() {
///   long_seg a;
///   ch.link_back(my_chain, static_cast<segment>(a));
///
///   str_seg b;
///   ch.link_back(my_chain, static_cast<segment>(b));
///
///   for (auto &seg = my_chain.segments()) {
///     switch (seg.value()) {
///       case segment_type::longs:
///         long &payload = static_cast<long_seg&>(seg);
///         ...
///       case segment_type::chars:
///         char &payload = static_cast<char_seg&>(seg);
///         ...
///       case segment_type::strs:
///         char &payload[5] = static_cast<str_seg&>(seg);
///         ...
///     };
///   }
/// }
/// ```
template<typename T>
class chain : private detail::chain_ptr {
  HEAPFREE_DECLARE_ME_SUPER(chain<T>, detail::chain_ptr)

  // does not work if the chain is empty!
  void fix_moved_ptrs() {
    next->prev = prev->next = &ptrs();
  }

public:
  using value_type      = T;
  using size_type       = size_t;
  using difference_type = std::ptrdiff_t;
  using reference       = value_type&;
  using pointer         = value_type*;
  using iterator        = detail::chain_iterator<me_t, false>;
  using const_reference = const value_type&;
  using const_pointer   = const value_type*;
  using const_iterator  = detail::chain_iterator<me_t, true>;

  /// The segment type is allocated by the user and stores the actual data
  /// See detail::chain_segment
  using segment = detail::chain_segment<me_t>;

  template<typename, bool, detail::chain_iterator_mode>
  friend class detail::chain_iterator;
  friend segment;

  /// At the start a chain is empty
  chain() {
    next = prev = &ptrs();
  }

  ~chain() {
    clear();
  }

  chain(const me_t&) = delete;
  me_t& operator=(const me_t&) = delete;

  chain(me_t &&otr) : chain{} {
    me() = std::move(otr);
  }

  me_t& operator=(me_t &&otr) {
    clear();
    if (!std::empty(otr)) {
      super() = otr.super();
      fix_moved_ptrs();
      otr.next = otr.prev = &otr.ptrs();
    }
    return me();
  }

  void swap(me_t &otr) {
    // Need special handling if one is empty,
    // because fix_moved_ptrs() can only fix
    // nonempty chains
    if (empty()) {
      me() = std::move(otr);
    } else if (std::empty(otr)) {
      otr = std::move(me());
    } else {
      std::swap(super(), otr.super());
      fix_moved_ptrs();
      otr.fix_moved_ptrs();
    }
  }

  // Size is O(N)
  size_t size() const { return std::distance(begin(), end()); }
  bool empty() const { return next == &ptrs(); }

  /// This can be used to link an existing segment into the chain.
  /// The segment must not be linked for this
  iterator link(const_iterator it, segment &seg) {
    HEAPFREE_ASSERT(!seg.is_linked(), "");
    HEAPFREE_ASSERT(&it.chain() == this, "");
    auto &sis = seg.ptrs();
    auto &p = const_cast<detail::chain_ptr&>(*it.ptrs().prev);
    auto &n = const_cast<detail::chain_ptr&>(it.ptrs());

    sis.prev = &p;
    sis.next = &n;
    n.prev = &sis;
    p.next = &sis;

    return iterator::unsafe_create(me(), seg);
  }

  iterator link_back(segment &seg) {
    return link(end(), seg);
  }

  iterator link_front(segment &seg) {
    return link(begin(), seg);
  }

  /// Unlinks a single segment from the chain;
  /// returns an iterator just after the one that was removed.
  iterator unlink(iterator it) {
    HEAPFREE_ASSERT(&it.chain() == &me(), "");
    auto r = std::next(it);
    it.segment().unlink();
    return r;
  }

  /// Unlinks *all* segments from the list
  void clear() {
    detail::chain_ptr *cur{next}, *nx;
    next = prev = &ptrs(); // Make us self referential
    while (cur != &ptrs()) {
      nx = cur->next;
      cur->next = cur->prev = nullptr;
      cur = nx;
    }
  }

  /// Construct and link a segment in one go.
  /// The parameters are forwarded to the segment constructor.
  ///
  /// ```
  /// auto my_segment = my_chain.place_back(...);
  /// ```
  template<typename... Args>
  [[nodiscard]] segment place(const_iterator it, Args&&... args) {
    segment seg{std::forward<Args>(args)...};
    link(it, seg);
    return seg;
  }

  template<typename... Args>
  [[nodiscard]] segment place_front(Args&&... args) {
    return place(begin(), std::forward<Args>(args)...);
  }

  template<typename... Args>
  [[nodiscard]] segment place_back(Args&&... args) {
    return place(end(), std::forward<Args>(args)...);
  }

  iterator begin() { return {me(), *next}; }
  iterator end() { return iterator::create_end(me()); }

  const_iterator begin() const { return {me(), *next}; }
  const_iterator end() const { return const_iterator::create_end(me()); }

  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *std::prev(end()); }
  const_reference back() const { return *(end() - 1); }

  /// This is a linked list, so numeric access is O(N)
  reference operator[](size_type idx) {
    return iterator_range{me()}[idx];
  }
  const_reference operator[](size_type idx) const {
    return const_cast<me_t&>(*this)[idx];
  }

  /// Constructs an iterator_range, that can be used to iterate over all
  /// segments in the chain, instead of the values
  auto segments() {
    using It = detail::chain_iterator<me_t, false, detail::chain_iterator_mode::segments>;
    return iterator_range{It{me(), *next}, It::create_end(me())};
  }
  auto segments() const {
    using It = detail::chain_iterator<me_t, true, detail::chain_iterator_mode::segments>;
    return iterator_range{It{me(), *next}, It::create_end(me())};
  }
};

template<typename Chain>
typename Chain::iterator make_chain_it(Chain &ch, typename Chain::segment &seg) {
  return {ch, seg};
}

template<typename Chain>
typename Chain::const_iterator make_chain_it(const Chain &ch, const typename Chain::segment &seg) {
  return {ch, seg};
}

template<typename Chain>
typename Chain::iterator unsafe_make_chain_it(Chain &ch, typename Chain::segment &seg) {
  return Chain::iterator::unsafe_create(ch, seg);
}

template<typename Chain>
typename Chain::const_iterator unsafe_make_chain_it(const Chain &ch, const typename Chain::segment &seg) {
  return Chain::const_iterator::unsafe_create(ch, seg);
}

} // namespace heapfree
} // namespace hardwave
