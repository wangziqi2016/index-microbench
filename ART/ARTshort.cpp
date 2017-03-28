/*
  Adaptive Radix Tree (without path compression)
  Viktor Leis, 2012
  leis@in.tum.de
 */

#include <stdlib.h>    // malloc, free
#include <string.h>    // memset, memcpy
#include <stdint.h>    // integer types
#include <emmintrin.h> // x86 SSE intrinsics
#include <assert.h>
#include <stdio.h>
#include <sys/time.h>
#include <set>
#include <algorithm>

// Constants for the node types
static const int8_t NodeType4=0;
static const int8_t NodeType16=1;
static const int8_t NodeType48=2;
static const int8_t NodeType256=3;

// Shared header of all inner nodes
struct Node {
   uint16_t count;
   int8_t type;

   Node(int8_t type) : count(0),type(type) {}
};

// Node with up to 4 children
struct Node4 : Node {
   uint8_t key[4];
   Node* child[4];

   Node4() : Node(NodeType4) {}
};

// Node with up to 16 children
struct Node16 : Node {
   uint8_t key[16];
   Node* child[16];

   Node16() : Node(NodeType16) {}
};

const uint8_t emptyMarker=48;

// Node with up to 48 children
struct Node48 : Node {
   uint8_t childIndex[256];
   Node* child[48];

   Node48() : Node(NodeType48) {
      memset(childIndex,emptyMarker,sizeof(childIndex));
      memset(child,0,sizeof(child));
   }
};

// Node with up to 256 children
struct Node256 : Node {
   Node* child[256];

   Node256() : Node(NodeType256) {
      memset(child,0,sizeof(child));
   }
};

Node* makeLeaf(uintptr_t tid) {
   // Create a pseudo-leaf, that is stored directly in the pointer
   return reinterpret_cast<Node*>((tid<<1)|1);
}

uintptr_t getLeafValue(Node* node) {
   // The the value stored in the pseudo-leaf
   return reinterpret_cast<uintptr_t>(node)>>1;
}

bool isLeaf(Node* node) {
   // Is the node a leaf?
   return reinterpret_cast<uintptr_t>(node)&1;
}

uint8_t flipSign(uint8_t keyByte) {
   // Flip the sign bit, enables signed SSE comparison of unsigned values, used by Node16
   return keyByte^128;
}

uint32_t* leaves;

inline void loadKey(uintptr_t tid,uint8_t key[]) {
   // Store the key of the tuple into the key vector
   // Implementation is database specific
   reinterpret_cast<uint32_t*>(key)[0]=__builtin_bswap32(leaves[tid]);
}

Node* nullNode=NULL;

inline Node** findChildPtr(Node* n,uint8_t keyByte) {
   // Find the next child location for the keyByte
   switch (n->type) {
      case NodeType4: {
         Node4* node=static_cast<Node4*>(n);
         for (unsigned i=0;i<node->count;i++)
            if (node->key[i]==keyByte)
               return &node->child[i];
         return &nullNode;
      }
      case NodeType16: {
         Node16* node=static_cast<Node16*>(n);
         __m128i cmp=_mm_cmpeq_epi8(_mm_set1_epi8(flipSign(keyByte)),_mm_loadu_si128(reinterpret_cast<__m128i*>(node->key)));
         uint16_t bitfield=_mm_movemask_epi8(cmp)&(0xFFFF>>(16-node->count));
         if (bitfield)
            return &node->child[__builtin_ctz(bitfield)]; else
            return &nullNode;
      }
      case NodeType48: {
         Node48* node=static_cast<Node48*>(n);
         if (node->childIndex[keyByte]!=emptyMarker)
            return &node->child[node->childIndex[keyByte]]; else
            return &nullNode;
      }
      case NodeType256: {
         Node256* node=static_cast<Node256*>(n);
         return &(node->child[keyByte]);
      }
   }
   __builtin_unreachable();
}

bool leafMatches(Node* leaf,uint8_t key[],unsigned keyLength,unsigned depth,unsigned maxKeyLength) {
   // Check if the key of the leaf is equal to the searched key
   uint8_t leafKey[maxKeyLength];
   loadKey(getLeafValue(leaf),leafKey);
   for (unsigned i=depth;i<keyLength;i++)
      if (leafKey[i]!=key[i])
         return false;
   return true;
}

