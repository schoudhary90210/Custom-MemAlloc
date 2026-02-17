#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include "mm.h"

/* --- 64-BIT CONSTANTS & MACROS --- */
#define WSIZE             8       
#define DSIZE             16      
#define CHUNKSIZE         (1<<12) 
#define SEG_LISTS         20

#define MAX(x, y)         ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p)            (*(unsigned long *)(p))
#define PUT(p, val)       (*(unsigned long *)(p) = (unsigned long)(val))

#define GET_SIZE(p)       (GET(p) & ~0xF)
#define GET_ALLOC(p)      (GET(p) & 0x1)

#define HDRP(bp)          ((char *)(bp) - WSIZE)
#define FTRP(bp)          ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)     ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)     ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define GET_PTR(p)        (*(void **)(p))
#define SET_PTR(p, ptr)   (*(void **)(p) = (void *)(ptr))

#define NEXT_FREEP(bp)    ((char *)(bp))
#define PREV_FREEP(bp)    ((char *)(bp) + WSIZE)

/* --- GLOBAL VARIABLES --- */
static char *heap_listp = 0;  
static char *mem_heap = 0;    
static char *mem_brk = 0;     
static char *mem_max_addr = 0;
static void *seg_free_lists[SEG_LISTS];
static pthread_mutex_t heap_lock = PTHREAD_MUTEX_INITIALIZER;

/* --- HELPER PROTOTYPES --- */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_node(void *bp, size_t size);
static void remove_node(void *bp);
void *mem_sbrk(int incr);

/* --- OS SIMULATION --- */
void *mem_sbrk(int incr) {
    if (mem_heap == 0) {
        mem_heap = mmap(NULL, 50 * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (mem_heap == MAP_FAILED) return (void *)-1;
        mem_brk = mem_heap;
        mem_max_addr = mem_heap + (50 * 1024 * 1024);
    }
    char *old_brk = mem_brk;
    if ((incr < 0) || ((mem_brk + incr) > mem_max_addr)) return (void *)-1;
    mem_brk += incr;
    return (void *)old_brk;
}

/* --- API IMPLEMENTATION --- */

int mm_init(void) {
    pthread_mutex_lock(&heap_lock);
    for (int i = 0; i < SEG_LISTS; i++) seg_free_lists[i] = NULL;
    mem_heap = 0;

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        pthread_mutex_unlock(&heap_lock);
        return -1;
    }

    PUT(heap_listp, 0);                          
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     
    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        pthread_mutex_unlock(&heap_lock);
        return -1;
    }
    pthread_mutex_unlock(&heap_lock);
    return 0;
}

void *mm_malloc(size_t size) {
    size_t asize;      
    size_t extendsize; 
    char *bp = NULL;      

    if (size == 0) return NULL;

    pthread_mutex_lock(&heap_lock);

    if (size <= DSIZE) asize = 2 * DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
    } else {
        extendsize = MAX(asize, CHUNKSIZE);
        if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
            pthread_mutex_unlock(&heap_lock);
            return NULL;
        }
        place(bp, asize);
    }

    pthread_mutex_unlock(&heap_lock);
    return (void *)bp;
}

void mm_free(void *ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&heap_lock);
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
    pthread_mutex_unlock(&heap_lock);
}

void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0) { mm_free(ptr); return NULL; }
    
    void *newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    
    pthread_mutex_lock(&heap_lock);
    size_t old_block_size = GET_SIZE(HDRP(ptr));
    pthread_mutex_unlock(&heap_lock);

    size_t old_payload_size = (old_block_size >= DSIZE) ? (old_block_size - DSIZE) : 0;
    size_t copy_size = (size < old_payload_size) ? size : old_payload_size;
    memcpy(newptr, ptr, copy_size);
    mm_free(ptr);
    return newptr;
}

/* --- SEGREGATED LIST HELPERS --- */

static void insert_node(void *bp, size_t size) {
    int list = 0;
    while ((list < SEG_LISTS - 1) && (size > 1)) {
        size >>= 1;
        list++;
    }
    SET_PTR(NEXT_FREEP(bp), seg_free_lists[list]);
    SET_PTR(PREV_FREEP(bp), NULL);
    if (seg_free_lists[list] != NULL)
        SET_PTR(PREV_FREEP(seg_free_lists[list]), bp);
    seg_free_lists[list] = bp;
}

static void remove_node(void *bp) {
    int list = 0;
    size_t size = GET_SIZE(HDRP(bp));
    while ((list < SEG_LISTS - 1) && (size > 1)) {
        size >>= 1;
        list++;
    }
    if (GET_PTR(PREV_FREEP(bp)) != NULL)
        SET_PTR(NEXT_FREEP(GET_PTR(PREV_FREEP(bp))), GET_PTR(NEXT_FREEP(bp)));
    else
        seg_free_lists[list] = GET_PTR(NEXT_FREEP(bp));

    if (GET_PTR(NEXT_FREEP(bp)) != NULL)
        SET_PTR(PREV_FREEP(GET_PTR(NEXT_FREEP(bp))), GET_PTR(PREV_FREEP(bp)));
}

/* --- HEAP MANAGEMENT HELPERS --- */

static void *extend_heap(size_t words) {
    char *bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;
    PUT(HDRP(bp), PACK(size, 0));         
    PUT(FTRP(bp), PACK(size, 0));         
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 
    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
    } else if (prev_alloc && !next_alloc) {
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        remove_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        remove_node(PREV_BLKP(bp));
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_node(bp, size);
    return bp;
}

static void *find_fit(size_t asize) {
    void *bp;
    for (int list = 0; list < SEG_LISTS; list++) {
        bp = seg_free_lists[list];
        while (bp != NULL) {
            if (asize <= GET_SIZE(HDRP(bp))) return bp;
            bp = GET_PTR(NEXT_FREEP(bp));
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    remove_node(bp);
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_node(bp, csize - asize);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
