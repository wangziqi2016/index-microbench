#include <iostream>
#include "indexkey.h"
#include "stx/btree_map.h"
#include "stx/btree.h"
#include "ART/hybridART.h"
#include "BwTree/bwtree.h"
#include <byteswap.h>

#include "skiplist/skiplist.h"
#include "masstree/mtIndexAPI.hh"

using namespace wangziqi2013;
using namespace bwtree;

template<typename KeyType, class KeyComparator>
class Index
{
 public:
  virtual bool insert(KeyType key, uint64_t value) = 0;

  virtual uint64_t find(KeyType key, std::vector<uint64_t> *v) = 0;

  virtual bool upsert(KeyType key, uint64_t value) = 0;

  virtual uint64_t scan(KeyType key, int range) = 0;

  virtual int64_t getMemory() const = 0;

  virtual void merge() = 0;
  
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

  bool insert(KeyType key, uint64_t value) {
    return index_p->Insert(key, value);
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v) {
    index_p->GetValue(key, *v);

    return 0UL;
  }

  bool upsert(KeyType key, uint64_t value) {
    //v.clear();
    //index_p->GetValue(key, v);
    //if(v.size() != 0) {
    index_p->Delete(key, value);
    //}

    index_p->Insert(key, value);

    return true;
  }

  uint64_t scan(KeyType key, int range) {
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

  void merge() {
    return;
  }

 private:
  BwTree<KeyType, uint64_t, KeyComparator, KeyEqualityChecker, KeyHashFunc> *index_p;

};

template<typename KeyType, 
         typename KeyComparator=std::less<KeyType>,
         typename KeyEqualityChecker=std::equal_to<KeyType>>
class SkipListIndex : public Index<KeyType, KeyComparator>
{
 public:
  SkipListIndex(uint64_t kt) {
    index_p = new SkipList<KeyType, uint64_t, KeyComparator, KeyEqualityChecker, std::equal_to<uint64_t>>{true};
    assert(index_p != nullptr);
    (void)kt;

    return;
  }
  
  void UpdateThreadLocal(size_t thread_num) { 
  }
  
  void AssignGCID(size_t thread_id) {
  }
  
  void UnregisterThread(size_t thread_id) {
  }

  bool insert(KeyType key, uint64_t value) {
    return index_p->Insert(key, value);
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v) {
    index_p->GetValue(key, *v);

    return 0UL;
  }

  bool upsert(KeyType key, uint64_t value) {
    //v.clear();
    //index_p->GetValue(key, v);
    //if(v.size() != 0) {
    index_p->Delete(key, value);
    //}

    index_p->Insert(key, value);

    return true;
  }

  uint64_t scan(KeyType key, int range) {
    return 0UL;
  }

  int64_t getMemory() const {
    return 0L;
  }

  void merge() {
    return;
  }

 private:
  SkipList<KeyType, uint64_t, KeyComparator, KeyEqualityChecker, std::equal_to<uint64_t>> *index_p;

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
    __bswap_64(i);
  }

  inline void swap_endian(const char *) {
    return;
  }
  
  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  bool insert(KeyType key, uint64_t value) {
    swap_endian(key);
    idx->put((const char*)&key, 8, (const char*)&value, 8);

    return true;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v=nullptr) {
    Str val;
    swap_endian(key);
    idx->get((const char*)&key, 8, val);

    v->clear();
    v->push_back(*(uint64_t *)val.s);

    return 0;
  }

  bool upsert(KeyType key, uint64_t value) {
    swap_endian(key);
    idx->put((const char*)&key, 8, (const char*)&value, 8);
    return true;
  }

  uint64_t scan(KeyType key, int range) {
    Str val;

    swap_endian(key);
    int key_len = 8;

    for (int i = 0; i < range; i++) {
      idx->dynamic_get_next(val, (char *)&key, &key_len);
    } 

    return 0UL;
  }

  int64_t getMemory() const {
    return 0;
  }

  void merge() {
    return;
  }

  MassTreeIndex(uint64_t kt) {
    idx = new MapType{};
    idx->setup(8, true);

    return;
  }

  MapType *idx;
};


template<typename KeyType, class KeyComparator>
class BtreeIndex : public Index<KeyType, KeyComparator>
{
 public:

  typedef AllocatorTracker<std::pair<const KeyType, uint64_t> > AllocatorType;
  typedef stx::btree_map<KeyType, uint64_t, KeyComparator, stx::btree_default_map_traits<KeyType, uint64_t>, AllocatorType> MapType;

  ~BtreeIndex() {
    delete idx;
    delete alloc;
  }
  
  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  bool insert(KeyType key, uint64_t value) {
    std::pair<typename MapType::iterator, bool> retval = idx->insert(key, value);
    return retval.second;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v=nullptr) {
    iter = idx->find(key);
    (void)v;
    if (iter == idx->end()) {
      std::cout << "READ FAIL\n";
      return 0;
    }
    return iter->second;
  }

