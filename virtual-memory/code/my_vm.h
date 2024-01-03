#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
// Assume the address space is 32 bits, so the max memory size is 4GB
// Page size is 4KB

// Add any important includes here which you may need
#define ADDRESS_SPACE 32

#define PGSIZE 4096
#define PGSIZE_BITS 12

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL * 1024 * 1024 * 1024

// Size of "physcial memory"
#define MEMSIZE 1024 * 1024 * 1024

#define TLB_ENTRIES 512

// Structure to represents TLB
struct tlb
{
    /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
     * Think about the size of each TLB entry that performs virtual to physical
     * address translation.
     */
    struct
    {
        unsigned long tag;
        unsigned long physical_address;
        bool valid;
    } entries[TLB_ENTRIES];
};

typedef struct page_dir_entry
{
    unsigned int page_index_of_page_table : 32 - PGSIZE_BITS;
    unsigned int allocated_page : 1;
} page_dir_entry;
typedef struct page_table_entry
{
    unsigned int page_frame_index : 32 - PGSIZE_BITS;
    unsigned int allocated_page : 1;
} page_table_entry;

void set_physical_mem();
void *translate(void *va);
int page_map(void *va, void *pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *t_malloc(unsigned int num_bytes);
void t_free(void *va, int size);
int put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
void *get_next_avail(int num_pages);

#endif
