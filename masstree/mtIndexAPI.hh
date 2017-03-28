#ifndef MTINDEXAPI_H
#define MTINDEXAPI_H

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <limits.h>
#if HAVE_NUMA_H
#include <numa.h>
#endif
#if HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#endif
#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#if __linux__
#include <asm-generic/mman.h>
#endif
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#ifdef __linux__
#include <malloc.h>
#endif
#include "nodeversion.hh"
#include "kvstats.hh"
#include "query_masstree.hh"
#include "masstree_tcursor.hh"
#include "masstree_insert.hh"
#include "masstree_remove.hh"
#include "masstree_scan.hh"
#include "timestamp.hh"
#include "json.hh"
#include "kvtest.hh"
#include "kvrandom.hh"
#include "kvrow.hh"
#include "kvio.hh"
#include "clp.h"
#include <algorithm>
#include <numeric>

#include <stdint.h>
#include "config.h"

#define GC_THRESHOLD 1000000
#define MERGE 0
#define MERGE_THRESHOLD 1000000
#define MERGE_RATIO 10
#define VALUE_LEN 8

#define LITTLEENDIAN 1
#define BITS_PER_KEY 8
#define K 2

#define SECONDARY_INDEX_TYPE 1

template <typename T>
class mt_index {
public:
  mt_index() {}
  ~mt_index() {
    table_->destroy(*ti_);
    delete table_;
    ti_->rcu_clean();
    ti_->deallocate_ti();
    free(ti_);
    if (cur_key_)
      free(cur_key_);
    
    return;
  }

  //#####################################################################################
  // Initialize
  //#####################################################################################
  unsigned long long rdtsc_timer() {
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((unsigned long long)hi << 32) | lo;
  }

  inline void setup() {
    ti_ = threadinfo::make(threadinfo::TI_MAIN, -1);
    table_ = new T;
    table_->initialize(*ti_);

    ic = 0;

    srand(rdtsc_timer());
  }

  void setup(int keysize, bool multivalue) {
    setup();

    cur_key_ = NULL;
    cur_keylen_ = 0;

    key_size_ = keysize;
    multivalue_ = multivalue;
  }

  void setup(int keysize, int keyLen, bool multivalue) {
    setup();

    cur_key_ = (char*)malloc(keyLen * 2);
    cur_keylen_ = 0;

    key_size_ = keysize;
    multivalue_ = multivalue;
  }

  //#####################################################################################
  // Garbage Collection
  //#####################################################################################
  inline void clean_rcu() {
    ti_->rcu_quiesce();
  }

  inline void gc_dynamic() {
    if (ti_->limbo >= GC_THRESHOLD) {
      clean_rcu();
      ti_->dealloc_rcu += ti_->limbo;
      ti_->limbo = 0;
    }
  }
  
  inline void reset() {
    if (multivalue_) {
      if (SECONDARY_INDEX_TYPE == 0) table_->destroy(*ti_);
      else if (SECONDARY_INDEX_TYPE == 1) table_->destroy_novalue(*ti_);
    }
    else {
      table_->destroy_novalue(*ti_);
    }
    
    delete table_;
    
    gc_dynamic();
    table_ = new T;
    table_->initialize(*ti_);
    ic = 0;
  }

  //#####################################################################################
  //Insert Unique
  //#####################################################################################
  inline bool put_uv(const Str &key, const Str &value) {
    typename T::cursor_type lp(table_->table(), key);
    bool found = lp.find_insert(*ti_);
    if (!found)
      ti_->advance_timestamp(lp.node_timestamp());
    else {
      lp.finish(1, *ti_);
      return false;
    }
    qtimes_.ts = ti_->update_timestamp();
    qtimes_.prev_ts = 0;
    lp.value() = row_type::create1(value, qtimes_.ts, *ti_);
    lp.finish(1, *ti_);
    ic++;

    return true;
  }
  
  bool put_uv(const char *key, int keylen, const char *value, int valuelen) {
    return put_uv(Str(key, keylen), Str(value, valuelen));
  }

