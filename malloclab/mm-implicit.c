/*
 * mm-implicit.c - The implicit free list malloc package.
 * 
 * Use the implicit free list approach. A block has a header and a footer.
 * Use the first fit method to find a free block. If the free block is not found, then extend the heap.
 * When freeing a block, coalesce its adjacent free blocks.
 * 
 * Perf index = 48 (util) + 19 (thru) = 67/100
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1<<12)   /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Global variables */
static void* heap_listp;

/*
 * coalesce - Merge the free block with any adjacent free blocks.
 *     Use the boundary-tags coalescing technique.
 */
static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (prev_alloc && next_alloc)           /* previous and next blocks are all allocated */
        return ptr;
    
    else if (prev_alloc && !next_alloc)     /* previous block is allocated but next block is unallocated */
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)     /* previous block is unallocated but next block is allocated */
    {
        size += GET_SIZE(FTRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }

    else                                    /* previous and next blocks are all unallocated */
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr))) + GET_SIZE(FTRP(PREV_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }

    return ptr;
}

/*
 * extend_heap - Extend the heap with a new free block.
 *     The extend_heap function is invoked in two different circumstances: 
 *     (1) when the heap is initialized and (2) when mm_malloc is unable to find a suitable fit.
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Calculate size to maintain alignment */
    size = ALIGN(words * WSIZE);
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));           /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * find_fit - Perform a first-fit search of the implicit free list.
 */
static void *find_fit(size_t asize)
{
    void *p = heap_listp;
    while (GET_SIZE(HDRP(p)) != 0)
    {
        if (GET_ALLOC(HDRP(p)) == 0 && GET_SIZE(HDRP(p)) >= asize)
            return p;
        p = NEXT_BLKP(p);
    }
    return NULL;
}

/*
 * place - Place the requested block at the beginning of the free block.
 *     Split only if the size of the remainder would equal or exceed the minimum block size.
 */
static void place(void *bp, size_t asize)
{
    char *header = HDRP(bp), *footer = FTRP(bp);
    size_t restsize = GET_SIZE(header) - asize;

    /* The size of remainder is less than the minimum block size. */
    if (restsize < 2 * DSIZE)
    {
        PUT(header, PACK(GET_SIZE(header), 1));
        PUT(footer, PACK(GET_SIZE(header), 1));
    }
    /* Split */
    else
    {
        PUT(header, PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(restsize, 0));
        PUT(footer, PACK(restsize, 0));
    }

}

/* 
 * mm_init - Initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;
    
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));        /* Prologue header */
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));        /* Epilogue header */
    heap_listp += 2 * WSIZE;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block from the free list.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    int newsize = ALIGN(size + SIZE_T_SIZE);
    char *bp;

    /* Search the free list for a fit */
    if ((bp = find_fit(newsize)) != NULL)
    {
        place(bp, newsize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    int extendsize = MAX(newsize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, newsize);
    return bp;    
}

/*
 * mm_free - Free the requested block and then merges adjacent free blocks.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Return a pointer to an allocated region of at least size bytes.
 *     If ptr is NULL, the call is equivalent to mm malloc(size). 
 *     If size is equal to zero, the call is equivalent to mm free(ptr). 
 *     If size is less than the original block size, remain the front part and free the rest.
 *     If size is larger than the original block size, check whether the next block is free and can hold the additional size.
 *     If can, increase the block. If not, find another free block.
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return mm_malloc(size);
    else if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    
    void *newptr = ptr;
    size_t oldsize = GET_SIZE(HDRP(ptr));
    int newsize = ALIGN(size + 2 * WSIZE);

    if (newsize == oldsize)
        return ptr;
    else if (newsize < oldsize)
    {
        char *header = HDRP(ptr), *footer = FTRP(ptr);
        PUT(header, PACK(newsize, 1));
        PUT(FTRP(ptr), PACK(newsize, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldsize - newsize, 0));
        PUT(footer, PACK(oldsize - newsize, 0));

        /* Coalesce if the next block was free */
        coalesce(NEXT_BLKP(ptr));
    }
    else
    {
        size_t addsize = newsize - oldsize;
        if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0 && GET_SIZE(HDRP(NEXT_BLKP(ptr))) >= addsize)
        {
            char *header = HDRP(ptr);
            char *nheader = HDRP(NEXT_BLKP(ptr)), *nfooter = FTRP(NEXT_BLKP(ptr));
            size_t nsize = GET_SIZE(nheader);
            PUT(header, PACK(newsize, 1));
            PUT(FTRP(ptr), PACK(newsize, 1));

            /* "nsize - addsize" should not be zero, 
               the header should not have a zero size except for the epilogue header */
            if (nsize > addsize)
            {
                PUT(HDRP(NEXT_BLKP(ptr)), PACK(nsize - addsize, 0));
                PUT(nfooter, PACK(nsize - addsize, 0));
            }
        }
        else
        {
            newptr = mm_malloc(size);
            memcpy(newptr, ptr, oldsize - 2 * WSIZE);
            mm_free(ptr);
        }
    }
    return newptr;
}

/*
 * mm_check - Check heap for consistency.
 */
void mm_check()
{
    return;
}
