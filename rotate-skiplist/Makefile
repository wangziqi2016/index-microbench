
CC=gcc
CFLAGS=-O3 -I./atomic_ops/
LDFLAGS=-pthread

.PHONY:	all clean

all:	main

$(BUILDIR)/ptst.o: ptst.h common.h 
	$(CC) $(CFLAGS) -c -o ./ptst.o ptst.c 

$(BUILDIR)/garbagecoll.o: garbagecoll.h ptst.h common.h
	$(CC) $(CFLAGS) -c -o ./garbagecoll.o garbagecoll.c 

$(BUILDIR)/nohotspot_ops.o: skiplist.h background.h nohotspot_ops.h common.h 
	$(CC) $(CFLAGS) -c -o ./nohotspot_ops.o nohotspot_ops.c 

$(BUILDIR)/skiplist.o: skiplist.h background.h common.h 
	$(CC) $(CFLAGS) -c -o ./skiplist.o skiplist.c 

$(BUILDIR)/background.o: background.h skiplist.h common.h
	$(CC) $(CFLAGS) -c -o ./background.o background.c 

intset.o: intset.h nohotspot_ops.h skiplist.h
	$(CC) $(CFLAGS) -c -o ./intset.o intset.c 

test.o: intset.h
	$(CC) $(CFLAGS) -c -o ./test.o test.c

main: intset.o background.o skiplist.o nohotspot_ops.o test.o ptst.o garbagecoll.o
	$(CC) $(CFLAGS) garbagecoll.o ptst.o skiplist.o nohotspot_ops.o intset.o background.o test.o -o ./main $(LDFLAGS)

clean:
	-rm -rf *.o
	-rm -f ./main
