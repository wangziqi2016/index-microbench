
#include "indexkey.h"
#include "microbench.h"
#include "index.h"

#ifndef _UTIL_H
#define _UTIL_H

//This enum enumerates index types we support
enum {
  TYPE_BWTREE = 0,
  TYPE_MASSTREE,
  TYPE_ARTOLC,
  TYPE_BTREEOLC,
};

// These are workload operations
enum {
  OP_INSERT,
  OP_READ,
  OP_UPSERT,
  OP_SCAN,
};

//==============================================================
// GET INSTANCE
//==============================================================
template<typename KeyType, 
         typename KeyComparator=std::less<KeyType>, 
         typename KeyEuqal=std::equal_to<KeyType>, 
         typename KeyHash=std::hash<KeyType>>
Index<KeyType, KeyComparator> *getInstance(const int type, const uint64_t kt) {
  if (type == TYPE_BWTREE)
    return new BwTreeIndex<KeyType, KeyComparator, KeyEuqal, KeyHash>(kt);
  else if (type == TYPE_MASSTREE)
    return new MassTreeIndex<KeyType, KeyComparator>(kt);
  else if (type == TYPE_ARTOLC)
      return new ArtOLCIndex<KeyType, KeyComparator>(kt);
  else if (type == TYPE_BTREEOLC)
    return new BTreeOLCIndex<KeyType, KeyComparator>(kt);
  else {
    fprintf(stderr, "Unknown index type: %d\n", type);
    exit(1);
  }
  
  return nullptr;
}

inline double get_now() { 
struct timeval tv; 
  gettimeofday(&tv, 0); 
  return tv.tv_sec + tv.tv_usec / 1000000.0; 
} 

template <bool hyperthreading=false>
void PinToCore(size_t core_id) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(core_id * (hyperthreading ? 1 : 2), &cpu_set);

  int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
  if(ret != 0) {
    fprintf(stderr, "PinToCore() returns non-0\n");
    exit(1);
  }

  return;
}

template <typename Fn, typename... Args>
void StartThreads(Index<keytype, keycomp> *tree_p,
                  uint64_t num_threads,
                  Fn &&fn,
                  Args &&...args) {
  std::vector<std::thread> thread_group;

  if(tree_p != nullptr) {
    tree_p->UpdateThreadLocal(num_threads);
  }

  auto fn2 = [tree_p, &fn](uint64_t thread_id, Args ...args) {
    if(tree_p != nullptr) {
      tree_p->AssignGCID(thread_id);
    }

    PinToCore(thread_id);
    fn(thread_id, args...);

    if(tree_p != nullptr) {
      tree_p->UnregisterThread(thread_id);
    }

    return;
  };

  for (uint64_t thread_itr = 0; thread_itr < num_threads; ++thread_itr) {
    thread_group.push_back(std::thread{fn2, thread_itr, std::ref(args...)});
  }

  for (uint64_t thread_itr = 0; thread_itr < num_threads; ++thread_itr) {
    thread_group[thread_itr].join();
  }

  if(tree_p != nullptr) {
    tree_p->UpdateThreadLocal(1);
  }

  return;
}

/*
 * GetTxnCount() - Counts transactions and return 
 */
template <bool upsert_hack=true>
int GetTxnCount(const std::vector<int> &ops,
                int index_type) {
  int count = 0;
 
  for(auto op : ops) {
    switch(op) {
      case OP_INSERT:
      case OP_READ:
      case OP_SCAN:
        count++;
        break;
      case OP_UPSERT:
        count++;

        break;
      default:
        fprintf(stderr, "Unknown operation\n");
        exit(1);
        break;
    }
  }

  return count;
}


#endif
