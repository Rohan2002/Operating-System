# Threadlib

## POSIX threads
POSIX threads, often referred to as pthreads, are a standardized threading API (Application Programming Interface) for creating and managing threads in a multi-threaded application on POSIX-compliant operating systems. POSIX stands for Portable Operating System Interface, and it is a family of standards that defines APIs for Unix-like operating systems, including Linux, macOS, and various flavors of Unix.

## API

```C
// Pthread types: thread and mutex type
pthread_t 
pthread_mutex_t
```
```C
// Pthread function calls
pthread_create 
pthread_exit 
pthread_join 
pthread_mutex_init 
pthread_mutex_lock 
pthread_mutex_unlock 
pthread_mutex_destroy
```

## Custom thread library

1. This library is implemented in the user-space.
2. This library uses a Round Robin Schedular.
3. This library implements all basic thread management functions.
4. This library implements mutexes.
5. Context switch time is defined as 10 ms and it can be changed in the ```thread-worker.h```
