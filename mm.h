/*
 * mm.h - Header file for the custom allocator
 */
#ifndef MM_H
#define MM_H

#include <stddef.h>

/* If you want to use this allocator for standard C library calls later,
 * you'd eventually rename these to malloc, free, realloc, etc.
 * For now, we use mm_* to avoid conflicts during testing.
 */
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

#endif
