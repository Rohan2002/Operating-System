CC         = gcc
SANITIZERS = -fsanitize=address,undefined
CFLAGS     = -g -Wall -Wvla -pthread 

SRC_DIR = code

all: bin/tw

bin/tw: bin/ obj/thread-worker.o obj/queue.o obj/thread-test.o
	$(CC) $(CFLAGS) obj/thread-worker.o obj/queue.o obj/thread-test.o -o $@

obj/thread-worker.o: obj/  $(SRC_DIR)/thread-worker.c  $(SRC_DIR)/thread-worker.h
	$(CC) $(CFLAGS) $(SRC_DIR)/thread-worker.c -c -o $@

obj/queue.o: obj/  $(SRC_DIR)/queue.c  $(SRC_DIR)/queue.h
	$(CC) $(CFLAGS) $(SRC_DIR)/queue.c -c -o $@

obj/thread-test.o: obj/  $(SRC_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/thread-test.c -c -o $@

obj/:
	mkdir -p $@

bin/:
	mkdir -p $@

clean:
	rm -r bin obj