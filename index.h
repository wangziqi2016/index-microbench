#include <iostream>
#include "indexkey.h"
#include "ARTOLC/Tree.h"
#include "BTreeOLC/BTreeOLC.h"
#include "BwTree/bwtree.h"
#include <byteswap.h>

#include "masstree/mtIndexAPI.hh"

#ifndef _INDEX_H
#define _INDEX_H

using namespace wangziqi2013;
using namespace bwtree;

template<typename KeyType, class KeyComparator>
class Index
{
 public:
  virtual bool insert(KeyType key, uint64_t value, threadinfo *ti) = 0;

  virtual uint64_t find(KeyType key, std::vector<uint64_t> *v, threadinfo *ti) = 0;

  virtual bool upsert(KeyType key, uint64_t value, threadinfo *ti) = 0;

  virtual uint64_t scan(KeyType key, int range, threadinfo *ti) = 0;

  virtual int64_t getMemory() const = 0;

  // This initializes the thread pool
  virtual void UpdateThreadLocal(size_t thread_num) = 0;
  virtual void AssignGCID(size_t thread_id) = 0;
  virtual void UnregisterThread(size_t thread_id) = 0;
  
  // After insert phase perform this action
  // By default it is empty
  // This will be called in the main thread
  virtual void AfterLoadCallback() {}
};

template<typename KeyType, class KeyComparator>
class ArtOLCIndex : public Index<KeyType, KeyComparator>
{
 public:

  ~ArtOLCIndex() {
    delete idx;
  }

  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  void setKey(Key& k, uint64_t key) { k.setInt(key); }
  void setKey(Key& k, GenericKey<31> key) { k.set(key.data,31); }

  bool insert(KeyType key, uint64_t value, threadinfo *ti) {
    auto t = idx->getThreadInfo();
    Key k; setKey(k, key);
    idx->insert(k, value, t);
    return true;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v, threadinfo *ti) {
    auto t = idx->getThreadInfo();
    Key k; setKey(k, key);
    uint64_t result=idx->lookup(k, t);
    v->clear();
    v->push_back(result);
    return 0;
  }

  bool upsert(KeyType key, uint64_t value, threadinfo *ti) {
    auto t = idx->getThreadInfo();
    Key k; setKey(k, key);
    idx->insert(k, value, t);
  }

  uint64_t scan(KeyType key, int range, threadinfo *ti) {
    auto t = idx->getThreadInfo();
    Key startKey; setKey(startKey, key);

    TID results[range];
    size_t resultCount;
    Key continueKey;
    idx->lookupRange(startKey, maxKey, continueKey, results, range, resultCount, t);

    return resultCount;
  }

  int64_t getMemory() const {
    return 0;
  }

  void merge() {
  }

  ArtOLCIndex(uint64_t kt) {
    if (sizeof(KeyType)==8) {
      idx = new ART_OLC::Tree([](TID tid, Key &key) { key.setInt(*reinterpret_cast<uint64_t*>(tid)); });
      maxKey.setInt(~0ull);
    } else {
      idx = new ART_OLC::Tree([](TID tid, Key &key) { key.set(reinterpret_cast<char*>(tid),31); });
      uint8_t m[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
		     0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
		     0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
		     0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
      maxKey.set((char*)m,31);
    }
  }

 private:
  Key maxKey;
  ART_OLC::Tree *idx;
};


template<typename KeyType, class KeyComparator>
class BTreeOLCIndex : public Index<KeyType, KeyComparator>
{
 public:

  ~BTreeOLCIndex() {
  }

  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  bool insert(KeyType key, uint64_t value, threadinfo *ti) {
    idx.insert(key, value);
    return true;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v, threadinfo *ti) {
    uint64_t result;
    idx.lookup(key,result);
    v->clear();
    v->push_back(result);
    return 0;
  }

  bool upsert(KeyType key, uint64_t value, threadinfo *ti) {
    idx.insert(key, value);
    return true;
  }

  void incKey(uint64_t& key) { key++; };
  void incKey(GenericKey<31>& key) { key.data[strlen(key.data)-1]++; };

  uint64_t scan(KeyType key, int range, threadinfo *ti) {
    uint64_t results[range];
    int count = idx.scan(key, range, results);
    // terrible hack:
    while (count < range) {
      incKey(key);
      count += idx.scan(key, range - count, results + count);
    }
    return count;
  }

  int64_t getMemory() const {
    return 0;
  }

  void merge() {}

  BTreeOLCIndex(uint64_t kt) {}

 private:

