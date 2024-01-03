
#include "my_vm.h"
#include "bitmap.h"
#include <pthread.h>

void *physical_memory;

CharBitmap *virtual_bitmap;
CharBitmap *physical_bitmap;

page_dir_entry *page_directory;

static unsigned int offset_bits;
static unsigned int page_table_bits;
static unsigned int page_dir_bits;

static unsigned int tlb_misses;
static unsigned int tlb_lookups;

static pthread_mutex_t general_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t map_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

struct tlb tlb_store;

size_t top_bits(int num_top_bits, size_t va)
{
    size_t top_bit_index = (sizeof(size_t) * 8) - num_top_bits;
    return va >> top_bit_index;
}
size_t pad_virtual_address(void *va)
{
    // Pad a 64 bit virtual address to 32 bits.
    return (size_t)va;
}

/*
Function responsible for allocating and setting your physical memory
*/
void set_physical_mem()
{

    physical_memory = malloc(MEMSIZE);
    if (physical_memory == NULL)
    {
        fprintf(stderr, "%s at line %d: failed to allocate physical memory.", __func__, __LINE__);
        exit(1);
    }

    int virtual_bits = ADDRESS_SPACE - log2(PGSIZE);

    size_t num_virtual_pages = (1 << virtual_bits); // 2^32 / 2^12 = 2^20
    size_t num_physical_pages = (MEMSIZE) / PGSIZE;

    virtual_bitmap = init_bitmap(num_virtual_pages);
    physical_bitmap = init_bitmap(num_physical_pages);

    // calculate number of bits needed for each address.
    int num_entries_in_page_directory = PGSIZE / sizeof(page_dir_entry);
    offset_bits = (unsigned int)log2(PGSIZE);
    page_dir_bits = log2(num_entries_in_page_directory);
    page_table_bits = 32 - offset_bits - page_dir_bits;

    page_directory = physical_memory;
    for (int i = 0; i < num_entries_in_page_directory; ++i)
    {
        page_directory[i].allocated_page = false;
        page_directory[i].page_index_of_page_table = 0;
    }

    set_bit(physical_bitmap, 0);
    set_bit(virtual_bitmap, 0);
}

/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    unsigned long i;
    unsigned long tag;

    // takes the top order bits from the virtual address
    tag = (unsigned long)va >> offset_bits;
    i = tag % TLB_ENTRIES;
    tlb_store.entries[i].tag = tag;
    tlb_store.entries[i].physical_address = pa;
    tlb_store.entries[i].valid = true;
    // tlb_misses++;
    return 0;
}

/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
void *check_TLB(void *va)
{

    /* Part 2: TLB lookup code here */
    unsigned long i;
    unsigned long tag;

    tag = (unsigned long)va >> offset_bits;
    i = tag % TLB_ENTRIES;

    tlb_lookups++;
    if (tlb_store.entries[i].valid && tlb_store.entries[i].tag == tag)
    {
        return tlb_store.entries[i].physical_address;
    }
    tlb_misses++;
    return NULL;
}

/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate()
{
    double miss_rate = 0;

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    if (tlb_lookups != 0)
        miss_rate = (double)tlb_misses / tlb_lookups;
    else
        miss_rate = 0.0;
    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}

void invalidate_TLB(void *va)
{
    unsigned long tag;
    unsigned long i;

    tag = (unsigned long)va >> offset_bits;
    i = tag % TLB_ENTRIES;
    if (tlb_store.entries[i].valid && tlb_store.entries[i].tag == tag)
    {
        tlb_store.entries[i].valid = false;
    }
}

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
/* Part 1 HINT: Get the Page directory index (1st level) Then get the
 * 2nd-level-page table index using the virtual address.  Using the page
 * directory index and page table index get the physical address.
 *
 * Part 2 HINT: Check the TLB before performing the translation. If
 * translation exists, then you can return physical address from the TLB.
 */

