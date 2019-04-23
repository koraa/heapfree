#pragma once
#include <cstddef>
#include <iterator>

namespace hardwave {
namespace heapfree {

/// The iterator range can be used wrap any pair of iterators into a
/// range/container like interface.
///
/// Bidirectional and random access iterators are supported (Input & Output).
/// Forward and Input iterators might be supported, but are untested.
///
/// The usual range interfaces (size, empty, [] operator, begin, end) are supported.
/// In addition to that slice() is supported
/// size, slice and [] are O(N) for bidirectional iterators and O(1) for random access iterators.
///
/// # Example
///
/// ```
/// #include <iostream>
/// #include <vector>
/// #include "hardwave/heapfree/iterator_range.hpp"
///
/// using namespace hardwave::heapfree;
///
/// int main() {
///   std::vector<int> v{1,2,3,4,5,6,7,8,9,10};
///   iterator_range r{v.begin() + 2, v.end() - 3};
///
///   for (const auto &i : r)
///     std::cerr << i << ", ";
///   std::cerr << "\n";
///
///   return 0;
/// }
/// ```
///
/// Output:
///
/// ```
/// 3, 4, 5, 6, 7,
/// ```
template<typename Begin, typename End>
class iterator_range {
  using me_t = iterator_range<Begin, End>;
  Begin b;
  End e;

  using ITraits = std::iterator_traits<Begin>;
public:
  using value_type = typename ITraits::value_type;
  using size_type = size_t;
  using difference_type = typename ITraits::difference_type;
  using reference = typename ITraits::reference;
  using pointer = typename ITraits::pointer;
  using iterator = Begin;

  iterator_range() = default;

  iterator_range(const Begin &beg, const End &en)
    : b{beg}, e{en} {}

  template<typename Range>
  iterator_range(Range &r) : b{std::begin(r)}, e{std::end(r)} {}

  iterator_range(const me_t&) = default;
  iterator_range& operator=(const me_t&) = default;

  iterator_range(me_t&&) = default;
  iterator_range& operator=(me_t&&) = default;

  size_type size() const {
    if (empty()) { // To support value initialized iterators
      return 0;
    } else {
      return std::distance(b, e);
    }
  }
  bool empty() const { return b == e; }

  Begin begin() const { return b; }
  End end() const { return e; }

  reference front() const { return *b; }
  reference back() const { return *std::prev(end()); }

  reference operator[](size_type idx) {
    auto it = begin();
    std::advance(it, idx);
    return *it;
  }
  reference operator[](size_type idx) const {
    return const_cast<me_t&>(*this)[idx];
  }
};

template<typename Range>
iterator_range(Range&r) -> iterator_range<decltype(std::begin(r)), decltype(std::end(r))>;

} // namespace heapfree
} // namespace heapfre
