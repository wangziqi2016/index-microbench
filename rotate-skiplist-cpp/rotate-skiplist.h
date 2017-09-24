
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
 *
 * We allocate thread state for each thread as its thread local data area, and
 * chain them up as a linked list. When a thread is created, we try to search 
 * a nonoccupied thread state object and assign it to the thread, or create
 * a new one if none was found. When a thread exist we simply clear the owned
 * flag to release a thread state object
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

  // Static data for maintaining the global linked list & thread ID

  // This is used as a key to access thread local data (i.e. pthread library
  // will return the registered thread local object when given the key)
  static pthread_key_t thread_state_key;
  // This is the ID of the next thread
  static std::atomic<unsigned int> next_id;
  // This is the head of the linked list
  static ThreadState *thread_state_head_p;

  // This is used for debugging purpose to check whether the global states
  // are initialized
  static bool inited;

  /*
   * ClearOwnedFlag() - This is called when the threda exits and the OS
   *                    destroies the thread local structure
   *
   * The function prototype is defined by the pthread library
   */
  static void ClearOwnedFlag(void *data) {
    ThreadState *thread_state_p = static_cast<ThreadState *>(data);

    // This does not have to be atomic
    // Frees the thread state object for the next thread
    thread_state_p->owned = false;
    
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