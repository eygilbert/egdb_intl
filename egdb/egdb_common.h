#pragma once
#include "egdb/egdb_intl.h"
#include <Windows.h>

#define MAXMSG 256
#define ONE_MB 1048576
#define IDX_READBUFSIZE 20000
#define NUMBEROFCOLORS 2

#define MALLOC_ALIGNSIZE 16
#define UNDEFINED_BLOCK_ID -1

/* Allocate cache buffers CACHE_ALLOC_COUNT at a time. */
#define CACHE_ALLOC_COUNT 512

/* If he didn't give us enough memory for at least MIN_CACHE_BUF_BYTES of
 * buffers, then use that much.
 */
#define MIN_CACHE_BUF_BYTES (10 * ONE_MB)

/* Use this macro to do the equivalent of multi-dimension array indexing.
 * We need to do this manually because we are dynamically
 * allocating the database table.
 */
#define DBOFFSET(bm, bk, wm, wk, color) \
	((color) + \
	 ((wk) * NUMBEROFCOLORS) + \
	 ((wm) * NUMBEROFCOLORS * (MAXPIECE + 1)) + \
	 ((bk) * NUMBEROFCOLORS * (MAXPIECE + 1) * (MAXPIECE + 1)) + \
	 ((bm) * NUMBEROFCOLORS * (MAXPIECE + 1) * (MAXPIECE + 1) * (MAXPIECE + 1)))

/* The number of pointers in the cprsubdatabase array. */
#define DBSIZE ((MAXPIECE + 1) * (MAXPIECE + 1) * (MAXPIECE + 1) * \
			(MAXPIECE + 1) * NUMBEROFCOLORS)

typedef uint32_t INDEX;



/* Round a value 'a' up to the next integer multiple of blocksize 's'. */
#define ROUND_UP(a, s) (((a) % (s)) ? ((1 + (a) / (s)) * (s)) : (a))

#define LOWORD32(x) (uint32_t)((x) & 0xffffffff)

/* Upper and lower words of a 64-bit int, for large file access. */
typedef union {
	int64_t word64;
	struct {
		unsigned long low32;
		unsigned long high32;
	} words32;
} I64_HIGH_LOW;

extern int get_num_subslices(int nbm, int nbk, int nwm, int nwk);
int read_file(HANDLE fp, unsigned char *buf, size_t size, int pagesize);
int set_file_pointer(HANDLE handle, int64_t pos);
int64_t get_file_size(HANDLE handle);
void *aligned_large_alloc(size_t size);
int get_mem_available_mb(void);
int get_pagesize(void);


inline int needs_reversal(int nbm, int nbk, int nwm, int nwk, int color)
{
	if (nwm + nwk > nbm + nbk)
		return(true);

	if (nwm + nwk == nbm + nbk) {
		if (nwk > nbk)
			return(true);
		if (nbm == nwm && nbk == nwk)
			if (color == EGDB_WHITE)
				return(true);
	}
	return(false);
}


/*
 * Do a binary search to find the block number that contains the target index.
 */
__forceinline int find_block(int first, int last, uint32_t block_starts[], uint32_t target)
{
	int mid;

	while (last > first + 1) {
		mid = first + (last - first) / 2;
		if (block_starts[mid] <= target)
			first = mid;
		else
			last = mid;
	}
	return(first);
}


/*
 * Return a pointer to a cache block.
 * Get the least recently used cache
 * block and load it into that.
 * Update the LRU list to make this block the most recently used.
 */
template <class DBHANDLE_T, class CPRSUBDB_T, class CCB_T> CCB_T *load_blocknum(DBHANDLE_T *hdat, CPRSUBDB_T *subdb, int blocknum)
{
	int old_blocknum;
	CCB_T *ccbp;

	++hdat->lookup_stats.lru_cache_loads;

	/* Not cached, need to load this block from disk. */
	ccbp = hdat->ccbs + hdat->ccbs_top;
	if (ccbp->blocknum != UNDEFINED_BLOCK_ID) {

		/* The LRU block is in use.
		 * Unhook this block from the subdb that was using it.
		 */
		old_blocknum = ccbp->blocknum;
		ccbp->subdb->file->cache_bufferi[old_blocknum] = UNDEFINED_BLOCK_ID;
	}

	/* The ccbs_top block is now free for use. */
	subdb->file->cache_bufferi[blocknum] = hdat->ccbs_top;
	ccbp->subdb = subdb;
	ccbp->blocknum = blocknum;

	/* Read this block from disk into ccbs_top's ccb. */
	read_blocknum_from_file(hdat, hdat->ccbs + hdat->ccbs_top);

	assign_subindices(hdat, subdb, ccbp);

	/* Fix linked list to point to the next oldest entry */
	hdat->ccbs_top = hdat->ccbs[hdat->ccbs_top].next;
	return(ccbp);
}


/*
 * This cache block was just accessed.
 * Update the lru to make this the most recently used.
 */
template <class DBHANDLE_T, class DBFILE_T, class CCB_T> CCB_T *update_lru(DBHANDLE_T *hdat, DBFILE_T *db, int ccbi)
{
	int next, prev;
	CCB_T *ccbp;

	++hdat->lookup_stats.lru_cache_hits;

	/* This block is already cached.  Update the lru linked list. */
	ccbp = hdat->ccbs + ccbi;

	/* Unlink this ccb and insert as the most recently used.
	 * If this is the head of the list, then just move the lru
	 * list pointer forward one node, effectively moving newest
	 * to the end of the list.
	 */
	if (ccbi == hdat->ccbs[hdat->ccbs_top].prev)
		/* Nothing to do, this is already the most recently used block. */
		;
	else if (ccbi == hdat->ccbs_top)
		hdat->ccbs_top = hdat->ccbs[hdat->ccbs_top].next;
	else {

		/* remove ccbi from the lru list. */
		prev = hdat->ccbs[ccbi].prev;
		next = hdat->ccbs[ccbi].next;
		hdat->ccbs[prev].next = next;
		hdat->ccbs[next].prev = prev;
		
		/* Insert ccbi at the end of the lru list. */
		prev = hdat->ccbs[hdat->ccbs_top].prev;
		hdat->ccbs[prev].next = ccbi;
		hdat->ccbs[ccbi].prev = prev;
		hdat->ccbs[ccbi].next = hdat->ccbs_top;
		hdat->ccbs[hdat->ccbs_top].prev = ccbi;
	}
	return(ccbp);
}


/*
 * Return the maximum number of cacheblocks that could be used if 
 * we had unlimited ram.
 */
template <class DBHANDLE_T> int needed_cache_buffers(DBHANDLE_T *hdat)
{
	int i;
	int count;

	count = 0;
	for (i = 0; i < hdat->numdbfiles; ++i)
		if (hdat->dbfiles[i].pieces <= hdat->dbpieces && !hdat->dbfiles[i].file_cache)
			count += hdat->dbfiles[i].num_cacheblocks;

	return(count);
}


template <class DBCRC_T> DBCRC_T *find_file_crc(char *name, DBCRC_T *table, int size)
{
	int i;

	for (i = 0; i < size; ++i)
		if (!_stricmp(name, table[i].filename))
			return(table + i);

	return(0);
}


