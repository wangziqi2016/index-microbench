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

//#include "btreeIndex.h"
//#include "artIndex.h"
#include "index.h"

//#include <map>
//#include "stx/btree_map.h"
//#include "stx/btree.h"
//#include "stx-compact/btree_map.h"
//#include "stx-compact/btree.h"
//#include "stx-compress/btree_map.h"
//#include "stx-compress/btree.h"

//#define INIT_LIMIT 1001
//#define LIMIT 1000000
#define INIT_LIMIT 50000000
#define LIMIT 10000000

#define VALUES_PER_KEY 10

//#define PAPI_IPC 1
#define PAPI_CACHE 1


//==============================================================
inline double get_now() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

