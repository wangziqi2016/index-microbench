#include <vector>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <utility>
#include <time.h>
#include <sys/time.h>
#include <papi.h>

#include "allocatortracker.h"

#include "index.h"

#define INIT_LIMIT 50000000
#define LIMIT 10000000

//#define PAPI_IPC 1
#define PAPI_CACHE 1

//==============================================================
inline double get_now() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