inline Node* lookup(Node* n,uint8_t* key,unsigned keyLength,unsigned maxKeyLength,unsigned depth=0) {
   // Lookup the key
   if (n==NULL)
      return NULL;

   while (true) {
      next:

      if (isLeaf(n)) {
         if (depth==keyLength||leafMatches(n,key,keyLength,depth,maxKeyLength))
            return n;
         return NULL;
      }

      uint8_t keyByte=key[depth++];
      switch (n->type) {
         case NodeType4: {
            Node4* node=static_cast<Node4*>(n);
            if (node->key[0]==keyByte) {
               n=node->child[0];
               goto next;
            }
            if (node->count==1)
               return NULL;
            if (node->key[1]==keyByte) {
               n=node->child[1];
               goto next;
            }
            if (node->count==2)
               return NULL;
            if (node->key[2]==keyByte) {
               n=node->child[2];
               goto next;
            }
            if (node->count==3)
               return NULL;
            if (node->key[3]==keyByte) {
               n=node->child[3];
               goto next;
            }
            return NULL;
         }
         case NodeType16: {
            Node16* node=static_cast<Node16*>(n);
            __m128i cmp=_mm_cmpeq_epi8(_mm_set1_epi8(flipSign(keyByte)),_mm_loadu_si128(reinterpret_cast<__m128i*>(node->key)));
            unsigned bitfield=_mm_movemask_epi8(cmp)& ((1<<node->count)-1);
            if (bitfield) {
               n=node->child[__builtin_ctz(bitfield)];
               continue;
            } else
               return NULL;
         }
         case NodeType48: {
            Node48* node=static_cast<Node48*>(n);
            if (node->childIndex[keyByte]!=emptyMarker) {
               n=node->child[node->childIndex[keyByte]];
               continue;
            } else
               return NULL;
         }
         case NodeType256: {
            Node256* node=static_cast<Node256*>(n);
            if (node->child[keyByte]) {
               n=node->child[keyByte];
               continue;
            } else
               return NULL;
         }
      }
      __builtin_unreachable();
   }
}

// Forward references
void insertNode4(Node4* node,Node** nodeRef,uint8_t keyByte,Node* child);
void insertNode16(Node16* node,Node** nodeRef,uint8_t keyByte,Node* child);
void insertNode48(Node48* node,Node** nodeRef,uint8_t keyByte,Node* child);
void insertNode256(Node256* node,Node** nodeRef,uint8_t keyByte,Node* child);

void insert(Node* node,Node** nodeRef,uint8_t key[],uintptr_t value,unsigned maxKeyLength,unsigned depth=0) {
   // Insert the leaf value into the tree

   if (node==NULL) {
      *nodeRef=makeLeaf(value);
      return;
   }

   if (isLeaf(node)) {
      // Replace leaf with Node4 and store both leafs in it
      uint8_t existingKey[maxKeyLength];
      loadKey(getLeafValue(node),existingKey);

      //assert(memcmp(key,existingKey,maxKeyLength)!=0);

      Node4* newNode=new Node4();
      *nodeRef=newNode;
      insertNode4(newNode,nodeRef,existingKey[depth],node);

      if (existingKey[depth]==key[depth])
         insert(node,&newNode->child[0],key,value,maxKeyLength,depth+1); else
         insertNode4(newNode,nodeRef,key[depth],makeLeaf(value));

      return;
   }

   // Recurse
   Node** child=findChildPtr(node,key[depth]);
   if (*child) {
      insert(*child,child,key,value,maxKeyLength,depth+1);
      return;
   }

   // Insert leaf into inner node
   Node* newNode=makeLeaf(value);
   switch (node->type) {
      case NodeType4: insertNode4(static_cast<Node4*>(node),nodeRef,key[depth],newNode); break;
      case NodeType16: insertNode16(static_cast<Node16*>(node),nodeRef,key[depth],newNode); break;
      case NodeType48: insertNode48(static_cast<Node48*>(node),nodeRef,key[depth],newNode); break;
      case NodeType256: insertNode256(static_cast<Node256*>(node),nodeRef,key[depth],newNode); break;
   }
}

void insertNode4(Node4* node,Node** nodeRef,uint8_t keyByte,Node* child) {
   // Insert leaf into inner node
   if (node->count<4) {
      // Insert element
      unsigned pos;
      for (pos=0;(pos<node->count)&&(node->key[pos]<keyByte);pos++);
      memmove(node->key+pos+1,node->key+pos,node->count-pos);
      memmove(node->child+pos+1,node->child+pos,(node->count-pos)*sizeof(uintptr_t));
      node->key[pos]=keyByte;
      node->child[pos]=child;
      node->count++;
   } else {
      // Grow to Node16
      Node16* newNode=new Node16();
      *nodeRef=newNode;
      newNode->count=4;
      for (unsigned i=0;i<4;i++)
         newNode->key[i]=flipSign(node->key[i]);
      memcpy(newNode->child,node->child,node->count*sizeof(uintptr_t));
      delete node;
      return insertNode16(newNode,nodeRef,keyByte,child);
   }
}

