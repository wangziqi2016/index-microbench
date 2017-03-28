CC = gcc
CXX = g++ -std=gnu++0x
DEPSDIR := masstree/.deps
DEPCFLAGS = -MD -MF $(DEPSDIR)/$*.d -MP
MEMMGR = -ltcmalloc_minimal -lpapi
CFLAGS = -g -Ofast -Wno-invalid-offsetof -mcx16 -DNDEBUG -DBWTREE_NODEBUG $(DEPCFLAGS) -include masstree/config.h

SNAPPY = /usr/lib/libsnappy.so.1.3.0

all: workload workload_string

workload.o: workload.cpp microbench.h index.h 
	$(CXX) $(CFLAGS) -c -o workload.o workload.cpp

workload: workload.o bwtree.o skiplist.o
	$(CXX) $(CFLAGS) -o workload workload.o bwtree.o skiplist.o masstree/mtIndexAPI.a $(MEMMGR) -lpthread -lm

bwtree.o:
	$(CXX) $(CFLAGS) ./BwTree/bwtree.cpp -c -o bwtree.o $(MEMMGR) -lpthread -lm

skiplist.o:
	$(CXX) $(CFLAGS) ./skiplist/skiplist.cpp -c -o skiplist.o $(MEMMGR) -lpthread -lm

workload_string.o: workload_string.cpp microbench.h
	$(CXX) $(CFLAGS) -c -o workload_string.o workload_string.cpp

workload_string: workload_string.o bwtree.o skiplist.o
	$(CXX) $(CFLAGS) -o workload_string workload_string.o bwtree.o skiplist.o $(MEMMGR) -lpthread -lm

generate_workload:
	python gen_workload.py workload_config.inp

clean:
	$(RM) workload workload_string *.o *~ *.d
