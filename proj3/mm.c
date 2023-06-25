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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20211569",
    /* Your full name*/
    "Seungyeon Lee",
    /* Your email address */
    "tmddus4671@sogang.ac.kr",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12) // heap 확장 위한 기본 size

#define MAX(x, y) (x > y ? x : y)
#define GET(p) (*(unsigned int *)(p))  
/* 크기, 할당 비트 통합해서 header, footer에 값 리턴 */
#define PACK(size, alloc) ((size) | (alloc)) 
/* 인자 p가 가리키는 워드에 val을 저장 */
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define HDRP(bp) ((char *)(bp) - WSIZE) // Header 포인터
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // Footer 포인터

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)) // 다음 block 포인터
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) // 이전 block 포인터
#define SET_PTR(p, bp) (*(unsigned int *)(p) = (unsigned int)(bp))

#define GET_NEXT(bp) (*(char **)((char *)(bp) + WSIZE))
#define GET_PRED(bp) (*(char **)(bp))

#define GET_SIZE(p) (GET(p) & ~0x7) // 뒤에 3자리 없어짐
#define GET_ALLOC(p) (GET(p) & 0x1)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


// static char *heap_listp = NULL;
static char *free_blk = NULL;
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void delete_freeblkp(void *bp);
static void insert_freeblkp(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    free_blk = NULL;

    size_t init_size = 8 * WSIZE;
    void *block = mem_sbrk(init_size);
    if (block == (void *)-1)
        return -1;

    PUT(block, 0);
    PUT(block + (1 * WSIZE), PACK(2 * WSIZE, 1));
    PUT(block + (2 * WSIZE), PACK(2 * WSIZE, 1));
    PUT(block + (3 * WSIZE), PACK(4 * WSIZE, 0));
    PUT(block + (6 * WSIZE), PACK(4 * WSIZE, 0));
    PUT(block + (7 * WSIZE), PACK(0, 1));

    free_blk = block + (4 * WSIZE);

    size_t extend_size = CHUNKSIZE / WSIZE;
    if (extend_heap(extend_size) == NULL)
        return -1;

    return 0;
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if(!size) return NULL;
    
    if(size <= DSIZE) asize = 2 * DSIZE;
    else asize = ((size + DSIZE + DSIZE - 1) / DSIZE) * DSIZE;

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);    
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){
    if (!ptr) {
        return mm_malloc(size);
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    if (size < 0) {
        return NULL;
    }

    size_t current_size = GET_SIZE(HDRP(ptr)) - DSIZE;
    size_t copy_size = (size < current_size) ? size : current_size;

    void *newptr = mm_malloc(size);
    if (!newptr) {
        return NULL;
    }

    memcpy(newptr, ptr, copy_size);
    mm_free(ptr);

    return newptr;
}

static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    delete_freeblkp(bp);

    if(csize >= asize + (2 * DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));

        PUT(HDRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
        bp = NEXT_BLKP(bp);
        coalesce(bp);
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    // return bp;
}

static void *find_fit(size_t asize)
{
    void *bp;

    for(bp = free_blk; bp != NULL; bp = GET_NEXT(bp)){
        if ((asize <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL;
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size = (words % 2 == 0) ? words * WSIZE : (words + 1) * WSIZE;

    // 힙 확장
    bp = mem_sbrk(size);
    if (bp == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t cur_size = GET_SIZE(HDRP(bp));
    size_t add_size;

    /* 앞 뒤 모두 alloc */
    if(prev_alloc && next_alloc) {
        insert_freeblkp(bp);
        return bp;
    }
    /* 앞만 free */
    else if(!prev_alloc && next_alloc){
        add_size = GET_SIZE(FTRP(PREV_BLKP(bp)));
        cur_size += add_size;
        // 이전 free block delete
        delete_freeblkp(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(cur_size, 0));
        PUT(FTRP(bp), PACK(cur_size, 0));
        bp = PREV_BLKP(bp);
    }

    /* 뒤만 free */
    else if(prev_alloc && !next_alloc){
        add_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        cur_size += add_size;
        // 다음 free block delete
        delete_freeblkp(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(cur_size, 0));
        PUT(FTRP(bp), PACK(cur_size, 0));
    }

    /* 앞 뒤 모두 free */
    else if(!prev_alloc && !next_alloc){
        add_size = GET_SIZE(FTRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        cur_size += add_size;
        // 이전, 다음 free block delete
        delete_freeblkp(PREV_BLKP(bp));
        delete_freeblkp(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(cur_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(cur_size, 0));
        bp = PREV_BLKP(bp);
    }
    // 처음에 bp 삽입
    insert_freeblkp(bp);
    return bp;
}


static void delete_freeblkp(void *bp) {
    if(bp != free_blk){
        GET_NEXT(GET_PRED(bp)) = GET_NEXT(bp);
        if (GET_NEXT(bp) != NULL) {
            GET_PRED(GET_NEXT(bp)) = GET_PRED(bp);    
        }
    }
    else {
        free_blk = GET_NEXT(free_blk);
        return;
    }

}

static void insert_freeblkp(void *bp) {
    if (free_blk == NULL) {
        free_blk = bp;
        GET_NEXT(bp) = NULL;
        GET_PRED(bp) = NULL;
    } else {
        GET_NEXT(bp) = free_blk;
        GET_PRED(bp) = NULL;
        GET_PRED(free_blk) = bp;
        free_blk = bp;
    }
}