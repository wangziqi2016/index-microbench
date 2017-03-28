#include <string.h>

template <std::size_t keySize>
class GenericKey {
public:
  inline void setFromString(std::string key) {
    memset(data, 0, keySize);
    strcpy(data, key.c_str());
  }

  char data[keySize];
};

template <std::size_t keySize>
class GenericComparator {
public:
  GenericComparator() {}

  inline bool operator()(const GenericKey<keySize> &lhs, const GenericKey<keySize> &rhs) const {
    int diff = strcmp(lhs.data, rhs.data);
    return diff < 0;
  }
};

template <std::size_t keySize>
class GenericEqualityChecker {
public:
  GenericEqualityChecker() {}

  inline bool operator()(const GenericKey<keySize> &lhs, const GenericKey<keySize> &rhs) const {
    int diff = strcmp(lhs.data, rhs.data);
    return diff == 0;
  }
};

template <std::size_t keySize>
class GenericHasher {
public:
  GenericHasher() {}

  inline size_t operator()(const GenericKey<keySize> &lhs) const {
    (void)lhs;
    return 0UL;
  }
};

