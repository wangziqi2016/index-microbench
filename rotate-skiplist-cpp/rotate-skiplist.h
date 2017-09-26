
#pragma once

#ifndef _H_ROTATE_SKIPLIST
#define _H_ROTATE_SKIPLIST

// This contains std::less and std::equal_to
#include <functional>
#include <atomic>

// Traditional C libraries 
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <pthread.h>

#include <unistd.h>

namespace rotate_skiplist {

// This prevents compiler rearranging the code cross this point
// Usually a hardware memury fence is not needed for x86-64
#define BARRIER() asm volatile("" ::: "memory")

// Note that since this macro may be defined also in other modules, we
// only define it if it is missing from the context
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

// This macro allocates a cahce line aligned chunk of memory
#define CACHE_ALIGNED_ALLOC(_s)                                 \
    ((void *)(((unsigned long)malloc((_s)+CACHE_LINE_SIZE*2) +  \
        CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE-1)))

// This macro defines an empty array of cache line size
// We use this to prevent false sharing
// We should pass in a different _n to specify different names for the struct
#define CACHE_PAD(_n) char __pad ## _n [CACHE_LINE_SIZE]

/////////////////////////////////////////////////////////////////////
// GC related classes
/////////////////////////////////////////////////////////////////////

/*
 * class GCChunk - This is a memory chunk as an allocation unit
 */
class GCChunk {
 public:
  // Number of blocks per chunk
  static constexpr int BLOCK_PER_CHUNK = 100;
  // Number of chunks we allocate from the heap in one function call
  static constexpr int CHUNK_PER_ALLOCATION = 1000;

 public:
  std::atomic<GCChunk *> next_p;
  
  // We use this to allocate blocks into the following array
  int next_block_index;
  void *blocks[BLOCK_PER_CHUNK];

  /*
   * AllocateFromHeap() - This function allocates an array of chunks
   *                      from the heap and initialize these chunks as 
   *                      a linked list
   *
   * We make sure that chunks are cache line aligned. Also, the returned 
   * chunk is a circular linked list, in which the last element is linked
   * to the first element in the array.
   */
  static GCChunk *AllocateFromHeap() {
    // Allocate that many chunks as an array
    GCChunk *chunk_p = static_cast<GCChunk *>( \
      CACHE_ALIGNED_ALLOC(sizeof(GCChunk) * CHUNK_PER_ALLOCATION));
    if(chunk_p == nullptr) {
      perror("GCCHunk::AllocateFromHeap() CACHE_ALIGNED_ALLOC");
      exit(1);
    }

    // Set next_p pointer as the next element
    for(int i = 0;i < CHUNK_PER_ALLOCATION;i++) {
      chunk_p[i].next_p = &chunk_p[i + 1];
    }

    // Make it circular
    chunk_p[CHUNK_PER_ALLOCATION - 1].next_p = &chunk_p[0];

    assert(GCChunk::DebugCountChunk(chunk_p) == CHUNK_PER_ALLOCATION);
    return chunk_p;
  }

  /*
   * LinkInto() - This function atomically links a circular linked
   *              list of GC chunks into another circular linked list
   *
   * Note that since the linked list we are linking is circular, we can treat
   * the pointer passed in as argument as a pointer to the actual tail,
   * and use the next node as a head. This requires:
   *    1. The linked list has at least 2 elements
   *    2. The list we are linking into has at least 1 element (since the
   *       free list is also a circular list we could not alter the first
   *       element)
   * which are all guaranteed
   */
  static void LinkInto(GCChunk *new_list_p, GCChunk *link_into_p) {
    // Checks condition 2
    assert(link_into_p != nullptr);
    // Checks condition 1
    assert(new_list_p->next_p != new_list_p);

    // This is a circular list, so no real head anyway
    GCChunk *head_p = new_list_p->next_p;
    GCChunk *tail_p = new_list_p;

    GCChunk *after_p = link_into_p->next_p.load();
    bool cas_ret;
    do {
      // Must reinitialize this every time
      tail_p->next_p = after_p;
      // after_p will be changed if CAS fails, so do not have to reload
      // it everytime but just adjust tail_p and retry
      cas_ret = link_into_p->next_p.compare_exchange_strong(after_p, head_p);
    } while(cas_ret == false);

    return;
  } 

  /*
   * DebugCountChunk() - This function counts the number of elements in a chunk
   *
   * Only called under debug mode
   */
  static int DebugCountChunk(const GCChunk * const chunk_p) {
    const GCChunk *p = chunk_p;
    int count = 0;
    do {
      count++;
      p = p->next_p;
    } while(p != chunk_p);

    return count;
  }
};

/////////////////////////////////////////////////////////////////////
// Thread local GC state
/////////////////////////////////////////////////////////////////////

/*
 * class GCThreadLocal - This is a thread local
 */
class GCThreadLocal {
 public:
  static constexpr int NUM_EPOCHS = 3;
  static constexpr int MAX_HOOKS = 4;
  static constexpr int NUM_SIZES = 1;

