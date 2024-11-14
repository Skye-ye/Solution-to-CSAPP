/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include "mm.h"
#include "memlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Sky",
    /* First member's full name */
    "Sky",
    /* First member's email address */
    "skyzheng@whu.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define BASE_PTR_CNT 14

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define GET_LIST_INDEX(size)                                                   \
  (MIN(BASE_PTR_CNT - 1, 31 - __builtin_clz((size) + (size)-1)))

#define ROUND_UP_POWER_2(x) (1 << (32 - __builtin_clz((x)-1)))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_PTR(p) ((void *)GET(p))
#define PUT_PTR(p, ptr) PUT(p, (unsigned int)(ptr))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define PUT_HDR(bp, size, alloc) PUT(HDRP(bp), PACK(size, alloc))
#define PUT_FTR(bp, size, alloc) PUT(FTRP(bp), PACK(size, alloc))

#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)(bp) + WSIZE)
#define PUT_PRED(bp, pred) PUT_PTR(PRED(bp), pred)
#define PUT_SUCC(bp, succ) PUT_PTR(SUCC(bp), succ)
#define GET_PRED(bp) (GET_PTR(PRED(bp)))
#define GET_SUCC(bp) (GET_PTR(SUCC(bp)))

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

#define BASE_OFF(base, n) (base + (n * WSIZE))

static char *heap_listp = 0;

static void *extend_heap(size_t words);
static void insert_block(void *bp);
static void delete_block(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkheap(int verbose);
static void checkblock(void *bp);
static ssize_t checklist(int verbose);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  if ((heap_listp = mem_sbrk(18 * WSIZE)) == (void *)-1) {
    return -1;
  }

  PUT(heap_listp, 0);
  PUT(BASE_OFF(heap_listp, 1), PACK(16 * WSIZE, 1));

  /* Initialize base pointers */
  for (int i = 2; i < 16; i++) {
    PUT(BASE_OFF(heap_listp, i), 0);
  }

  PUT(BASE_OFF(heap_listp, 16), PACK(16 * WSIZE, 1));
  PUT(BASE_OFF(heap_listp, 17), PACK(0, 1));

  heap_listp += 2 * WSIZE;
  return 0;
}

void *mm_malloc(size_t size) {
  size_t asize; /* Adjusted block size */
  char *bp;

  if (heap_listp == 0) {
    mm_init();
  }
  if (size == 0)
    return NULL;

  if (size <= DSIZE)
    asize = 2 * DSIZE; /* Minimum block size is 4 bytes */
  else {
    asize = ALIGN(size + DSIZE); /* Add Header and Footer size */
  }

  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  if ((bp = extend_heap(asize / WSIZE)) == NULL)
    return NULL;
  place(bp, asize);
  return bp;
}

