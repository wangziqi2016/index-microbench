
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
 * main() - The main testing function
 */
int main() {
  RotateSkiplist<uint64_t, uint64_t> rsl{};
  (void)rsl;

  ThreadStateTest1();

  fprintf(stderr, "All tests have passed\n");
  
  return 0;
}