 public:
  unsigned int epoch;

  unsigned int entries_since_reclaim;

  /* used with gc_async_barrier() */
  void *async_page;
  int async_page_state;

  GCChunk *garbage_list[NUM_EPOCHS][NUM_SIZES];
  GCChunk *garbage_tail_list[NUM_EPOCHS][NUM_SIZES];
  GCChunk *chunk_cache;

  // Local allocation of chunks
  GCChunk *alloc_list[NUM_SIZES];
  // The 
  unsigned int alloc_chunk_list[NUM_SIZES];

  GCChunk *hook_list[NUM_EPOCHS][MAX_HOOKS];
};

/////////////////////////////////////////////////////////////////////
// Global GC state
/////////////////////////////////////////////////////////////////////

// This is used as a pointer by class GCState
class ThreadState;

/*
 * class GCState - This is the global GC state object which has a unique copy
 *                 over all threads (i.e. singleton)
 */
class GCState {
 public:
  using gc_hookfn = void (*)(ThreadState *, void *);

 public:
  CACHE_PAD(0);

  // The current epoch
  int current;

  CACHE_PAD(1);

  // Grants exclusive access to GC reclaim function
  std::atomic_flag gc_lock;

  CACHE_PAD(2);

  int system_page_size;
  unsigned long node_size_count;
  int block_size_list[GCThreadLocal::NUM_SIZES];
  unsigned long hook_count;
  gc_hookfn hook_fn_list[GCThreadLocal::MAX_HOOKS];

  // A circular linked list of free chunks
  GCChunk *free_list_p;

  GCChunk *alloc[GCThreadLocal::NUM_SIZES];
  unsigned long alloc_size[GCThreadLocal::NUM_SIZES];

 public:

  /*
   * GetFreeGCChunk() - This function returns free GC chunks from the current
   *                    free list
   *
   * If there are not enough free chunks we just batch allocate some
   * and link into the list before we retry. Note that since we treat free
   * list as a circular linked list, the free list pointer can be thought of
   * as pointing to the tail of the free list, and we are doing CAS on the
   * head of the linked list.
   */
  GCChunk *GetFreeGCChunk(int num) {
    // Need to loop for retry
    while(1) {
      // Whether we need to allocate new chunks
      bool need_new_chunk = false;
      GCChunk * const tail_p = free_list_p;
      // This is the first node of the linked list that we allocate
      GCChunk * head_p = tail_p->next_p;
      // Use this as iterator to find the last node we allocate
      // Note that we start at tail because head will be the
      // first element in the allocated linked list
      GCChunk *p = tail_p;
      for(int i = 0;i < num;i++) {
        // Note that we need to leave at least one element in the free list
        // so could not remove all chunks
        if(p->next_p == free_list_p) {
          need_new_chunk = true;
          break;
        }

        p = p->next_p;
      }

      if(need_new_chunk == true) {
        GCChunk *new_chunk_p = GCChunk::AllocateFromHeap();
        GCChunk::LinkInto(new_chunk_p, free_list_p);
        // Retry
        continue;
      }

      // Otherwise we know we have found a linked list of chunks
      // Note that here we will not have lost update problem when removing
      // elements from the linked list because we always remove from
      // the beginning
      bool cas_ret = tail_p->next_p.compare_exchange_strong(head_p, p->next_p);
      if(cas_ret == false) {
        continue;
      }

      // Make it circular and return
      p->next_p = head_p;
      return head_p;
    }

    assert(false);
    return nullptr;
  }

  /*
   * GetFilledGCChunk() - Returns a circular linked list of GC chunks and 
   *                      assign each block inside the chunk a valid piece
   *                      of memory of given size
   *
   * This function internally calls GetFreeGCChunk() to obtain chunks with 
   * uninitialized memory pointers, and then fills each pointer with a valid
   * memory address of size given in the argumemt, and return.
   */
  GCChunk *GetFilledGCChunk(int gc_chunk_count, int block_size) {
    GCChunk * const new_chunk_p = GetFreeGCChunk(gc_chunk_count);
    GCChunk *p = new_chunk_p;

    // Allocate this much memory in a sngle allocation and disperse
    // them as different blocks
    uint8_t *mem_p = static_cast<uint8_t *>( \
      CACHE_ALIGNED_ALLOC(gc_chunk_count * GCChunk::BLOCK_PER_CHUNK * block_size));

    // End condition is p->next_p == new_chunk_p but should check this
    // only after we have performed pointer initialization on the last chunk
    do {
      for(int i = 0;i < GCChunk::BLOCK_PER_CHUNK;i++) {
        p->blocks[i] = mem_p;
        mem_p += block_size;
      }

      p = p->next_p;
    } while(p != new_chunk_p);
    
    assert(GCChunk::DebugCountChunk(new_chunk_p) == gc_chunk_count);
    return new_chunk_p;
  }