void mm_free(void *bp) {
  if (bp == 0)
    return;

  size_t size = GET_SIZE(HDRP(bp));
  if (heap_listp == 0) {
    mm_init();
  }

  PUT_HDR(bp, size, 0);
  PUT_FTR(bp, size, 0);
  coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size) {
  size_t oldsize, newsize, nextsize, prevsize, combinedsize;
  void *newptr, *nextbp, *prevbp;

  /* If size == 0 then this is just free, and we return NULL. */
  if (size == 0) {
    mm_free(ptr);
    return 0;
  }

  /* If oldptr is NULL, then this is just malloc. */
  if (ptr == NULL) {
    return mm_malloc(size);
  }

  oldsize = GET_SIZE(HDRP(ptr));

  if (size <= DSIZE) {
    newsize = 2 * DSIZE;
  } else {
    newsize = ALIGN(size + DSIZE);
  }

  /* Case 1: new size is smaller than old size */
  if (newsize <= oldsize) {
    place(ptr, newsize);
    return ptr;
  }

  /* Case 2: new size is larger than old size */
  nextbp = NEXT_BLKP(ptr);
  nextsize = GET_SIZE(HDRP(nextbp));
  prevbp = PREV_BLKP(ptr);
  prevsize = GET_SIZE(HDRP(prevbp));

  /* Case 2.1: next block is free and the sum of the size of the current block
   * and the next block is larger than the new size */
  if (!GET_ALLOC(HDRP(nextbp)) && oldsize + nextsize >= newsize) {
    combinedsize = oldsize + nextsize;
    delete_block(nextbp);
    PUT_HDR(ptr, combinedsize, 1);
    PUT_FTR(ptr, combinedsize, 1);
    place(ptr, newsize);
    return ptr;
  }

  /* Case 2.2: previous block is free and the sum of the size of the current
   * block and the previous block is larger than the new size */
  if (!GET_ALLOC(HDRP(prevbp)) && oldsize + prevsize >= newsize) {
    combinedsize = oldsize + prevsize;
    delete_block(prevbp);
    PUT_HDR(prevbp, combinedsize, 1);
    PUT_FTR(prevbp, combinedsize, 1);
    memmove(prevbp, ptr, oldsize - DSIZE); /* payload only */
    place(prevbp, newsize);
    return prevbp;
  }

  /* Case 2.3: both previous and next blocks are free and the sum of the size of
   * the current block, the previous block and the next block is larger than the
   * new size */
  if (!GET_ALLOC(HDRP(nextbp)) && !GET_ALLOC(HDRP(prevbp)) &&
      oldsize + nextsize + prevsize >= newsize) {
    combinedsize = oldsize + nextsize + prevsize;
    delete_block(prevbp);
    delete_block(nextbp);
    PUT_HDR(prevbp, combinedsize, 1);
    PUT_FTR(prevbp, combinedsize, 1);
    memmove(prevbp, ptr, oldsize - DSIZE); /* payload only */
    place(prevbp, newsize);
    return prevbp;
  }

  /* Case 2.4: look for block that fits the size in free list */
  newptr = mm_malloc(size);
  if (newptr == NULL) {
    return NULL;
  }

  /* Copy the old data */
  memmove(newptr, ptr, oldsize - DSIZE); /* payload only */

  /* Free the old block */
  mm_free(ptr);

  return newptr;
}

void mm_check(int verbose) { checkheap(verbose); }

static void *extend_heap(size_t words) {
  char *bp;
  size_t size;
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1) {
    return NULL;
  }
  PUT_HDR(bp, size, 0);
  PUT_FTR(bp, size, 0);
  PUT_HDR(NEXT_BLKP(bp), 0, 1);
  return coalesce(bp);
}

static void insert_block(void *bp) {
  int index = GET_LIST_INDEX(GET_SIZE(HDRP(bp)));
  void *head = BASE_OFF(heap_listp, index);
  void *next = GET_PTR(head);

  /* insert the block to the head of the list */
  PUT_PRED(bp, NULL);

  PUT_SUCC(bp, next);

  if (next != NULL) {
    PUT_PRED(next, bp);
  }

  PUT_PTR(head, bp);
}

static void delete_block(void *bp) {
  if (GET_ALLOC(HDRP(bp))) {
    return;
  }
  size_t size = GET_SIZE(HDRP(bp));
  int index = GET_LIST_INDEX(size);
  void *pred = GET_PRED(bp);
  void *succ = GET_SUCC(bp);

  if (pred != NULL) {
    PUT_SUCC(pred, succ);
  } else {
    PUT_PTR(BASE_OFF(heap_listp, index), succ);
  }

  if (succ != NULL) {
    PUT_PRED(succ, pred);
  }
}

static void place(void *bp, size_t asize) {
  size_t csize = GET_SIZE(HDRP(bp));
  delete_block(bp);

  if ((csize - asize) >= (2 * DSIZE)) { /* There is enough space to split */
    PUT_HDR(bp, asize, 1);
    PUT_FTR(bp, asize, 1);
    bp = NEXT_BLKP(bp);
    PUT_HDR(bp, csize - asize, 0);
    PUT_FTR(bp, csize - asize, 0);
    insert_block(bp);
  } else {
    PUT_HDR(bp, csize, 1);
    PUT_FTR(bp, csize, 1);
  }
}