// If translation not successful, then return NULL
void *translate(void *va)
{
    size_t virtual_address = pad_virtual_address(va);

    size_t page_dir_index = top_bits(page_dir_bits, virtual_address);
    size_t page_table_index = top_bits(page_table_bits, virtual_address << page_dir_bits);
    size_t offset_index = top_bits(offset_bits, virtual_address << (page_dir_bits + page_table_bits));

    void *cached_physical_address = check_TLB(va);
    if (cached_physical_address != NULL)
    {
        return cached_physical_address;
    }

    page_dir_entry pg_dir_entry = page_directory[page_dir_index];
    if (pg_dir_entry.allocated_page == false)
    {
        fprintf(stderr, "%s at line %d: failed to translate address\n", __func__, __LINE__);
        return NULL;
    }

    page_table_entry *page_table_address = (PGSIZE * pg_dir_entry.page_index_of_page_table) + physical_memory;
    page_table_entry pg_table_entry = page_table_address[page_table_index];

    if (pg_table_entry.allocated_page == false)
    {
        fprintf(stderr, "%s at line %d: failed to translate address\n", __func__, __LINE__);
        return NULL;
    }
    void *page_frame_address = (PGSIZE * pg_table_entry.page_frame_index) + physical_memory;
    void *physical_address = page_frame_address + offset_index;

    add_TLB(va, physical_address);

    return physical_address;
}

/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(void *va, void *pa)
{
    size_t virtual_address = pad_virtual_address(va);

    size_t page_dir_index = top_bits(page_dir_bits, virtual_address);
    size_t page_table_index = top_bits(page_table_bits, virtual_address << page_dir_bits);
    size_t offset_index = top_bits(offset_bits, virtual_address << (page_dir_bits + page_table_bits));

    page_dir_entry pg_dir_entry = page_directory[page_dir_index];
    page_table_entry *page_table_address;
    if (pg_dir_entry.allocated_page == false)
    {
        // allocating the page table.
        // page table takes one page of the physical memory.

        pg_dir_entry.allocated_page = true;
        int free_page_number = get_free_bit(physical_bitmap);
        pg_dir_entry.page_index_of_page_table = free_page_number;

        page_directory[page_dir_index] = pg_dir_entry;

        page_table_address = (PGSIZE * free_page_number) + physical_memory;
        int num_entries_in_page_table = PGSIZE / sizeof(page_table_entry);

        for (int i = 0; i < num_entries_in_page_table; ++i)
        {
            page_table_address[i].allocated_page = false;
            page_table_address[i].page_frame_index = 0;
        }
        // printf("physical_bitmap:free_page_number: %d\n", free_page_number);
        set_bit(physical_bitmap, free_page_number);
    }
    else
    {
        page_table_address = (PGSIZE * pg_dir_entry.page_index_of_page_table) + physical_memory;
    }
    page_table_entry pg_table_entry = page_table_address[page_table_index];
    if (pg_table_entry.allocated_page == false)
    {

        int physical_page_number = ((size_t)pa - (size_t)physical_memory) / PGSIZE;

        // fill the page table entry
        pg_table_entry.allocated_page = true;
        pg_table_entry.page_frame_index = physical_page_number;

        page_table_address[page_table_index] = pg_table_entry;

        set_bit(physical_bitmap, physical_page_number);
    }
    // add_TLB(va, pa);
    return 0;
}

/*Function that gets the next available page
 */
void *get_next_avail(int num_pages)
{
    size_t start_page_num = 0;
    size_t end_page_num = 0;

    int examine_bytes = num_pages < 15 ? 0 : ((num_pages + 1) / 8) - 1;
    // 15 >= free bits
    // num_pages = 15, examine_bytes = 1
    // virtual_bitmap->size = 16
    // 2
    size_t num_bytes = virtual_bitmap->size / 8;

    // bool found_consecutive_examine_bytes = fals
    // num_bytes = 4;
    // examine_bytes = 2;
    // 4 - 2 = 2
    for (size_t i = 1; i < num_bytes - examine_bytes; ++i)
    {
        bool continues_examine_byte = true;
        for (size_t j = 0; j < examine_bytes; j++)
        {
            // examine_bytes = 2
            // i = 1, j = 0  = 1
            // i = 1, j = 1 = 2
            if (virtual_bitmap->data[i + j] != 0)
            {
                continues_examine_byte = false;
                break;
            }
        }
        if (!continues_examine_byte)
        {
            continue;
        }
        char leftByte = virtual_bitmap->data[i - 1];
        char rightByte = virtual_bitmap->data[i + examine_bytes];

        start_page_num = (8 * i);
        end_page_num = 8 * (i + examine_bytes);
        // go from right to left
        // 0000 0000 | 0000 0000 0000 0000 | 0000 0000
        // 0000 0001
        for (int k = 0; k < 8; k++)
        {
            int mask = 1 << k;
            int bit = (rightByte & mask) != 0;
            if (bit)
            {
                break;
            }
            else
            {
                end_page_num++;
            }
        }
        // go from left to right
        for (int k = 7; k >= 0; k--)
        {
            int mask = 1 << k;
            int bit = (leftByte & mask) != 0;
            if (bit)
            {
                break;
            }
            else
            {
                start_page_num--;
            }
        }
        if (end_page_num - start_page_num >= num_pages)
        {
            break;
        }
    }
    if (end_page_num - start_page_num >= num_pages)
    {
        for (int j = start_page_num; j < (start_page_num + num_pages); j++)
        {
            set_bit(virtual_bitmap, j);
        }
        return start_page_num * PGSIZE;
    }
    else
    {
        return NULL;
    }
}

