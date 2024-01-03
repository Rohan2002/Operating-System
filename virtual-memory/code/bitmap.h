#ifndef BITMAP_H_INCLUDED
#define BITMAP_H_INCLUDED
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct
{
    char *data;  // The bitmap (0-indexed)
    size_t size; // Number of bits to be stored in bitmap
} CharBitmap;

CharBitmap *init_bitmap(size_t num_bits);
void print_bitmap(CharBitmap *bitmap, int lines);
void free_bitmap(CharBitmap *bitmap);
int get_bit(CharBitmap *bp, size_t position);
void clear_bit(CharBitmap *bp, size_t position);
void set_bit(CharBitmap *bp, size_t position);
int get_free_bit(CharBitmap *bp);
#endif