/* Use best-fit strategy to find a fit block */
static void *find_fit(size_t asize) {
  void *bp;
  void *best_fit = NULL;
  size_t min_diff = (size_t)-1;
  int index;

  for (index = GET_LIST_INDEX(asize); index < BASE_PTR_CNT; index++) {
    for (bp = GET_PTR(BASE_OFF(heap_listp, index)); bp != NULL;
         bp = GET_SUCC(bp)) {
      size_t bsize = GET_SIZE(HDRP(bp));
      if (asize <= bsize) {
        size_t diff = bsize - asize;
        if (diff < min_diff) {
          min_diff = diff;
          best_fit = bp;
          if (diff <= DSIZE)
            return best_fit;
        }
      }
    }
    if (best_fit != NULL)
      return best_fit;
  }
  return NULL;
}

static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) { /* Case 1 */
    insert_block(bp);
    return bp;
  }

  else if (prev_alloc && !next_alloc) { /* Case 2 */
    delete_block(NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT_HDR(bp, size, 0);
    PUT_FTR(bp, size, 0);
  }

  else if (!prev_alloc && next_alloc) { /* Case 3 */
    delete_block(PREV_BLKP(bp));
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT_FTR(bp, size, 0);
    PUT_HDR(PREV_BLKP(bp), size, 0);
    bp = PREV_BLKP(bp);
  }

  else { /* Case 4 */
    delete_block(PREV_BLKP(bp));
    delete_block(NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT_HDR(PREV_BLKP(bp), size, 0);
    PUT_FTR(NEXT_BLKP(bp), size, 0);
    bp = PREV_BLKP(bp);
  }

  insert_block(bp);
  return bp;
}

static void printblock(void *bp) {
  size_t hsize, halloc, fsize, falloc;
  void *pred, *succ;

  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));

  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  if (!halloc) {
    pred = GET_PRED(bp);
    succ = GET_SUCC(bp);
    printf("Free block: \n");
    printf("%p: header: [%d:%c] footer: [%d:%c] pred: %p succ: %p\n", bp, hsize,
           (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'), pred, succ);
  } else {
    printf("Allocated block: \n");
    printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp, hsize,
           (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
  }

  printf("\n");
}

static void checkblock(void *bp) {
  if ((size_t)bp % ALIGNMENT)
    printf("Error: %p is not doubleword aligned\n", bp);
  if (GET(HDRP(bp)) != GET(FTRP(bp)))
    printf("Error: header does not match footer\n");
}

void checkheap(int verbose) {
  char *bp = heap_listp;
  ssize_t count = 0; /* count of free blocks */

  if (verbose)
    printf("Heap (%p):\n", heap_listp);

  if ((GET_SIZE(HDRP(heap_listp)) != 16 * WSIZE) ||
      !GET_ALLOC(HDRP(heap_listp)))
    printf("Bad prologue header\n");
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose)
      printblock(bp);
    checkblock(bp);

    if (!GET_ALLOC(HDRP(bp))) { /* count free blocks */
      count++;
    }
  }

  if (verbose)
    printblock(bp);
  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
    printf("Bad epilogue header\n");

  printf("Free block count: %d\n\n", count);

  if (count != checklist(1)) {
    printf("Error: free block count is not consistent\n");
  }

  if (verbose)
    printf("\n\n\n");
}

static ssize_t checklist(int verbose) {
  void *bp;
  int index;
  ssize_t count = 0; /* count of free blocks */

  if (verbose)
    printf("Checklist:\n");

  for (index = 0; index < BASE_PTR_CNT; index++) {
    for (bp = GET_PTR(BASE_OFF(heap_listp, index)); bp != NULL;
         bp = GET_SUCC(bp)) {
      if (bp <= mem_heap_lo() || bp >= mem_heap_hi()) {
        printf("Error: %p is not in the heap\n", bp);
        return -1;
      }
      if (GET_ALLOC(HDRP(bp))) {
        printf("Error: %p is allocated but in the list\n", bp);
        return -1;
      }
      if (verbose) {
        printf("Index: %d\n", index);
        printblock(bp);
      }
      count++;
    }
  }

  printf("List free block count: %d\n\n", count);

  return count;
}
