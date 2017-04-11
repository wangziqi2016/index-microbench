#include <iostream>
#include "indexkey.h"
#include "BwTree/bwtree.h"
#include <byteswap.h>

#include "masstree/mtIndexAPI.hh"

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
    index_p->Delete(key, value);
    index_p->Insert(key, value);

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

  inline void swap_endian(const char *) {
    return;
  }
  
  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  bool insert(KeyType key, uint64_t value, threadinfo *ti) {
    swap_endian(key);
    idx->put((const char*)&key, 8, (const char*)&value, 8, ti);

    return true;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v, threadinfo *ti) {
    Str val;
    swap_endian(key);
    idx->get((const char*)&key, 8, val, ti);

    v->clear();
    v->push_back(*(uint64_t *)val.s);

    return 0;
  }

  bool upsert(KeyType key, uint64_t value, threadinfo *ti) {
    swap_endian(key);
    idx->put((const char*)&key, 8, (const char*)&value, 8, ti);
    return true;
  }

  uint64_t scan(KeyType key, int range, threadinfo *ti) {
    Str val;

    swap_endian(key);
    int key_len = 8;

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



