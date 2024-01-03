CC = gcc
CFLAGS = -g -c -m32 -lm
AR = ar -rc
RANLIB = ranlib

all: my_vm.a

my_vm.a: my_vm.o bitmap.o
	$(AR) libmy_vm.a my_vm.o bitmap.o
	$(RANLIB) libmy_vm.a

my_vm.o: my_vm.h
	$(CC) $(CFLAGS) my_vm.c

bitmap.o: bitmap.h
	$(CC) $(CFLAGS) bitmap.c

clean:
	rm -rf *.o *.a