void insertNode16(Node16* node,Node** nodeRef,uint8_t keyByte,Node* child) {
   // Insert leaf into inner node
   if (node->count<16) {
      // Insert element
      uint8_t keyByteFlipped=flipSign(keyByte);
      __m128i cmp=_mm_cmplt_epi8(_mm_set1_epi8(keyByteFlipped),_mm_loadu_si128(reinterpret_cast<__m128i*>(node->key)));
      uint16_t bitfield=_mm_movemask_epi8(cmp)&(0xFFFF>>(16-node->count));
      unsigned pos=bitfield?__builtin_ctz(bitfield):node->count;
      memmove(node->key+pos+1,node->key+pos,node->count-pos);
      memmove(node->child+pos+1,node->child+pos,(node->count-pos)*sizeof(uintptr_t));
      node->key[pos]=keyByteFlipped;
      node->child[pos]=child;
      node->count++;
   } else {
      // Grow to Node48
      Node48* newNode=new Node48();
      *nodeRef=newNode;
      memcpy(newNode->child,node->child,node->count*sizeof(uintptr_t));
      for (unsigned i=0;i<node->count;i++)
         newNode->childIndex[flipSign(node->key[i])]=i;
      newNode->count=node->count;
      delete node;
      return insertNode48(newNode,nodeRef,keyByte,child);
   }
}

void insertNode48(Node48* node,Node** nodeRef,uint8_t keyByte,Node* child) {
   // Insert leaf into inner node
   if (node->count<48) {
      // Insert element
      unsigned pos=node->count;
      if (node->child[pos])
         for (pos=0;node->child[pos]!=NULL;pos++);
      node->child[pos]=child;
      node->childIndex[keyByte]=pos;
      node->count++;
   } else {
      // Grow to Node256
      Node256* newNode=new Node256();
      for (unsigned i=0;i<256;i++)
         if (node->childIndex[i]!=48)
            newNode->child[i]=node->child[node->childIndex[i]];
      newNode->count=node->count;
      *nodeRef=newNode;
      delete node;
      return insertNode256(newNode,nodeRef,keyByte,child);
   }
}

void insertNode256(Node256* node,Node** nodeRef,uint8_t keyByte,Node* child) {
   // Insert leaf into inner node
   node->count++;
   node->child[keyByte]=child;
}

static inline double gettime(void) {
  struct timeval now_tv;
  gettimeofday (&now_tv,NULL);
  return ((double)now_tv.tv_sec) + ((double)now_tv.tv_usec)/1000000.0;
}

int main(int argc,char** argv) {
   if (argc!=3) {
      printf("usage: %s n 0|1|2\nn: number of keys\n0: sorted keys\n1: dense keys\n2: sparse keys\n", argv[0]);
      exit(1);
   }

   unsigned n=atoi(argv[1]);
   uint32_t* keys=new uint32_t[n];
   for(unsigned i=0;i<n;i++)
      // sorted dense keys
      keys[i]=i;
   if (atoi(argv[2])==1)
      // shuffle dense keys
      std::random_shuffle(keys,keys+n);
   if (atoi(argv[2])==2) {
      // create sparse, unique keys
      unsigned count=0;
      std::set<uint32_t> keySet;
      while (count<n) {
         keys[count]=rand();
         if (!keySet.count(keys[count]))
            keySet.insert(keys[count++]);
      }
   }

   leaves=new uint32_t[n];
   memcpy(leaves,keys,sizeof(uint32_t)*n);

   // Build tree
   Node* tree=NULL;
   double start=gettime();
   for (unsigned i=0;i<n;i++) {
      uint8_t key[4];loadKey(i,key);
      insert(tree,&tree,key,i,4);
   }
   printf("%d,insert,%f\n",n,(n*1)/((gettime()-start))/1000000.0);

   // Repeat lookup for small trees to get more reproducable results
   unsigned repeat=100000000/n;
   if (repeat<1)
      repeat=1;
   start=gettime();
   for (unsigned r=0;r<repeat;r++) {
      for (unsigned i=0;i<n;i++) {
         uint8_t key[4];loadKey(i,key);
         Node* leaf=lookup(tree,key,4,4);
         assert(isLeaf(leaf) && getLeafValue(leaf)==i);
      }
   }
   printf("%d,search,%f\n",n,(n*repeat)/((gettime()-start))/1000000.0);

   return 0;
}
