
#include "microbench.h"

static constexpr int PAPI_CACHE_EVENT_COUNT = 3;
static constexpr int PAPI_INST_EVENT_COUNT = 4;

void InitInstMonitor() {
  return;
}

void StartInstMonitor() {
  int events[PAPI_INST_EVENT_COUNT] = \
    /*{PAPI_LD_INS, PAPI_SR_INS, */{PAPI_BR_INS, PAPI_TOT_CYC, PAPI_TOT_INS, PAPI_BR_MSP};
  long long counters[PAPI_INST_EVENT_COUNT];
  int retval;

  if ((retval = PAPI_start_counters(events, PAPI_INST_EVENT_COUNT)) != PAPI_OK) {
    fprintf(stderr, "PAPI failed to start counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  return;
}

void EndInstMonitor() {
  long long counters[PAPI_INST_EVENT_COUNT];
  int retval;

  if ((retval = PAPI_stop_counters(counters, PAPI_INST_EVENT_COUNT)) != PAPI_OK) {
    fprintf(stderr, "PAPI failed to read counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  //std::cout << "Total load = " << counters[0] << "\n";
  //std::cout << "Total store = " << counters[1] << "\n";
  std::cout << "Total branch = " << counters[0] << "\n";
  std::cout << "Total cycle = " << counters[1] << "\n";
  std::cout << "Total instruction = " << counters[2] << "\n";
  std::cout << "Total branch misprediction = " << counters[3] << "\n";
  
  return;
}

/*
 * StartCacheMonitor() - Uses PAPI Library to start monitoring L1, L2, L3 cache misses  
 */
void StartCacheMonitor() {
  int events[PAPI_CACHE_EVENT_COUNT] = \
    {PAPI_L1_TCM, PAPI_L2_TCM, PAPI_L3_TCM};//, PAPI_LD_INS, PAPI_SR_INS};
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

  if ((retval = PAPI_stop_counters(counters, PAPI_CACHE_EVENT_COUNT)) != PAPI_OK) {
    fprintf(stderr, "PAPI failed to read counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  std::cout << "L1 miss = " << counters[0] << "\n";
  std::cout << "L2 miss = " << counters[1] << "\n";
  std::cout << "L3 miss = " << counters[2] << "\n";

  return;
}

// Empty function for consistency
void InitCacheMonitor() {
  return;
}
