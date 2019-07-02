# Heapfree

Linked lists & event based programming for embedded environments in which the heap should not be used:

Heapfree is a C++17 library to facilitate programming without making use of the heap. Use of the heap is generally not discouraged in most software
projects (although heap allocation is still expensive and thus should be minimized); in some applications however heap-free programming can be a very
desirable goal, either because the amount of memory available is very limited or because of hard real-time constraints which do not allow for the unpredictable
latencies of memory allocation.

The data structures and helpers in this library are designed with these systems in mind, but they may prove useful for other applications as well.

# Install

Just add the `include` subdirectory to your list of include paths or copy it into your include directory.
Type `make install` to install this library in /usr/local/include.

## Test

Clone the repository and then just type `make test`.

## Features

* Heap-free doubly linked list (`chain`)
* Heap-free event & event listeners (based on the chain)
* Class methods as event listeners
* Range/Container like wrapper around iterators (`iterator_range`)
* Error handling facilities suitable for an embedded environment
* Modern C++17
* Header only
* Fully tested
* Contract based programming (avoids undefined behaviour)
* Not a single memory allocation in the headers

### Chain

```c++
#include <iostream>
#include "hardwave/heapfree/chain.hpp"

using namespace hardwave::heapfree;

int main() {
  chain<int> my_chain;

  auto elm1 = my_chain.place_back(42);
  auto elm2 = my_chain.place_back(5);
  auto elm3 = my_chain.place_back(13);

  std::cerr << "Second elm: " << my_chain[1] << "\n";

  my_chain[1] = 10;

  std::cerr << "Chain elms: ";
  for (const auto &v : my_chain)
    std::cerr << v << ", ";
  std::cerr << "\n";

  return 0;
}
```

Outputs:

```
Second elm: 5
Chain elms: 42, 10, 13,
```

#### API Selection

See the header files themselves for full api documentation!

```c++
/// Basically a heap-free doubly linked list
/// Instead of the linked list taking care of segment creation and deleteion,
/// the user creates segments on the stack. Segments are part of the chain for
/// their lifetime
template<typename T>
class chain {

  /// This is the type that actually stores the the data inside a chain.
  /// The user creates a chain segment on the stack which is the linked
  /// into the chain and automatically unlinked when the segment is freed
  /// May be moved; references to the chain are automatically updated
  class chain<T>::chain_segment;

  /// This can be used to link an existing segment into the chain.
  /// The segment must not be linked for this
  iterator link(const_iterator it, segment &seg);

  iterator link_back(segment &seg);
  iterator link_front(segment &seg);

  /// Unlinks a single segment from the chain;
  /// returns an iterator just after the one that was removed.
  iterator unlink(iterator it);

  /// Construct and link a segment in one go
  template<typename... Args>
  segment place(const_iterator it, Args&&... args);

  template<typename... Args>
  segment place_front(Args&&... args);

  template<typename... Args>
  segment place_back(Args&&... args);

  /// This is a linked list, so numeric access is O(N)
  reference operator[](size_type idx);
  const_reference operator[](size_type idx) const;

  /// Constructs an iterator_range, that can be used to iterate over all
  /// segments in the chain, instead of the values
  auto segments();
  auto segments() const;

  ...
};

...
```

### Events

```c++
#include <iostream>
#include "hardwave/heapfree/event.hpp"

using namespace hardwave::heapfree;

int main() {
  event<int, int> my_event;

  int counter=0;
  auto listener = on(my_event, [&](int a, int b) {
      std::cerr << "Event handler one was called for the " << ++counter << "nd time: " << a << ", " << b << "\n";
  });

  fire(my_event, 42, 23);
  fire(my_event, 0, 1);

  return 0;
}
```

Output:

```
Event handler one was called for the 1nd time: 42, 23
Event handler one was called for the 2nd time: 0, 1
```

#### API Selection

See the header files themselves for full api documentation!

```c++
/// Events hold a list of event listeners.
/// The template parameters indicate the parameters that listeners receive when called.
template<typename... Args>
class event {
  /// Listeners registered with HEAPFREE_MEMBER_EVENT_LISTENER
  chain_type member_listeners;

  /// Listeners registered with `on()`
  chain_type listeners;
};


/// On can be used to register an event handler.
/// Returns the chain segment that actually stores the event handler;
/// this segment must be kept in scope as long as the handler shall stay
/// registered.
///
/// The segment type is constructed on the fly so various kinds of lambda
/// types can be stored. (This allows us to store lambdas with arbitrary context).
template<typename Lambda, typename... Args>
[[nodiscard]] auto on(event<Args...> &ev, const Lambda &fn);

/// Used to invoke all the event handlers of an event.
/// Returns `true` if at least a single event listener was called.
template<typename... Args>
bool try_fire(event<Args...> &ev, Args&&... args);

/// Used to invoke all the event handlers of an event.
/// Will abort program execution using `HEAPFREE_ASSERT` if no listener
/// was called (because none are registered).
template<typename... Args>
void fire(event<Args...> &ev, Args&&... args);
```

