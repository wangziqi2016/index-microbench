
#pragma once

#ifndef _H_ROTATE_SKIPLIST
#define _H_ROTATE_SKIPLIST

// This contains std::less and std::equal_to
#include <functional>
#include <atomic>

// Traditional C libraries 
#include <cstdlib>
#include <cstdio>
#include <pthread.h>

namespace rotate_skiplist {

// This prevents compiler rearranging the code cross this point
// Usually a hardware memury fence is not needed for x86-64
#define BARRIER() asm volatile("" ::: "memory")

/*
 * class GCState
 */
class GCState {

};

/*
 * class ThreadState - This class implements thread state related functions
 */
class ThreadState {
 public:
  // This is the current thread ID
  unsigned int id;
  
  // Whether the thread state object is already owned by some
  // active thread. We set this flag when a thread claims a certain
  // object, and clears the flag when thread exits by using a callback
  std::atomic<bool> owned;

  // Points to the next state object in the linked list
  ThreadState *next_p;
  // Points to the gargabe collection state for the current thread
  GCState *gc_p;

  // This is used as a key to access thread local data
  // i.e. the thread state object allocated for every thread
  static pthread_key_t thread_state_key;
  static std::atomic<unsigned int> next_id;
  static ThreadState *thread_state_head_p;

  static void ClearOwnedFlag(ThreadState *thread_state_p) {
    // This does not have to be atomic
    thread_state_p.owned = false;
    
    return;
  }
};

/*
 * class RotateSkiplist - Main class of the skip list
 *
 * Note that for simplicity of coding, we contain all private classes used
 * by the main Skiplist class in the main class. We avoid a policy-based design
 * and do not templatize internal nodes in the skiplist as type parameters
 *
 * The skiplist is built based on partial ordering of keys, and therefore we 
 * need at least one functor which is the key comparator. To make key comparison
 * faster, a key equality checker is also required.
 */
template <typename _KeyType, 
          typename _ValueType,
          typename _KeyLess = std::less<_KeyType>,
          typename _KeyEq = std::equal_to<_KeyType>>
class RotateSkiplist {
 public:
  // Define member types to make it easier for external code to manipulate
  // types inside this class (if we just write it in the template argument
  // list then it is impossible for external code to obtain the actual
  // template arguments)
  using KeyType = _KeyType;
  using ValueType = _ValueType;
  using KeyLess = _KeyLess;
  using KeyEq = _KeyEq;
 
 private:
  
};

} // end of namespace rotate-skiplist

#endif