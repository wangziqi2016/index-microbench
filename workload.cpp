#include "microbench.h"

#include <cstring>
#include <cctype>

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
    init_file = "workloads/loada_zipf_int_100M.dat";
    txn_file = "workloads/txnsa_zipf_int_100M.dat";
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
      values.push_back(init_keys_data[count]);
      count++;
    }
  }

  count = 0;
  while ((count < LIMIT) && infile_txn.good()) {
    infile_txn >> op >> key;
    if (op.compare(insert) == 0) {
      ops.push_back(0);
      keys.push_back(key);
      ranges.push_back(1);
    }
    else if (op.compare(read) == 0) {
      ops.push_back(1);
      keys.push_back(key);
    }
    else if (op.compare(update) == 0) {
      ops.push_back(2);
      keys.push_back(key);
    }
    else if (op.compare(scan) == 0) {
      infile_txn >> range;
      ops.push_back(3);
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
  auto func = [idx, &init_keys, num_thread, &values](uint64_t thread_id, bool) {
    size_t total_num_key = init_keys.size();
    size_t key_per_thread = total_num_key / num_thread;
    size_t start_index = key_per_thread * thread_id;
    size_t end_index = start_index + key_per_thread;
   
    threadinfo *ti = threadinfo::make(threadinfo::TI_MAIN, -1);
 
    for(size_t i = start_index;i < end_index;i++) {
      idx->insert(init_keys[i], values[i], ti);
    }
    
    return;
  };
  
  StartThreads(idx, num_thread, func, false);
  
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
  int txn_num = 0;
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

  // Calculate the number of transactions here
  for(auto op : ops) {
    switch(op) {
      case 0: 
      case 1:
      case 3:
        txn_num++;
        break;
      case 2:
        if(index_type == TYPE_BWTREE) txn_num += 2;
        else txn_num++;
        break;
      default:
        fprintf(stderr, "Unknown operation\n");
        exit(1);
        break;
    }
  }

  fprintf(stderr, "# of Txn: %d\n", txn_num);
  
  auto func2 = [num_thread, 
                idx, 
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
      
      if (op == 0) { //INSERT
        idx->insert(keys[i] + 1, values[i], ti);
      }
      else if (op == 1) { //READ
        v.clear();
        idx->find(keys[i], &v, ti);
      }
      else if (op == 2) { //UPDATE
        idx->upsert(keys[i], values[i], ti);
      }
      else if (op == 3) { //SCAN
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

int main(int argc, char *argv[]) {

  if (argc != 5) {
    std::cout << "Usage:\n";
    std::cout << "1. workload type: a, c, e\n";
    std::cout << "2. key distribution: rand, mono\n";
    std::cout << "3. index type: bwtree masstree\n";
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
  else
    kt = 0;

  int index_type = 0;
  if (strcmp(argv[3], "bwtree") == 0)
    index_type = TYPE_BWTREE;
  else if (strcmp(argv[3], "masstree") == 0)
    index_type = TYPE_MASSTREE;
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

  std::vector<keytype> init_keys;
  std::vector<keytype> keys;
  std::vector<uint64_t> values;
  std::vector<int> ranges;
  std::vector<int> ops; //INSERT = 0, READ = 1, UPDATE = 2

  load(wl, kt, index_type, init_keys, keys, values, ranges, ops);
  printf("Finished loading workload file (mem = %lu)\n", MemUsage());
  exec(wl, index_type, num_thread, init_keys, keys, values, ranges, ops);
  printf("Finished running benchmark (mem = %lu)\n", MemUsage());

  return 0;
}
