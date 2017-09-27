
#include "rotate-skiplist.h"

// We do this for testing code
using namespace rotate_skiplist;

/*
 * ThreadStateTest1() - Tests threda state basic
 */
void ThreadStateTest1() {
  fprintf(stderr, "Testing class ThreadState basic\n");

  ThreadState::Init();
  ThreadState *thread_state_p = ThreadState::EnterCritical();

  assert(ThreadState::thread_state_head_p == thread_state_p);
  assert(ThreadState::next_id.load() == 1U);
  assert(thread_state_p->next_p == nullptr);
  assert(thread_state_p->owned.test_and_set() == true);
  // Has been registered
  assert(pthread_getspecific(ThreadState::thread_state_key) == thread_state_p);

  ThreadState::LeaveCritical(thread_state_p);

  return;
}

/*
 * GCChunkTest1() - Tests GC Chunk
 */
void GCChunkTest1() {
  fprintf(stderr, "Testing GCChunk basic\n");

  GCChunk *chunk_p = GCChunk::AllocateFromHeap();
  GCChunk *head_p = chunk_p;
  for(int i = 0;i < GCChunk::CHUNK_PER_ALLOCATION - 1;i++) {
    // Whether it is a linked list
    assert(chunk_p + 1 == chunk_p->next_p);
    chunk_p = chunk_p->next_p;
  }

  // Whether it is circular
  assert(head_p[GCChunk::CHUNK_PER_ALLOCATION - 1].next_p == head_p);

  return;
}

/*
 * GCCHunkTest2() - Tests GC chunk allocation
 */
void GCChunkTest2() {
  fprintf(stderr, "Testing GCChunk allocation\n");

  // This is a local object but we have constructor
  GCState gc_state{};
  fprintf(stderr, "  System page size = %d bytes\n", gc_state.system_page_size);
  fprintf(stderr, 
          "  Free list elements = %d\n", 
          GCChunk::DebugCountChunk(gc_state.free_list_p));

  // Allocate 17 chunks of 23 bytes block each
  GCChunk * const p = gc_state.GetFilledGCChunk(17, 23);
  
  assert(GCChunk::DebugCountChunk(p) == 17);

  GCChunk *p2 = p;
  int count = 0;
  do {
    count += 1;
    for(int i = 0;i < GCChunk::BLOCK_PER_CHUNK;i++) {
      // This will be reported by Valgrind if memory is not valid
      memset(p2->blocks[i], 0x88, 23);
    }

    // This must be true for a new chunk
    assert(p2->next_block_index == GCChunk::BLOCK_PER_CHUNK);

    p2 = p2->next_p;
  } while(p2 != p);

  assert(count == 17);

  return;
}

/*
 * main() - The main testing function
 */
int main() {
  RotateSkiplist<uint64_t, uint64_t> rsl{};
  (void)rsl;

  ThreadStateTest1();
  GCChunkTest1();
  GCChunkTest2();

  fprintf(stderr, "All tests have passed\n");
  
  return 0;
}