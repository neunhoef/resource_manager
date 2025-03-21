#pragma once

#include <atomic>

// In this file we implement a high performance append-only bounded
// list. The list is bounded in that one can specify a limit on the used
// memory and older entries are automatically freed. Appending is fast
// in nearly all cases (two atomic operations). If an older chunk has
// to be freed, deallocation of the old list entries must be performed.
// With the exception of this relatively rare operation, the class is
// lock-free. This is used to keep the most recent API calls and to be
// able to deliver them via some API.

// The class AtomicList is a simple lock-free implementation of a singly
// linked list. One can only prepend new items and get a snapshot in form
// of a raw pointer to a Node. With this one can traverse the list but
// one must not free it.
template <typename T> class AtomicList {
public:
  // Note that Nodes use bare pointers, since AtomicList guards the allocation
  // of the whole list.
  struct Node {
    T _data;
    Node *_next;

    Node(const T &value) : _data(value), _next(nullptr) {}
    Node *next() { return _next; }
  };

private:
  std::atomic<Node *> _head;

public:
  AtomicList() : _head(nullptr) {}

  // It is *not* safe to destruct the AtomicList whilst other threads are
  // still prepending! The user of the class has to ensure that this is
  // done properly!
  ~AtomicList() {
    Node *n = _head.load();
    while (n != nullptr) {
      Node *next = n->next();
      delete n;
      n = next;
    }
    _head.store(nullptr);
  }

  void prepend(T &&value) noexcept {
    Node *new_node;
    try {
      new_node = new Node(std::move(value));
    } catch (...) {
      // We intentionally ignore out-of-memory errors and simply drop the
      // item to be noexcept.
      return;
    }
    Node *old_head;

    do {
      old_head = _head.load();
      new_node->_next = old_head;
      // The following compare_exchange_weak synchronizes with the
      // load in getSnapShot, therefore we use release semantics here.
      // In the case that the compare_exchange_weak fails the reload
      // uses acquire semantics to synchronize with another
      // compare_exchange_weak which might have released a pointer.
    } while (!_head.compare_exchange_weak(old_head, new_node,
                                          std::memory_order_release,
                                          std::memory_order_acquire));
  }

  // Returns a snapshot of the list at the moment of calling.
  // Result must not be freed externally! It is the AtomicList which
  // guards the allocation of all its Nodes!
  Node *getSnapshot() const noexcept {
    // This load synchronizes with the compare_exchange_weak in `prepend`
    // to ensure that any thread which gets the snapshot can actually
    // see modifications in the object the Node pointer is pointing to.
    return _head.load(std::memory_order_acquire);
  }
};
