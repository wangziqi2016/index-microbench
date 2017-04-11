CC = gcc
CXX = g++ -std=gnu++0x
DEPSDIR := masstree/.deps
DEPCFLAGS = -MD -MF $(DEPSDIR)/$*.d -MP
MEMMGR = -ltcmalloc_minimal -lpapi
CFLAGS = -g -Ofast -Wno-invalid-offsetof -mcx16 -DNDEBUG -DBWTREE_NODEBUG $(DEPCFLAGS) -include masstree/config.h

# By default just use 1 thread. Override this option to allow running the
# benchmark with 20 threads. i.e. THREAD_NUM=20 make run_all_atrloc
THREAD_NUM=1
TYPE=bwtree

SNAPPY = /usr/lib/libsnappy.so.1.3.0

all: workload workload_string

run_all: workload workload_string
	./workload a rand $(TYPE) $(THREAD_NUM) 
	./workload c rand $(TYPE) $(THREAD_NUM)
	./workload e rand $(TYPE) $(THREAD_NUM)
	./workload a mono $(TYPE) $(THREAD_NUM) 
	./workload c mono $(TYPE) $(THREAD_NUM)
	./workload e mono $(TYPE) $(THREAD_NUM)
	./workload_string a email $(TYPE) $(THREAD_NUM)
	./workload_string c email $(TYPE) $(THREAD_NUM)
	./workload_string e email $(TYPE) $(THREAD_NUM)

workload.o: workload.cpp microbench.h index.h 
	$(CXX) $(CFLAGS) -c -o workload.o workload.cpp

workload: workload.o bwtree.o ./masstree/mtIndexAPI.a
	$(CXX) $(CFLAGS) -o workload workload.o bwtree.o masstree/mtIndexAPI.a $(MEMMGR) -lpthread -lm

workload_string.o: workload_string.cpp microbench.h index.h
	$(CXX) $(CFLAGS) -c -o workload_string.o workload_string.cpp

workload_string: workload_string.o bwtree.o ./masstree/mtIndexAPI.a
	$(CXX) $(CFLAGS) -o workload_string workload_string.o bwtree.o masstree/mtIndexAPI.a $(MEMMGR) -lpthread -lm

bwtree.o: ./BwTree/bwtree.h ./BwTree/bwtree.cpp
	$(CXX) $(CFLAGS) -c -o bwtree.o ./BwTree/bwtree.cpp

generate_workload:
	python gen_workload.py workload_config.inp

clean:
	$(RM) workload workload_string *.o *~ *.d
