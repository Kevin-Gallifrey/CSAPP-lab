/*
 * mm-explicit.c - The explicit free list malloc package.
 * 
 * Use the explicit free list approach. A block has a header and a footer.
 * The free blocks contain a predecessor pointer and a successor pointer, which help to construct the free list.
 * A block should be at least 2 * DISE (the payload need to be large enough to hold the two pointers).
 * Use the first fit method to find a free block. If the free block is not found, then extend the heap.
 * When freeing a block, coalesce its adjacent free blocks.
 * 
 * Perf index = 43 (util) + 40 (thru) = 83/100
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

/* Given block ptr bp, compute address of its pred and succ (for free blocks only) */
#define PRED(bp) ((char *)(bp) + 0)
#define SUCC(bp) ((char *)(bp) + WSIZE)

/* Given block ptr bp, read and write its pred and succ field (for free blocks only) */
#define PUT_PRED(bp, val) (PUT(PRED(bp), (unsigned int)(val)))
#define PUT_SUCC(bp, val) (PUT(SUCC(bp), (unsigned int)(val)))
#define GET_PRED(bp) ((void *)(GET(PRED(bp))))
#define GET_SUCC(bp) ((void *)(GET(SUCC(bp))))

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Global variables */
static void* heap_listp;
static void* freelist;

static void print_freelist();

/*
 * add_to_freelist - Add the free block to the head of the freelist.
 */
static void add_to_freelist(void *bp)
{
    /* Avoid adding allocated block */
    if (GET_ALLOC(HDRP(bp)) != 0)
        return;
    
    PUT_PRED(bp, NULL);            /* Set the predecessor */
    PUT_SUCC(bp, freelist);        /* Set the successor */
    if (freelist != NULL)
        PUT_PRED(freelist, bp);    /* Set the predecessor of the first block in the freelist */
    freelist = bp;                 /* Set the freelist pointer */
}

/*
 * delete_from_freelist - Delete the block from the freelist.
 *     Need to change the freelist point if the first block is deleted.
 */
static void delete_from_freelist(void *bp)
{
    void *pred = GET_PRED(bp), *succ = GET_SUCC(bp);
    if (pred == NULL && succ == NULL)
        freelist = NULL;
    else if (succ == NULL)
        PUT_SUCC(pred, NULL);
    else if (pred == NULL)
    {
        PUT_PRED(succ, NULL);
        freelist = succ;
    }
    else
    {
        PUT_PRED(succ, pred);
        PUT_SUCC(pred, succ);
    }
}

/*
 * coalesce - Merge the free block with any adjacent free blocks and delete the merged blocks from the freelist.
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
        void *next = NEXT_BLKP(ptr);
        delete_from_freelist(next);
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)     /* previous block is unallocated but next block is allocated */
    {
        void *prev = PREV_BLKP(ptr);
        delete_from_freelist(prev);
        size += GET_SIZE(FTRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }

    else                                    /* previous and next blocks are all unallocated */
    {
        void *prev = PREV_BLKP(ptr), *next = NEXT_BLKP(ptr);
        delete_from_freelist(prev);
        delete_from_freelist(next);
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
 * find_fit - Perform a first-fit search of the explicit free list.
 */
static void *find_fit(size_t asize)
{
    void *p = freelist;
    while (p != NULL)
    {
        if (GET_ALLOC(HDRP(p)) == 0 && GET_SIZE(HDRP(p)) >= asize)
        {
            delete_from_freelist(p);
            return p;
        }
        p = GET_SUCC(p);
    }
    return NULL;
}

/*
 * place - Place the requested block at the beginning of the free block.
 *     Split only if the size of the remainder would equal or exceed the minimum block size.
 *     If splitted, add the remaining free block to the free list.
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
        add_to_freelist(NEXT_BLKP(bp));
    }

}

/* 
 * mm_init - Initialize the malloc package.
 *     Need to initialize the freelist.
 */
int mm_init(void)
{
    // print_freelist();

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;
    
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));        /* Prologue header */
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));        /* Epilogue header */
    heap_listp += 2 * WSIZE;

    void *bp;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if ((bp = extend_heap(CHUNKSIZE / WSIZE)) == NULL)
        return -1;

    /* Add the free block to the freelist */
    freelist = NULL;
    add_to_freelist(bp);
    // print_freelist();

    return 0;
}

/* 
 * mm_malloc - Allocate a block from the free list.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // print_freelist();
    
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    size_t newsize = ALIGN(size + SIZE_T_SIZE);
    void *bp;

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
 *     After freeing, add the free block to the freelist.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    add_to_freelist(coalesce(ptr));
}

/*
 * mm_realloc - Return a pointer to an allocated region of at least size bytes.
 *     If ptr is NULL, the call is equivalent to mm malloc(size). 
 *     If size is equal to zero, the call is equivalent to mm free(ptr). 
 *     If size is less than the original block size, remain the front part and free the rest.
 *     If size is larger than the original block size, check whether the next block is free and can hold the additional size.
 *     If can, increase the block. If not, find another free block.
 *     Notice that the free block should be at least 2 * DSIZE, because it has to contain the predecessor and successor field.
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
        if (oldsize - newsize < 2 * DSIZE)
            return ptr;
        char *header = HDRP(ptr), *footer = FTRP(ptr);
        PUT(header, PACK(newsize, 1));
        PUT(FTRP(ptr), PACK(newsize, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldsize - newsize, 0));
        PUT(footer, PACK(oldsize - newsize, 0));

        /* Coalesce if the next block was free */
        void *p = coalesce(NEXT_BLKP(ptr));
        add_to_freelist(p);
    }
    else
    {
        size_t addsize = newsize - oldsize;
        if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0 && GET_SIZE(HDRP(NEXT_BLKP(ptr))) >= addsize)
        {
            delete_from_freelist(NEXT_BLKP(ptr));
            char *header = HDRP(ptr);
            char *nheader = HDRP(NEXT_BLKP(ptr)), *nfooter = FTRP(NEXT_BLKP(ptr));
            size_t nsize = GET_SIZE(nheader);
            if (nsize - addsize < 2 * DSIZE)
            {
                PUT(header, PACK(oldsize + nsize, 1));
                PUT(FTRP(ptr), PACK(oldsize + nsize, 1));
            }
            else
            {
                PUT(header, PACK(newsize, 1));
                PUT(FTRP(ptr), PACK(newsize, 1));
                PUT(HDRP(NEXT_BLKP(ptr)), PACK(nsize - addsize, 0));
                PUT(nfooter, PACK(nsize - addsize, 0));
                add_to_freelist(NEXT_BLKP(ptr));
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

/*
 * print_freelist - Print the freelist.
 */
static void print_freelist()
{
    int count = 0;
    void *p = freelist;
    while (p != NULL)
    {
        printf("%x -> ", (unsigned int)p);
        p = GET_SUCC(p);
        count++;
        if (count > 10)
        {
            printf("\n");
            return;
        }
    }
    printf("^\n");
}