### Members as event listeners

```c++
#include <iostream>
#include <utility>
#include "hardwave/heapfree/error.hpp"
#include "hardwave/heapfree/event.hpp"
#include "hardwave/heapfree/event/member_listener.hpp"

using namespace hardwave::heapfree;

event<int, int> my_event;

struct my_struct {
  // Adding this is mandatory
  HEAPFREE_DECLARE_ME(my_struct);
public:
  const char *id;
  int counter{0};
  my_struct(const char *i) : id{i} {}

  HEAPFREE_MEMBER_EVENT_LISTENER(my_event, my_listener)
  void my_listener(int a, int b) {
    std::cerr << "Method " << id << " listener called! #"
      << counter++ << "; " << a << " | " << b << "\n";
  }
};

int main() {
  my_struct s{"whitemice"};
  auto listener = on(my_event, [](int a, int b) {
    std::cerr << "this too! " << a << " " << b << "\n";
  });

  fire(my_event, 1, 2);

  // Can move my_struct; event listener will now be called on both!
  my_struct s2{std::move(s)};
  s.id = "grayrats";
  fire(my_event, 5, 6);
  HEAPFREE_ASSERT(s2.counter == 2, "");

  // Moving the event
  event<int, int> ev = std::move(my_event);
  fire(ev, 3, 4);

  return 0;
}
```

Output:

```
Method whitemice listener called! #0; 1 | 2
this too! 1 2
Method grayrats listener called! #1; 5 | 6
this too! 5 6
Method whitemice listener called! #1; 5 | 6
Method grayrats listener called! #2; 3 | 4
this too! 3 4
Method whitemice listener called! #2; 3 | 4
```

#### API Selection

See the header files themselves for full api documentation!

```c++
/// Macro that allows class members to be used as event listeners
#define HEAPFREE_MEMBER_EVENT_LISTENER(event, handler)

/// Similar to HEAPFREE_MEMBER_EVENT_LISTENER, but for classes which
/// contain both event listener and event itself.
#define HEAPFREE_RELATIVE_MEMBER_EVENT_LISTENER(event, handler)
```

### Iterator Range

```c++
#include <iostream>
#include <vector>
#include "hardwave/heapfree/iterator_range.hpp"

using namespace hardwave::heapfree;

int main() {
  std::vector<int> v{1,2,3,4,5,6,7,8,9,10};
  iterator_range r{v.begin() + 2, v.end() - 3};

  for (const auto &i : r)
    std::cerr << i << ", ";
  std::cerr << "\n";

  return 0;
}
```

Output:

```
3, 4, 5, 6, 7,
```

#### API Selection

See the header files themselves for full api documentation!

```c++
/// The iterator range can be used wrap any pair of iterators into a
/// range/container like interface.
template<typename Begin, typename End>
class iterator_range {
  typename value_type;
  typename size_type;
  typename difference_type;
  typename reference;
  typename pointer;
  typename iterator;

  iterator_range();
  iterator_range(const Begin &, const End &);
  template<typename Range>
  iterator_range(Range &r);

  size_type size() const;
  bool empty() const;

  Begin begin() const;
  End end() const;

  reference front() const { return *b; }
  reference back() const { return *std::prev(end()); }

  reference operator[](size_type idx);
  reference operator[](size_type idx) const;

  ...
};

...
```

### Metaprogramming

See the header files themselves for full api documentation!

```c++
/// Just an alias that makes declaring a function pointer
/// slightly more readable
template<typename R, typename... Args>
using function_ptr = R (*)(Args...);

/// Automatically defines me_t, me() and me() const
#define HEAPFREE_DECLARE_ME(T)

/// Defines me_t, me(), me() const, super_t, super() and super() const
#define HEAPFREE_DECLARE_ME_SUPER(Tme, Tsuper)

...
```

### Error Handling

See the header files themselves for full api documentation!

```c++
/// Hook that is called to abort program execution
// You will probably want to replace this with your own custom handler.
#define HEAPFREE_ABORT

/// Check if the given condition `b` is true.
#define HEAPFREE_ASSERT(b, ...);

...
```

## Copyright

Copyright © 2018-2019 by Karolin Varner.

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
