# CS416 Project 3: User-level Memory Management

Rohan Deshpande (ryd4)
Jinyue (Eric) Liu (jl2661)
(Our submission time is extended because of medical emergency extension)
## Part 1. Implementation of Virtual Memory System

## Prelim data structures and notes.
### PGSIZE_BITS
Macro is the log base 2 of ```PGSIZE```. So when setting ```PGSIZE```, please make
sure to set the ```PGSIZE_BITS``` too. 

### page directory data structure.
A pointer to a ```page_dir_entry```. 

```c
struct page_dir_entry{
    unsigned int page_index_of_page_table : 32 - PGSIZE_BITS;
    unsigned int allocated_page: 1;
} page_dir_entry;
```
We use a bit field to represent ```page_dir_entry```. 
- ```page_index_of_page_table``` is the index in the physical bitmap.
The page table will be in its own page of the physical memory.
- ```allocated_page``` 1 to represent if the page of physical memory is allocated else 0.

### page table data structure.
A pointer to a ```page_table_entry```. 

```c
struct page_table_entry{
    unsigned int page_frame_index : 32 - PGSIZE_BITS;
    unsigned int allocated_page: 1;
} page_table_entry;
```
We use a bit field to represent ```page_table_entry```. 
- ```page_frame_index``` is the index in the physical bitmap. It represents a page from the physical memory for a, associated page in the virtual memory.
- ```allocated_page``` 1 to represent if the page of physical memory is allocated else 0.



## Functions

```void set_physical_mem()```
1. ```MEMSIZE``` bytes are allocated in heap to simulate physical memory.
2. Compute the number of number of virtual and physical pages.
    - The number of virtual pages. The number of virtual pages for 32-bit address space is 2^32 / ```PGSIZE```.
    - The number of virtual pages. The number of virtual pages for 32-bit address space is 2^(```MEMSIZE```) / ```PGSIZE```.
3. Compute the number of bits allocated for each component of the address.
    - Bits: offset within page.
    - Bits: page directory index.
    - Bits: page table index.
4. The page directory is points to the starting address of physical memory.
5. All entries inside page directory are set to their init state. 
6. We set the first page of the physical and virtual memory as allocated as a convention.
The first page is never cleared for physical and virtual memory.

```void *translate(void *va)```
1. From the virtual address, we extract the page directory index, page table index and offset.
2. Access the page directory entry by indexing page directory using the page directory index.
3. If page directory entry is not allocated then return NULL address.
4. Access page table entry by indexing page table using the page table index.
5. If page table entry is not allocated then return the NULL address.
6. Get the page frame index stored in the page table entry.
7. Compute the address of page frame in physical memory.
8. Add the offset to the address of the page frame and return the physical address for the given virtual address.

```int page_map(void *va, void *pa)```
1. From the virtual address, we extract the page directory index, page table index and offset.
2. Access the page directory entry by indexing page directory using the page directory index.
3. If page directory entry is not allocated then do the following procedures.
 - Allocate new page directory entry. 
 - The page directory entry will contain the index of the page in physical memory.
 - The page in physical memory will point to a new page table, which is a sequence of page table entries.
 - Each page table entry will be in its init state.
 - Compute the address of this new page table and store it in a variable.
4. Access the new page table from the computed address above.
5. Access a page table entry from page table using the page table index.
5. If page table entry is not allocated then do the following procedures.
 - Allocate a new page table entry. 
 - The page table entry will contain the page number of the given physical address (pa).

```void *get_next_avail(int num_pages)```
1. We implement the extra credit to handle internal fragmentation. (More details in the extra credit section).
2. In a nutshell this function will allocate ```num_pages``` free pages in virtual memory.
3. It will return the address of the starting page number in virtual memory.

```void *t_malloc(unsigned int num_bytes)```
1. Setup Virtual Memory data structures by calling ```set_physical_mem()```. Only for first time.
2. Compute the number of pages based on the given bytes to allocate.
3. For each pages, compute the physical page address that will be allocated. 
4. For each pages, compute the virtual page address using ```get_next_avail(pages)``` 
5. Call ```Pagemap(start_virtual_address, start_physical_address)``` to create a entry in the page
data structures.

```void t_free(void *va, int size)```
1. Use size parameter to compute the number of pages.
2. For each page compute the virual address and physical address.
3. De-allocate the pages by clearing the bits in the virtual and physical bitmaps.

