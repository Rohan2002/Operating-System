#include "bitmap.h"

CharBitmap *init_bitmap(size_t num_bits)
{
    CharBitmap *bp = malloc(sizeof(CharBitmap));
    if (bp == NULL)
    {
        fprintf(stderr, "%s at line %d: failed to allocate bitmap struct.\n", __func__, __LINE__);
        exit(1);
    }

    // add the padding of 7 bits to ensure the correct # bytes is returned.
    size_t numCharBytes = (num_bits + 7) / 8;
    bp->data = (char *)calloc(numCharBytes, sizeof(char));
    if (bp->data == NULL)
    {
        fprintf(stderr, "%s at line %d: failed to allocate bitmap.\n", __func__, __LINE__);
        exit(1);
    }
    bp->size = num_bits;
    return bp;
}

void set_bit(CharBitmap *bp, size_t position)
{
    if (position >= bp->size)
    {
        fprintf(stderr, "%s at line %d: cannot index bitmap at %zu because size is %zu\n", __func__, __LINE__, position, bp->size);
        exit(1);
    }
    size_t byteIndex = position / 8;
    size_t bitIndex = position % 8;

    bp->data[byteIndex] |= (1 << bitIndex);
}

void clear_bit(CharBitmap *bp, size_t position)
{
    if (position >= bp->size)
    {
        fprintf(stderr, "%s at line %d: cannot index bitmap at %zu because size is %zu\n", __func__, __LINE__, position, bp->size);
        exit(1);
    }
    size_t byteIndex = position / 8;
    size_t bitIndex = position % 8;

    bp->data[byteIndex] &= ~(1 << bitIndex);
}

int get_bit(CharBitmap *bp, size_t position)
{
    if (position >= bp->size)
    {
        fprintf(stderr, "%s at line %d: cannot index bitmap at %zu because size is %zu\n", __func__, __LINE__, position, bp->size);
        exit(1);
    }
    size_t byteIndex = position / 8;
    size_t bitIndex = position % 8;

    return (bp->data[byteIndex] & (1 << bitIndex)) != 0;
}

int get_free_bit(CharBitmap *bp)
{
    for (size_t i = 0; i < bp->size; ++i)
    {
        size_t charIndex = i / 8;
        size_t bitIndex = i % 8;
        int bitValue = get_bit(bp, i);
        if (bitValue == 0)
        {
            return i;
        }
    }
    return -1;
}

void print_bitmap(CharBitmap *bitmap, int lines)
{
    for (size_t i = 0; i < lines * 32; ++i)
    {
        size_t charIndex = i / 8;
        size_t bitIndex = i % 8;
        int bitValue = get_bit(bitmap, (charIndex * sizeof(CharBitmap *)) + bitIndex);
        printf("%d", bitValue);

        if ((i + 1) % 8 == 0)
        {
            printf(" ");
        }
        if ((i + 1) % 32 == 0)
        {
            printf("\n");
        }
    }
    printf("\n");
}

void free_bitmap(CharBitmap *bitmap)
{
    free(bitmap->data);
    free(bitmap);
}

// int main()
// {
//     CharBitmap *virtualBitMap = init_bitmap(32);
//     printf("Size of virtualBitMap: %zu\n", virtualBitMap->size);
//     print_bitmap(virtualBitMap);

//     set_bit(virtualBitMap, 3);
//     printf("Bit value: %d\n", get_bit(virtualBitMap, 3));
//     print_bitmap(virtualBitMap);

//     clear_bit(virtualBitMap, 3);
//     printf("Bit value: %d\n", get_bit(virtualBitMap, 3));
//     print_bitmap(virtualBitMap);
//     return EXIT_SUCCESS;
// }