
#include "rotate-skiplist.h"

using namespace rotate_skiplist;

// Three global variables defined within thread state object
// which mains the global linked list of thread objects
pthread_key_t ThreadState::thread_state_key{};
std::atomic<unsigned int> ThreadState::next_id{0U};
ThreadState *ThreadState::thread_state_head_p = nullptr;