  btreeolc::BTree<KeyType,uint64_t> idx;
};

template<typename KeyType, 
         typename KeyComparator,
         typename KeyEqualityChecker=std::equal_to<KeyType>,
         typename KeyHashFunc=std::hash<KeyType>>
class BwTreeIndex : public Index<KeyType, KeyComparator>
{
 public:
  BwTreeIndex(uint64_t kt) {
    index_p = new BwTree<KeyType, uint64_t, KeyComparator, KeyEqualityChecker, KeyHashFunc>{};
    assert(index_p != nullptr);
    (void)kt;

    return;
  }
  
  void AfterLoadCallback() {
    int inner_depth_total = 0,
        leaf_depth_total = 0,
        inner_node_total = 0,
        leaf_node_total = 0;

    fprintf(stderr, "BwTree - Start consolidating delta chains...\n");
    int ret = index_p->DebugConsolidateAllRecursive(
      index_p->root_id.load(),
      &inner_depth_total,
      &leaf_depth_total,
      &inner_node_total,
      &leaf_node_total);
    fprintf(stderr, "BwTree - Finished consolidating %d delta chains\n", ret);
    fprintf(stderr,
            "    Inner Avg. Depth: %f (%d / %d)\n",
            (double)inner_depth_total / (double)inner_node_total,
            inner_depth_total,
            inner_node_total);
    fprintf(stderr,
            "    Leaf Avg. Depth: %f (%d / %d)\n",
            (double)leaf_depth_total / (double)leaf_node_total,
            leaf_depth_total,
            leaf_node_total);

    return;
  }
  
  void UpdateThreadLocal(size_t thread_num) { 
    index_p->UpdateThreadLocal(thread_num); 
  }
  
  void AssignGCID(size_t thread_id) {
    index_p->AssignGCID(thread_id); 
  }
  
  void UnregisterThread(size_t thread_id) {
    index_p->UnregisterThread(thread_id); 
  }

  bool insert(KeyType key, uint64_t value, threadinfo *) {
    return index_p->Insert(key, value);
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v, threadinfo *) {
    index_p->GetValue(key, *v);

    return 0UL;
  }

  bool upsert(KeyType key, uint64_t value, threadinfo *) {
    //index_p->Delete(key, value);
    //index_p->Insert(key, value);
    
    index_p->Upsert(key, value);

    return true;
  }

  uint64_t scan(KeyType key, int range, threadinfo *) {
    auto it = index_p->Begin(key);

    if(it.IsEnd() == true) {
      std::cout << "Iterator reaches the end\n";
      return 0UL;
    }

    uint64_t sum = 0;
    for(int i = 0;i < range;i++) {
      if(it.IsEnd() == true) {
        return sum;
      }

      sum += it->second;
      it++;
    }

    return sum;
  }

  int64_t getMemory() const {
    return 0L;
  }

 private:
  BwTree<KeyType, uint64_t, KeyComparator, KeyEqualityChecker, KeyHashFunc> *index_p;

};


template<typename KeyType, class KeyComparator>
class MassTreeIndex : public Index<KeyType, KeyComparator>
{
 public:

  typedef mt_index<Masstree::default_table> MapType;

  ~MassTreeIndex() {
    delete idx;
  }

  inline void swap_endian(uint64_t &i) {
    // Note that masstree internally treat input as big-endian
    // integer values, so we need to swap here
    // This should be just one instruction
    i = __bswap_64(i);
  }

  inline void swap_endian(GenericKey<31> &) {
    return;
  }
  
  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  bool insert(KeyType key, uint64_t value, threadinfo *ti) {
    swap_endian(key);
    idx->put((const char*)&key, sizeof(KeyType), (const char*)&value, 8, ti);

    return true;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v, threadinfo *ti) {
    Str val;
    swap_endian(key);
    idx->get((const char*)&key, sizeof(KeyType), val, ti);

    v->clear();
    v->push_back(*(uint64_t *)val.s);

    return 0;
  }

  bool upsert(KeyType key, uint64_t value, threadinfo *ti) {
    swap_endian(key);
    idx->put((const char*)&key, sizeof(KeyType), (const char*)&value, 8, ti);
    return true;
  }

  uint64_t scan(KeyType key, int range, threadinfo *ti) {
    Str val;

    swap_endian(key);
    int key_len = sizeof(KeyType);

    for (int i = 0; i < range; i++) {
      idx->dynamic_get_next(val, (char *)&key, &key_len, ti);
    } 

    return 0UL;
  }

  int64_t getMemory() const {
    return 0;
  }

  MassTreeIndex(uint64_t kt) {
    idx = new MapType{};

    threadinfo *main_ti = threadinfo::make(threadinfo::TI_MAIN, -1);
    idx->setup(main_ti);

    return;
  }

  MapType *idx;
};

#endif


