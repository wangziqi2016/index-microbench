
#include "microbench.h"

#include <cstring>
#include <cctype>

//#define USE_TBB

#ifdef USE_TBB
#include "tbb/tbb.h"
#endif

//#define BWTREE_CONSOLIDATE_AFTER_INSERT

#ifdef BWTREE_CONSOLIDATE_AFTER_INSERT
  #ifdef USE_TBB
  #warning "Could not use TBB and BwTree consolidate together"
  #endif
#endif

#ifdef BWTREE_COLLECT_STATISTICS
  #ifdef USE_TBB
  #warning "Could not use TBB and BwTree statistics together"
  #endif
#endif

// Whether insert interleaves
//#define INTERLEAVED_INSERT

// Whether read operatoin miss will be counted
#define COUNT_READ_MISS

typedef uint64_t keytype;
typedef std::less<uint64_t> keycomp;

static const uint64_t key_type=0;
static const uint64_t value_type=1; // 0 = random pointers, 1 = pointers to keys

#include "util.h"

/*
 * MemUsage() - Reads memory usage from /proc file system
 */
size_t MemUsage() {
  const char *reason = nullptr;
  size_t usage = 0UL;
  size_t file_size = 0UL;

  char *start_p, *end_p;
  int ret = 0;

  static constexpr size_t PROC_MEM_BUFFER_SIZE = 4096UL;
  char buffer[PROC_MEM_BUFFER_SIZE];

  FILE *fp = fopen("/proc/meminfo", "rb");
  if(fp == nullptr) {
    fprintf(stderr, "Could not open /proc/meminfo to read memory usage\n");
    exit(1);
  }

  int offset = 0;
  while((offset < PROC_MEM_BUFFER_SIZE - 1) && (feof(fp) == false)) {
    buffer[offset] = fgetc(fp);
    offset++;
  }

  buffer[offset] = '\0';

  static constexpr const char *pattern1 = "MemFree:";
  static constexpr const char *pattern2 = "kB";
  
  start_p = strstr(buffer, pattern1);
  if(start_p == nullptr) {
    fprintf(stderr, "Could not find \"%s\"\n", pattern1);
    exit(1);
  } else {
    start_p += strlen(pattern1);
  }

  end_p = strstr(start_p + strlen(pattern1), pattern2);
  if(end_p == nullptr) {
    fprintf(stderr, "Could not find \"%s\"\n", pattern2);
    exit(1);
  } else {
    end_p--;
  }

  while((start_p <= end_p) && (isdigit(*start_p) == false)) {
    start_p++;
  }

  while((end_p >= start_p) && (isdigit(*end_p) == false)) {
    end_p--;
  }

  while((start_p <= end_p)) {
    if(isdigit(*start_p) == false) {
      fprintf(stderr, "Unrecognized digit: %c\n", *start_p);
      exit(1);
    }

    size_t digit = static_cast<size_t>(*start_p - '0');
    usage = usage * 10 + digit;

    start_p++;
  }

  ret = fclose(fp);
  if(ret != 0) {
    reason = "fclose";
    goto syscall_error;
  }

  return usage;

syscall_error:
  fprintf(stderr, "System call failed (%s)\n", reason);
  exit(1);
}


