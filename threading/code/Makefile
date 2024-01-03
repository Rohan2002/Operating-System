CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib

all: clean thread-worker.a

thread-worker.a: thread-worker.o queue.o
	$(AR) libthread-worker.a thread-worker.o queue.o
	$(RANLIB) libthread-worker.a

thread-worker.o: thread-worker.h 
queue.o: queue.h

ifeq ($(SCHED), PSJF)
	$(CC) -pthread $(CFLAGS) -DPSJF thread-worker.c
	$(CC) -pthread $(CFLAGS) queue.c
else ifeq ($(SCHED), MLFQ)
	$(CC) -pthread $(CFLAGS) -DMLFQ thread-worker.c
	$(CC) -pthread $(CFLAGS) queue.c
else
	echo "no such scheduling algorithm"
endif

clean:
	rm -rf testfile *.o *.a