  //#####################################################################################
  // Upsert
  //#####################################################################################
  inline void put(const Str &key, const Str &value) {
    typename T::cursor_type lp(table_->table(), key);
    bool found = lp.find_insert(*ti_);
    if (!found) {
      ti_->advance_timestamp(lp.node_timestamp());
      qtimes_.ts = ti_->update_timestamp();
      qtimes_.prev_ts = 0;
    }
    else {
      qtimes_.ts = ti_->update_timestamp(lp.value()->timestamp());
      qtimes_.prev_ts = lp.value()->timestamp();
      lp.value()->deallocate_rcu(*ti_);
    }
    
    lp.value() = row_type::create1(value, qtimes_.ts, *ti_);
    lp.finish(1, *ti_);
    ic += (value.len/VALUE_LEN);
  }

  void put(const char *key, int keylen, const char *value, int valuelen) {
    return put(Str(key, keylen), Str(value, valuelen));
  }

  //#################################################################################
  // Get (unique value)
  //#################################################################################
  inline bool dynamic_get(const Str &key, Str &value) {
    if (ic == 0) return false;
    typename T::unlocked_cursor_type lp(table_->table(), key);
    bool found = lp.find_unlocked(*ti_);
    if (found)
      value = lp.value()->col(0);
    return found;
  }
  
  inline bool get(const Str &key, Str &value) {
    return dynamic_get(key, value);
  }
  
  bool get (const char *key, int keylen, Str &value) {
    return get(Str(key, keylen), value);
  }

  //#################################################################################
  // Exist
  //#################################################################################
  inline bool exist(const Str &key) {
    bool found = false;
    if (ic != 0) {
    	typename T::unlocked_cursor_type lp(table_->table(), key);
    	found = lp.find_unlocked(*ti_);
    }
    
    return found;
  }
  
  bool exist(const char *key, int keylen) {
    return exist(Str(key, keylen));
  }

  //#################################################################################
  // Get Next (ordered)
  //#################################################################################
  bool dynamic_get_next(Str &value, char *cur_key, int *cur_keylen) {
    Json req = Json::array(0, 0, Str(cur_key, *cur_keylen), 2);
    q_[0].run_scan(table_->table(), req, *ti_);
    if (req.size() < 4)
      return false;
    value = req[3].as_s();
    if (req.size() < 6) {
      *cur_keylen = 0;
    }
    else {
      Str cur_key_str = req[4].as_s();
      memcpy(cur_key, cur_key_str.s, cur_key_str.len);
      *cur_keylen = cur_key_str.len;
    }
    
    return true;
  }

  //#################################################################################
  // Remove Unique
  //#################################################################################
  inline bool dynamic_remove(const Str &key) {
    if (ic == 0) return false;
    
    if (ti_->limbo >= GC_THRESHOLD) {
      clean_rcu();
      ti_->dealloc_rcu += ti_->limbo;
      ti_->limbo = 0;
    }
    bool remove_success = q_[0].run_remove(table_->table(), key, *ti_);
    if (remove_success)
      ic--;
    return remove_success;
  }

  inline bool remove(const Str &key) {
    bool remove_success = dynamic_remove(key);
    return remove_success;
  }

  bool remove(const char *key, int keylen) {
    return remove(Str(key, keylen));
  }

  //#################################################################################
  // Update
  //#################################################################################

  inline bool update_uv(const Str &key, const char *value) {
    Str get_value;
    if (dynamic_get(key, get_value)) {
      put(key, Str(value, VALUE_LEN));
      return true;
    }
    
    return false;
  }

  bool update_uv(const char *key, int keylen, const char *value) {
    return update_uv(Str(key, keylen), value);
  }

  //#################################################################################
  // Memory Stats
  //#################################################################################
  uint64_t memory_consumption () const {
    return (ti_->pool_alloc 
	    + ti_->alloc 
	    - ti_->pool_dealloc 
	    - ti_->dealloc 
	    - ti_->pool_dealloc_rcu 
	    - ti_->dealloc_rcu);
  }

  //#################################################################################
  // Accessors
  //#################################################################################
  int get_ic () {
    return ic;
  }

private:
  T *table_;
  int ic;
  threadinfo *ti_;
  query<row_type> q_[1];
  loginfo::query_times qtimes_;

  char* cur_key_;
  int cur_keylen_;

  bool multivalue_;
  int key_size_;
};

#endif //MTINDEXAPI_H