/* Function responsible for allocating pages
and used by the benchmark
*/
/*
 * HINT: If the physical memory is not yet initialized, then allocate and initialize.
 */
/*
 * HINT: If the page directory is not initialized, then initialize the
 * page directory. Next, using get_next_avail(), check if there are free pages. If
 * free pages are available, set the bitmaps and map a new page. Note, you will
 * have to mark which physical pages are used.
 */
void *t_malloc(unsigned int num_bytes)
{
    static int memory_init = 0;
    pthread_mutex_lock(&general_lock);
    if (!memory_init)
    {
        set_physical_mem();
        memory_init = 1;
    }

    size_t num_pages = (num_bytes / PGSIZE) + 1;
    void *start_virtual_address_page = get_next_avail(num_pages);
    if (!start_virtual_address_page)
    {
        pthread_mutex_unlock(&general_lock);
        return NULL;
    }

    for (size_t i = 0; i < num_pages; i++)
    {
        int free_bit_page_num = get_free_bit(physical_bitmap);

        void *physical_page_address = physical_memory + (free_bit_page_num * PGSIZE);
        void *virtual_page_address = start_virtual_address_page + (i * PGSIZE);

        set_bit(physical_bitmap, free_bit_page_num);

        int page_map_status = page_map(virtual_page_address, physical_page_address);
        if (page_map_status != 0)
        {
            fprintf(stderr, "%s at line %d: failed to map virtual address to physical address", __func__, __LINE__);
            pthread_mutex_unlock(&general_lock);
            return NULL;
        }
        // printf("Malloc virtual %p and physical %p\n", virtual_page_address, physical_page_address);
    }
    pthread_mutex_unlock(&general_lock);
    return start_virtual_address_page;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
 */
void t_free(void *va, int size)
{
    pthread_mutex_lock(&general_lock);
    size_t num_pages = (size / PGSIZE) + 1;
    size_t virtual_address = pad_virtual_address(va);

    size_t page_dir_index = top_bits(page_dir_bits, virtual_address);
    size_t page_table_index = top_bits(page_table_bits, virtual_address << page_dir_bits);

    size_t virtual_page_index = page_dir_index + page_table_index;

    page_dir_entry pg_dir_entry = page_directory[page_dir_index];

    page_table_entry *page_table_address = (PGSIZE * pg_dir_entry.page_index_of_page_table) + physical_memory;
    page_table_entry pg_table_entry = page_table_address[page_table_index];

    pg_table_entry.allocated_page = false;
    pg_table_entry.page_frame_index = 0;

    for (size_t i = 0; i < num_pages; i++)
    {
        size_t virtual_page_position = virtual_page_index + i;

        void *curr_virtual_address = va + (i * PGSIZE);
        void *curr_physical_address = translate(curr_virtual_address);
        if (curr_physical_address == NULL)
        {
            fprintf(stderr, "%s at line %d: physical address translation failed.", __func__, __LINE__);
            exit(1);
        }
        int physical_page_number = ((size_t)curr_physical_address - (size_t)physical_memory) / PGSIZE;
        invalidate_TLB(curr_virtual_address);
        clear_bit(virtual_bitmap, virtual_page_position);
        clear_bit(physical_bitmap, physical_page_number);
    }
    pthread_mutex_unlock(&general_lock);
}

/*
 * The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 */
int put_value(void *va, void *src, int size)
{
    pthread_mutex_lock(&general_lock);
    // remaining number of bytes to be copied
    size_t remaining_bytes = size;

    // initialize current virtual address, current source address, current physical address
    void *cur_va = va;
    void *cur_src = src;
    void *cur_pa = translate(cur_va);
    if (cur_pa == NULL)
    {
        fprintf(stderr, "%s at line %d: the current physical address is NULL\n", __func__, __LINE__);
        pthread_mutex_unlock(&general_lock);
        return -1;
    }
    // do first page
    // number of bytes to be copied to first page
    size_t bytes_to_copy = PGSIZE - ((size_t)va % PGSIZE);
    if (size < bytes_to_copy)
    {
        memcpy(cur_pa, cur_src, size);
        pthread_mutex_unlock(&general_lock);
        return 0;
    }
    memcpy(cur_pa, cur_src, bytes_to_copy);
    cur_va += bytes_to_copy;
    cur_src += bytes_to_copy;
    remaining_bytes -= bytes_to_copy;

    // copy all middle pages
    while (remaining_bytes >= PGSIZE)
    {
        cur_pa = translate(cur_va);
        if (cur_pa == NULL)
        {
            fprintf(stderr, "%s at line %d: the current physical address is NULL\n", __func__, __LINE__);
            pthread_mutex_unlock(&general_lock);
            return -1;
        }
        memcpy(cur_pa, cur_src, PGSIZE);
        cur_va += PGSIZE;
        cur_src += PGSIZE;
        remaining_bytes -= PGSIZE;
    }
    if (remaining_bytes > 0)
    {
        // do last page
        cur_pa = translate(cur_va);
        if (cur_pa == NULL)
        {
            fprintf(stderr, "%s at line %d: the current physical address is NULL\n", __func__, __LINE__);
            pthread_mutex_unlock(&general_lock);
            return -1;
        }
        memcpy(cur_pa, cur_src, remaining_bytes);
    }
    pthread_mutex_unlock(&general_lock);
    return 0;
}

/*
 * Given a virtual address, this function copies (size) bytes from the page
 * to val
 */
void get_value(void *va, void *src, int size)
{
    pthread_mutex_lock(&general_lock);
    // remaining number of bytes to be copied
    size_t remaining_bytes = size;

    // initialize current virtual address, current source address, current physical address
    void *cur_va = va;
    void *cur_src = src;
    void *cur_pa = translate(cur_va);
    if (cur_pa == NULL)
    {
        fprintf(stderr, "%s at line %d: the current physical address is NULL\n", __func__, __LINE__);
        pthread_mutex_unlock(&general_lock);
        return;
    }
    // do first page
    // number of bytes to be copied to first page
    size_t bytes_to_copy = PGSIZE - ((size_t)va % PGSIZE);
    if (size < bytes_to_copy)
    {
        memcpy(cur_src, cur_pa, size);
        pthread_mutex_unlock(&general_lock);
        return;
    }
    memcpy(cur_src, cur_pa, bytes_to_copy);
    cur_va += bytes_to_copy;
    cur_src += bytes_to_copy;
    remaining_bytes -= bytes_to_copy;

    // copy all middle pages
    while (remaining_bytes >= PGSIZE)
    {
        cur_pa = translate(cur_va);
        if (cur_pa == NULL)
        {
            fprintf(stderr, "%s at line %d: the current physical address is NULL\n", __func__, __LINE__);
            pthread_mutex_unlock(&general_lock);
            return;
        }
        memcpy(cur_src, cur_pa, PGSIZE);
        cur_va += PGSIZE;
        cur_src += PGSIZE;
        remaining_bytes -= PGSIZE;
    }
    if (remaining_bytes > 0)
    {
        // do last page
        cur_pa = translate(cur_va);
        if (cur_pa == NULL)
        {
            fprintf(stderr, "%s at line %d: the current physical address is NULL\n", __func__, __LINE__);
            pthread_mutex_unlock(&general_lock);
            return;
        }
        memcpy(cur_src, cur_pa, remaining_bytes);
    }
    pthread_mutex_unlock(&general_lock);
}

/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer)
{

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to
     * getting the values from two matrices, you will perform multiplication and
     * store the result to the "answer array"
     */

    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++)
    {
        for (j = 0; j < size; j++)
        {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++)
            {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value((void *)address_a, &a, sizeof(int));
                get_value((void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n",
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}