```int put_value(void *va, void *src, int size)```
1. The goal is copy data from ```src``` to a physical page pointed by ```va```. The ```size``` of 
src can be more than a page size so we need to respect the page boundaries while copying the data.
2. Suppose ```va``` begins in the middle of the page, then only copy data from ```src``` that fits the remaning amount of page.
3. Then fill the whole pages of ```PGSIZE``` with data in ```src```.
4. If some bytes still remain after filling out pages, (the extra bytes), fill the next page to only
that amount of bytes.

```void get_valueput_value(void *va, void *src, int size)```
1. The goal is copy data from the physical page pointed by ```va``` to the ```src```. The same logic applies of copying data by batches from ```put_value```.
## Part 2. Implementation of a TLB

### add_TLB()
It first retrieves a tag out of the virtual address by taking the upper order bits, then maps the address to a specific index (Calculated via tag % entries), then sets the status to valid.

### check_TLB()
The function checks the existing TLB to see if an existing tag is in place, if so it will return the corresponding physical address. Otherwise it will increment the counter that kept track of TLB misses

### print_TLB_missrate()
Outputs the TLB miss rate by divindg two global counters: TLB misses/ TLB lookups

### invalidate_TLB()
I implemented this additional function to make sure a specific TLB entry is unavailable after it has been freed, this is used in t_free() function.
## Part 3. Extra Credit Parts

We handled internal fragmentation in the list of virtual pages represented by the virtual bitmap.

```get_next_avail``` function is responsible for this behaviour because it allocates N contiguous virtual pages and returns the address of the starting virtual page. 

General assumption of the algorithm

- A byte is a consecutive sequence of 8 bits. 
- Each bit represents a page.

We had two scenarios.
1. The number of pages is less than 15. (14 pages or less)
- We have a ```examine_bytes``` parameter set to 0. 
- The parameters means we will always examine two bytes next to each other or 16 bits.
- This is our search space to find the number of pages to fit.
Again each page is a bit.

2. The number of pages is 15 more or more.
- We created an expression for the ```examine_bytes``` parameter.
- ```examine_bytes = ((num_pages + 1) / 8) - 1;```
- ```examine_bytes``` are the number of free consecutive bytes to be found in the virtual bitmap.
- Now we have ```examine_bytes``` consecutive number of bytes, and 2 bytes to the left and right of the consecutive block of ```examine_bytes```.
- We use the bits in this bytes as a search space to find the number of pages to fit.
Again each page is a bit.

## Benchmark

Our program was able to pass 10 benchmarks under these conditions:
Benchmark #1:
Page Size: 4096
Thread: 15
Matrix Size: 5
Miss Rate: 0.024390

Benchmark #2:
Page Size: 4096
Thread: 15
Matrix Size: 6
Miss Rate: 0.015015

Benchmark #3:
Page Size: 4096
Thread: 15
Matrix Size: 20
Miss Rate: 0.000507 

The above 3 tests shows evidence that our TLB is working as intended, and makes sure that our program still performs under larger matrix numbers.

Benchmark #4:
Page Size: 8192 (12 bits)
Thread: 15
Matrix Size: 5
Miss Rate: 0.016393 

Benchmark #5:
Page Size: 16384 (13 bits)
Thread: 15
Matrix Size: 5
Miss Rate: 0.008264 

Benchmark #6:
Page Size: 131072 (17 bits)
Thread: 15
Matrix Size: 5
Miss Rate: 0.008264 

The above 3 tests shows evidence that our program can function on different page sizes (Multiple of 4K)

Benchmark #7
Page Size: 4096
Thread: 30
Matrix Size: 5
Miss Rate: 0.024725

Benchmark #8
Page Size: 4096
Thread: 30
Matrix Size: 5
Miss Rate: 0.024725

Benchmark #9
Page Size: 4096
Thread: 300
Matrix Size: 5
Miss Rate: 0.028623 

The above 3 tests shows evidence that our program can function on different thread counts

Benchmark #10
Page Size: 131072 (17 bits)
Thread: 300
Matrix Size: 20
Miss Rate: 0.000170 

For our very last test we decided that we should put all 3 extreme conditions together to perform a stress test, we allow 17 bits page sizes, 300 thread to calculate a 20x20 matrix, and program is still working fine. We are overall happy with the result of the benchmark and see positive sign about our program through them.

## Steps to compile
1. Run ```make``` to compile static binary inside the ```code``` directory.
2. Run ```make``` to compile ```test``` and ```mtest``` binaries in benchmark.