  /*
   * GCState() - Initialize the object (rather than static states)
   */
  GCState() {
    system_page_size = static_cast<unsigned int>(sysconf(_SC_PAGESIZE));
    free_list_p = GCChunk::AllocateFromHeap();

    hook_count = 0;
    node_size_count = 0;

    return;
  }
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
  std::atomic_flag owned;

  // Points to the next state object in the linked list
  ThreadState *next_p;
  // Points to the gargabe collection state for the current thread
  GCState *gc_p;

  ///////////////////////////////////////////////////////////////////
  // Static data for maintaining the global linked list & thread ID
  ///////////////////////////////////////////////////////////////////

  // This is used as a key to access thread local data (i.e. pthread library
  // will return the registered thread local object when given the key)
  static pthread_key_t thread_state_key;
  // This is the ID of the next thread
  static std::atomic<unsigned int> next_id;
  // This is the head of the linked list
  static std::atomic<ThreadState *> thread_state_head_p;

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
    thread_state_p->owned.clear();
    
    return;
  }

  /*
   * Init() - Initialize the thread local environment
   */
  static void Init() {
    assert(inited == false);

    // Initialize the thread ID allocator
    next_id = 0U;
    // There is no element in the linked list
    thread_state_head_p = nullptr;

    // Must do a barrier to make sure all cores observe the same value
    BARRIER();

    // We use the clear owned flag as a call back which will be 
    // invoked when the thread exist
    if (pthread_key_create(&thread_state_key, ClearOwnedFlag)) {
      // Use this for system call failure as it translates errno
      // The format is: the string here + ": " + strerror(errno)
      perror("ThreadState::Init() pthread_key_create()");
      exit(1);
    }

    // Finally
    inited = true;
    return;
  }

  /*
   * EnterCritical() - Inform GC that some thread is still on the structure
   *
   * We return the thread state object to the caller to let is pass it in
   * as an argument when leaving
   */
  inline static ThreadState *EnterCritical() {
    // This is the current thread's local object
    ThreadState *thread_state_p = GetCurrentThreadState();
    // TODO: ENTER GC

    return thread_state_p;
  }

  /*
   * LeaveCritical() - Inform GC that some thread has no reference to the
   *                   data structure
   *
   * This function is just a simple wrapper over GC
   */
  inline static void LeaveCritical(ThreadState *thread_state_p) {
    (void)thread_state_p;
    // TODO: ADD GC LEAVE
    return;
  }

 private:
  /*
   * GetCurrentThreadState() - This function returns the current thread local
   *                           object for thread
   *
   * If the object is registered with pthread library then we fetch it using
   * a library call; Otherwise we try to recycle one from the linked list
   * by atomically CAS into a nonoccupied local object in the linked list.
   * Finally if none above succeeds, we allocate a cache aligned chunk of
   * memory and then add it into the list and register with pthread
   */
  static ThreadState *GetCurrentThreadState() {
    // 1. Try to obtain it as a registered per-thread object
    ThreadState *thread_state_p = static_cast<ThreadState *>(
      pthread_getspecific(thread_state_key));
    
    // If found then return - this should be the normal path
    if(thread_state_p != nullptr) {
      return thread_state_p;
    }

    // This is the head of the linked list
    thread_state_p = thread_state_head_p.load();
    while(thread_state_p != nullptr) {
      // Try to CAS a true value into the boolean
      // Note that the TAS will return false if successful so we take 
      // logical not
      bool ownership_acquired = !thread_state_p->owned.test_and_set();
      // If we succeeded and we successfully acquired an object, so just
      // register it and we are done
      if(ownership_acquired == true) {
        pthread_setspecific(thread_state_key, thread_state_p);
        return thread_state_p;
      }
    }

    // If we are here then the while loop exited without finding
    // an appropriate thread state object. So allocate one
    thread_state_p = \
      static_cast<ThreadState *>(CACHE_ALIGNED_ALLOC(sizeof(ThreadState)));
    if(thread_state_p == nullptr) {
      perror("ThreadState::GetCurrentThreadState() CACHE_ALIGNED_ALLOC()");
      exit(1);
    }

    // Initialize the value and set it
    thread_state_p->owned.clear();
    thread_state_p->owned.test_and_set();
    // This atomically increments the counter and returns the old value
    thread_state_p->id = next_id.fetch_add(1U);
    // TODO: ADD GC INIT HERE ALSO
    thread_state_p->gc_p = nullptr;

    // Whether the new node is installed using CAS into the linked list
    bool installed;
    // This will also be reloded with the current value if CAS fails
    ThreadState *old_head = thread_state_head_p.load();
    do {
      thread_state_p->next_p = old_head;
      installed = \
        thread_state_head_p.compare_exchange_strong(old_head, thread_state_p);
    } while(installed == false);

    // Also register this thread local
    pthread_setspecific(thread_state_key, thread_state_p);

    return thread_state_head_p;
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