//==============================================================
// LOAD
//==============================================================
inline void load(int wl, int kt, int index_type, std::vector<keytype> &init_keys, std::vector<keytype> &keys, std::vector<uint64_t> &values, std::vector<int> &ranges, std::vector<int> &ops) {
  std::string init_file;
  std::string txn_file;
  // 0 = a, 1 = c, 2 = e
  if (kt == 0 && wl == 0) {
    init_file = "workloads/loada_zipf_int_100M.dat";
    txn_file = "workloads/txnsa_zipf_int_100M.dat";
  }
  else if (kt == 0 && wl == 1) {
    init_file = "workloads/loadc_zipf_int_100M.dat";
    txn_file = "workloads/txnsc_zipf_int_100M.dat";
  }
  else if (kt == 0 && wl == 2) {
    init_file = "workloads/loade_zipf_int_100M.dat";
    txn_file = "workloads/txnse_zipf_int_100M.dat";
  }
  else if (kt == 1 && wl == 0) {
    init_file = "workloads/mono_inc_loada_zipf_int_100M.dat";
    txn_file = "workloads/mono_inc_txnsa_zipf_int_100M.dat";
  }
  else if (kt == 1 && wl == 1) {
    init_file = "workloads/mono_inc_loadc_zipf_int_100M.dat";
    txn_file = "workloads/mono_inc_txnsc_zipf_int_100M.dat";
  }
  else if (kt == 1 && wl == 2) {
    init_file = "workloads/mono_inc_loade_zipf_int_100M.dat";
    txn_file = "workloads/mono_inc_txnse_zipf_int_100M.dat";
  }
  else {
    fprintf(stderr, "Unknown workload type or key type\n");
    exit(1);
  }

  std::ifstream infile_load(init_file);
  std::ifstream infile_txn(txn_file);

  std::string op;
  keytype key;
  int range;

  std::string insert("INSERT");
  std::string read("READ");
  std::string update("UPDATE");
  std::string scan("SCAN");

  int count = 0;
  while ((count < INIT_LIMIT) && infile_load.good()) {
    infile_load >> op >> key;
    if (op.compare(insert) != 0) {
      std::cout << "READING LOAD FILE FAIL!\n";
      return;
    }
    init_keys.push_back(key);
    count++;
  }
  
  fprintf(stderr, "    Loaded %d keys\n", count);

  count = 0;
  uint64_t value = 0;
  void *base_ptr = malloc(8);
  uint64_t base = (uint64_t)(base_ptr);
  free(base_ptr);

  keytype *init_keys_data = init_keys.data();

  if (value_type == 0) {
    while (count < INIT_LIMIT) {
      value = base + rand();
      values.push_back(value);
      count++;
    }
  }
  else {
    while (count < INIT_LIMIT) {
      values.push_back(reinterpret_cast<uint64_t>(init_keys_data+count));
      count++;
    }
  }

  count = 0;
  while ((count < LIMIT) && infile_txn.good()) {
    infile_txn >> op >> key;
    if (op.compare(insert) == 0) {
      ops.push_back(OP_INSERT);
      keys.push_back(key);
      ranges.push_back(1);
    }
    else if (op.compare(read) == 0) {
      ops.push_back(OP_READ);
      keys.push_back(key);
    }
    else if (op.compare(update) == 0) {
      ops.push_back(OP_UPSERT);
      keys.push_back(key);
    }
    else if (op.compare(scan) == 0) {
      infile_txn >> range;
      ops.push_back(OP_SCAN);
      keys.push_back(key);
      ranges.push_back(range);
    }
    else {
      std::cout << "UNRECOGNIZED CMD!\n";
      return;
    }
    count++;
  }

}

