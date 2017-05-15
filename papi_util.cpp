
#include "microbench.h"

static constexpr int PAPI_CACHE_EVENT_COUNT = 4;

/*
 * StartCacheMonitor() - Uses PAPI Library to start monitoring L1, L2, L3 cache misses  
 */
void StartCacheMonitor() {
  int events[PAPI_CACHE_EVENT_COUNT] = {PAPI_L1_TCM, PAPI_L2_TCM, PAPI_L3_TCM, PAPI_L1_TCA};
  long long counters[PAPI_CACHE_EVENT_COUNT];
  int retval;

  if ((retval = PAPI_start_counters(events, PAPI_CACHE_EVENT_COUNT)) != PAPI_OK) {
    fprintf(stderr, "PAPI failed to start counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  return;
}

/*
 * EndCacheMonitor() - Ends and prints PAPI result on cache misses 
 */
void EndCacheMonitor() {
  // We use this array to receive counter values
  long long counters[PAPI_CACHE_EVENT_COUNT];
  int retval;

  if ((retval = PAPI_read_counters(counters, PAPI_CACHE_EVENT_COUNT)) != PAPI_OK) {
    fprintf(stderr, "PAPI failed to read counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  std::cout << "L1 miss = " << counters[0] << "\n";
  std::cout << "L2 miss = " << counters[1] << "\n";
  std::cout << "L3 miss = " << counters[2] << "\n";
  std::cout << "Total cache ref = " << counters[3] << "\n";

  return;
}

// Empty function for consistency
void InitCacheMonitor() {
  return;
}