  bool upsert(KeyType key, uint64_t value) {
    (*idx)[key] = value;
    return true;
  }

  uint64_t scan(KeyType key, int range) {
    iter = idx->lower_bound(key);

    if (iter == idx->end()) {
      std::cout << "SCAN FIRST READ FAIL\n";
      return 0;
    }
    
    uint64_t sum = 0;
    sum += iter->second;
    for (int i = 0; i < range; i++) {
      ++iter;
      if (iter == idx->end()) {
	break;
      }
      sum += iter->second;
    }
    return sum;
  }

  int64_t getMemory() const {
    return memory;
  }

  void merge() {
    return;
  }

  BtreeIndex(uint64_t kt) {
    memory = 0;
    alloc = new AllocatorType(&memory);
    idx = new MapType(KeyComparator(), (*alloc));
  }

  MapType *idx;
  int64_t memory;
  AllocatorType *alloc;
  typename MapType::const_iterator iter;
};


template<typename KeyType, class KeyComparator>
class ArtIndex : public Index<KeyType, KeyComparator>
{
 public:

  ~ArtIndex() {
    delete idx;
    delete key_bytes;
  }
  
  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  bool insert(KeyType key, uint64_t value) {
    //std::cout << "insert " << key << "\n";
    loadKey(key);
    idx->insert(key_bytes, value, key_length);
    return true;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v=nullptr) {
    loadKey(key);
    (void)v;
    return idx->lookup(key_bytes, key_length, key_length);
  }

  bool upsert(KeyType key, uint64_t value) {
    loadKey(key);
    //idx->insert(key_bytes, value, key_length);
    idx->upsert(key_bytes, value, key_length, key_length);
    return true;
  }

  uint64_t scan(KeyType key, int range) {
    loadKey(key);
    uint64_t sum = idx->lower_bound(key_bytes, key_length, key_length);
    for (int i = 0; i < range - 1; i++)
      sum += idx->next();
    return sum;
  }

  int64_t getMemory() const {
    return idx->getMemory();
  }

  void merge() {
    idx->tree_info();
    idx->merge();
  }

  ArtIndex(uint64_t kt) {
    key_type = kt;
    if (kt == 0) {
      key_length = 8;
      key_bytes = new uint8_t [8];
    }
    else {
      key_length = 8;
      key_bytes = new uint8_t [8];
    }

    idx = new hybridART(key_length);
  }

 private:

  inline void loadKey(KeyType key) {
    if (key_type == 0) {
      reinterpret_cast<uint64_t*>(key_bytes)[0]=__builtin_bswap64(key);
    }
  }

  inline void printKeyBytes() {
    for (int i = 0; i < key_length; i ++)
      std::cout << (uint64_t)key_bytes[i] << " ";
    std::cout << "\n";
  }

  hybridART *idx;
  uint64_t key_type; // 0 = uint64_t
  unsigned key_length;
  uint8_t* key_bytes;
};


template<typename KeyType, class KeyComparator>
class ArtIndex_Generic : public Index<KeyType, KeyComparator>
{
 public:

  ~ArtIndex_Generic() {
    delete idx;
    delete key_bytes;
  }
  
  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  bool insert(KeyType key, uint64_t value) {
    loadKey(key);
    idx->insert(key_bytes, value, key_length);
    return true;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v) {
    (void)v;
    loadKey(key);
    return idx->lookup(key_bytes, key_length, key_length);
  }

  bool upsert(KeyType key, uint64_t value) {
    loadKey(key);
    //idx->insert(key_bytes, value, key_length);
    idx->upsert(key_bytes, value, key_length, key_length);
    return true;
  }

  uint64_t scan(KeyType key, int range) {
    loadKey(key);
    uint64_t sum = idx->lower_bound(key_bytes, key_length, key_length);
    for (int i = 0; i < range - 1; i++)
      sum += idx->next();
    return sum;
  }

  int64_t getMemory() const {
    return idx->getMemory();
  }

  void merge() {
    idx->tree_info();
    idx->merge();
  }

  ArtIndex_Generic(uint64_t kt) {
    key_type = kt;
    if (kt == 0) {
      key_length = 31;
      key_bytes = new uint8_t [31];
    }
    else {
      key_length = 31;
      key_bytes = new uint8_t [31];
    }

    idx = new hybridART(key_length);
  }

 private:

  inline void loadKey(KeyType key) {
    if (key_type == 0) {
      key_bytes = (uint8_t*)key.data;
    }
  }

  inline void printKeyBytes() {
    for (int i = 0; i < key_length; i ++)
      std::cout << (uint64_t)key_bytes[i] << " ";
    std::cout << "\n";
  }

  hybridART *idx;
  uint64_t key_type; // 0 = GenericKey<31>
  unsigned key_length;
  uint8_t* key_bytes;
};