//==============================================================
// EXEC
//==============================================================
inline void exec(int wl, 
                 int index_type, 
                 int num_thread,
                 std::vector<keytype> &init_keys, 
                 std::vector<keytype> &keys, 
                 std::vector<uint64_t> &values, 
                 std::vector<int> &ranges, 
                 std::vector<int> &ops) {

  Index<keytype, keycomp> *idx = getInstance<keytype, keycomp>(index_type, key_type);

  //WRITE ONLY TEST-----------------
  int count = (int)init_keys.size();
  double start_time = get_now();
  

#ifdef USE_TBB  
  tbb::task_scheduler_init init{num_thread};

  std::atomic<int> next_thread_id;
  next_thread_id.store(0);
  
  auto func = [idx, &init_keys, &values, &next_thread_id](const tbb::blocked_range<size_t>& r) {
    size_t start_index = r.begin();
    size_t end_index = r.end();
   
    threadinfo *ti = threadinfo::make(threadinfo::TI_MAIN, -1);

    int thread_id = next_thread_id.fetch_add(1);
    idx->AssignGCID(thread_id);
    
    int gc_counter = 0;
    for(size_t i = start_index;i < end_index;i++) {
      idx->insert(init_keys[i], values[i], ti);
      gc_counter++;
      if(gc_counter % 4096 == 0) {
        ti->rcu_quiesce();
      }
    }

    ti->rcu_quiesce();
    idx->UnregisterThread(thread_id);
    
    return;
  };

  idx->UpdateThreadLocal(num_thread);
  tbb::parallel_for(tbb::blocked_range<size_t>(0, count), func);
  idx->UpdateThreadLocal(1);
#else

#ifdef INTERLEAVED_INSERT
  fprintf(stderr, "    Interleaved insert\n");
#endif

  auto func = [idx, &init_keys, num_thread, &values](uint64_t thread_id, bool) {
    size_t total_num_key = init_keys.size();
    size_t key_per_thread = total_num_key / num_thread;
    size_t start_index = key_per_thread * thread_id;
    size_t end_index = start_index + key_per_thread;
   
    threadinfo *ti = threadinfo::make(threadinfo::TI_MAIN, -1);

    int gc_counter = 0;
#ifdef INTERLEAVED_INSERT
    for(size_t i = thread_id;i < total_num_key;i += num_thread) {
#else
    for(size_t i = start_index;i < end_index;i++) {
#endif
      idx->insert(init_keys[i], values[i], ti);
      gc_counter++;
      if(gc_counter % 4096 == 0) {
        ti->rcu_quiesce();
      }
    } 

    ti->rcu_quiesce();
    
    return;
  };
  
  
  StartThreads(idx, num_thread, func, false);

  // Only execute consolidation if BwTree delta chain is used
#ifdef BWTREE_CONSOLIDATE_AFTER_INSERT
  idx->AfterLoadCallback();
#endif

#endif   
  /*
  while (count < (int)init_keys.size()) {
    if (!idx->insert(init_keys[count], values[count])) {
      std::cout << "LOAD FAIL!\n";
      return;
    }
    count++;
  }
  */
  
  double end_time = get_now();
  double tput = count / (end_time - start_time) / 1000000; //Mops/sec

  std::cout << "insert " << tput << "\n";
  std::cout << "memory " << (idx->getMemory() / 1000000) << "\n\n";

  //idx->merge();
  std::cout << "static memory " << (idx->getMemory() / 1000000) << "\n\n";
  //return;

  //READ/UPDATE/SCAN TEST----------------
  start_time = get_now();
  int txn_num = GetTxnCount(ops, index_type);
  uint64_t sum = 0;
  uint64_t s = 0;

#ifdef PAPI_IPC
  //Variables for PAPI
  float real_time, proc_time, ipc;
  long long ins;
  int retval;

  if((retval = PAPI_ipc(&real_time, &proc_time, &ins, &ipc)) < PAPI_OK) {    
    printf("PAPI error: retval: %d\n", retval);
    exit(1);
  }
#endif

#ifdef PAPI_CACHE
  static const int EVENT_COUNT = 3;
  int events[EVENT_COUNT] = {PAPI_L1_TCM, PAPI_L2_TCM, PAPI_L3_TCM};
  long long counters[EVENT_COUNT];
  int retval;

  if ((retval = PAPI_start_counters(events, EVENT_COUNT)) != PAPI_OK) {
    fprintf(stderr, "PAPI failed to start counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }
#endif

  if(values.size() < keys.size()) {
    fprintf(stderr, "Values array too small\n");
    exit(1);
  }

  fprintf(stderr, "# of Txn: %d\n", txn_num);
  
  // This is used to count how many read misses we have found
  std::atomic<size_t> read_miss_counter{};
  read_miss_counter.store(0UL);

  auto func2 = [num_thread, 
                idx, 
                &read_miss_counter,
                &keys,
                &values,
                &ranges,
                &ops](uint64_t thread_id, bool) {
    size_t total_num_op = ops.size();
    size_t op_per_thread = total_num_op / num_thread;
    size_t start_index = op_per_thread * thread_id;
    size_t end_index = start_index + op_per_thread;
   
    std::vector<uint64_t> v;
    v.reserve(10);
 
    threadinfo *ti = threadinfo::make(threadinfo::TI_MAIN, -1);

    for(size_t i = start_index;i < end_index;i++) {
      int op = ops[i];
      
      if (op == OP_INSERT) { //INSERT
        idx->insert(keys[i], values[i], ti);
      }
      else if (op == OP_READ) { //READ
        v.clear();
        idx->find(keys[i], &v, ti);
        
        // If we count read misses then increment the 
        // counter here if the vetor is empty
#ifdef COUNT_READ_MISS
        if(v.size() == 0UL) {  
          read_miss_counter.fetch_add(1);
        }
#endif
      }
      else if (op == OP_UPSERT) { //UPDATE
        idx->upsert(keys[i], reinterpret_cast<uint64_t>(&keys[i]), ti);
      }
      else if (op == OP_SCAN) { //SCAN
        idx->scan(keys[i], ranges[i], ti);
      }
    }
    
    return;
  };
  
  StartThreads(idx, num_thread, func2, false);
  
  end_time = get_now();

#ifdef PAPI_IPC
  if((retval = PAPI_ipc(&real_time, &proc_time, &ins, &ipc)) < PAPI_OK) {    
    printf("PAPI error: retval: %d\n", retval);
    exit(1);
  }

  std::cout << "Time = " << real_time << "\n";
  std::cout << "Tput = " << LIMIT/real_time << "\n";
  std::cout << "Inst = " << ins << "\n";
  std::cout << "IPC = " << ipc << "\n";
#endif

#ifdef PAPI_CACHE
  if ((retval = PAPI_read_counters(counters, EVENT_COUNT)) != PAPI_OK) {
    fprintf(stderr, "PAPI failed to read counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  std::cout << "L1 miss = " << counters[0] << "\n";
  std::cout << "L2 miss = " << counters[1] << "\n";
  std::cout << "L3 miss = " << counters[2] << "\n";
#endif

  tput = txn_num / (end_time - start_time) / 1000000; //Mops/sec

  std::cout << "sum = " << sum << "\n";

  if (wl == 0) {  
    std::cout << "read/update " << (tput + (sum - sum)) << "\n";
  }
  else if (wl == 1) {
    std::cout << "read " << (tput + (sum - sum)) << "\n";
  }
  else if (wl == 2) {
    std::cout << "insert/scan " << (tput + (sum - sum)) << "\n";
  }
  else {
    std::cout << "read/update " << (tput + (sum - sum)) << "\n";
  }
}

/*
 * run_rdtsc_benchmark() - This function runs the RDTSC benchmark which is a high
 *                         contention insert-only benchmark
 *
 * Note that key num is the total key num
 */
void run_rdtsc_benchmark(int wl, int index_type, int thread_num, int key_num) {
  Index<keytype, keycomp> *idx = getInstance<keytype, keycomp>(index_type, key_type);

  auto func = [idx, thread_num, key_num](uint64_t thread_id, bool) {
    size_t key_per_thread = key_num / thread_num;

    threadinfo *ti = threadinfo::make(threadinfo::TI_MAIN, -1);

    int gc_counter = 0;
    for(size_t i = 0;i < key_per_thread;i++) {
      // Note that RDTSC may return duplicated keys from different cores
      // to counter this we combine RDTSC with thread IDs to make it unique
      // The counter value on a single core is always unique, though
      uint64_t key = Rdtsc() << 5 | thread_id;
      fprintf(stderr, "%lu\n", key);
      idx->insert(key, 0, ti);
      gc_counter++;
      if(gc_counter % 4096 == 0) {
        ti->rcu_quiesce();
      }
    }

    ti->rcu_quiesce();

    return;
  };

  double start_time = get_now();
  StartThreads(idx, thread_num, func, false);
  double end_time = get_now();

  // Only execute consolidation if BwTree delta chain is used
#ifdef BWTREE_CONSOLIDATE_AFTER_INSERT
  idx->AfterLoadCallback();
#endif
  
  double tput = key_num * 1.0 / (end_time - start_time) / 1000000; //Mops/sec
  std::cout << "insert " << tput << "\n";

  return;
}

int main(int argc, char *argv[]) {

  if (argc != 5) {
    std::cout << "Usage:\n";
    std::cout << "1. workload type: a, c, e\n";
    std::cout << "2. key distribution: rand, mono\n";
    std::cout << "3. index type: bwtree masstree artolc btreeolc\n";
    std::cout << "4. number of threads (integer)\n";
    
    return 1;
  }

  int wl = 0;
  // 0 = a
  // 1 = c
  // 2 = e
  if (strcmp(argv[1], "a") == 0)
    wl = 0;
  else if (strcmp(argv[1], "c") == 0)
    wl = 1;
  else if (strcmp(argv[1], "e") == 0)
    wl = 2;
  else
    wl = 0;

  fprintf(stderr, "Workload type: %d\n", wl);

  int kt = 0;
  // 0 = rand
  // 1 = mono
  if (strcmp(argv[2], "rand") == 0)
    kt = 0;
  else if (strcmp(argv[2], "mono") == 0)
    kt = 1;
  else if (strcmp(argv[2], "rdtsc") == 0)
    kt = 2;
  else
    kt = 0;

  int index_type = 0;
  if (strcmp(argv[3], "bwtree") == 0)
    index_type = TYPE_BWTREE;
  else if (strcmp(argv[3], "masstree") == 0)
    index_type = TYPE_MASSTREE;
  else if (strcmp(argv[3], "artolc") == 0)
    index_type = TYPE_ARTOLC;
  else if (strcmp(argv[3], "btreeolc") == 0)
    index_type = TYPE_BTREEOLC;
  else {
    fprintf(stderr, "Unknown index type: %d\n", index_type);
    exit(1);
  }
  
  // Then read number of threads using command line
  int num_thread = atoi(argv[4]);
  if(num_thread < 1 || num_thread > 40) {
    fprintf(stderr, "Do not support %d threads\n", num_thread);
    
    return 1; 
  } else {
    fprintf(stderr, "Number of threads: %d\n", num_thread);
  }

  fprintf(stderr, "index type = %d\n", index_type);

#ifdef COUNT_READ_MISS
  fprintf(stderr, "Counting read misses\n");
#endif

#ifdef BWTREE_CONSOLIDATE_AFTER_INSERT
  fprintf(stderr, "BwTree will considate after insert phase\n");
#endif

#ifdef USE_TBB
  fprintf(stderr, "Using Intel TBB to run concurrent tasks\n");
#endif

  // If the key type is RDTSC we just run the special function
  if(kt != 2) {
    std::vector<keytype> init_keys;
    std::vector<keytype> keys;
    std::vector<uint64_t> values;
    std::vector<int> ranges;
    std::vector<int> ops; //INSERT = 0, READ = 1, UPDATE = 2

    load(wl, kt, index_type, init_keys, keys, values, ranges, ops);
    printf("Finished loading workload file (mem = %lu)\n", MemUsage());
    exec(wl, index_type, num_thread, init_keys, keys, values, ranges, ops);
    printf("Finished running benchmark (mem = %lu)\n", MemUsage());
  } else {
    fprintf(stderr, "Running RDTSC benchmark...\n");
    run_rdtsc_benchmark(wl, index_type, num_thread, 50 * 1000 * 1000);
  }

  return 0;
}
