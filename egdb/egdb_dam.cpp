#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winbase.h>

#include "project.h"
#include "board.h"
#include "bool.h"
#include "bicoef.h"
#include "bitcount.h"
#include "egdb_intl.h"
#include "egdb_internal.h"
#include "reverse.h"
#include "compression_tables.h"
#include "indexing.h"
#include "crc.h"

#define PATH_4PIECES "db4/"

#define MAX_SUBSLICE_INDICES (__int64)(0x80000000)

#define INDEXSIZE 4		/* set to 4 or 8. */
#define MAXPIECES 6

#define LOG_HITS 0

/* DAM database values. */
#define DB_WIN	0
#define DB_DRAW 1
#define DB_LOSE 2
#define DB_UNKNOWN 3


#define REQUIRED_PIECES 4

#define IDX_BLOCKSIZE 1024
#define IDX_BLOCKS_PER_L3_BLOCK 4
#define L3_BLOCKSIZE (IDX_BLOCKSIZE * IDX_BLOCKS_PER_L3_BLOCK)

/* We need to read one extra byte at the end of each block because some runlength
 * tokens are 2 bytes, and they may occur at block boundaries.
 */
#define EXTRA_BYTES 206

#define MAXFILENAME MAX_PATH
#define MAXLINE 256
#define MAXFILES 155		/* This is enough for a 6pc database. */



/* Round a value 'a' up to the next integer multiple of blocksize 's'. */
#define ROUND_UP(a, s) (((a) % (s)) ? ((1 + (a) / (s)) * (s)) : (a))

#define IDX_READBUFSIZE 20000
#define RANK0MAX 4
#define NUMBEROFCOLORS 2

#define MALLOC_ALIGNSIZE 16
#define UNDEFINED_BLOCK_ID -1

/* Allocate cache buffers CACHE_ALLOC_COUNT at a time. */
#define CACHE_ALLOC_COUNT 256

/* Size for reading huge files. */
#define CHUNKSIZE 0x100000

/* If he didn't give us enough memory for at least MIN_CACHE_BUF_BYTES of
 * buffers, then use that much.
 */
#define ONE_MB 1048576
#define MIN_CACHE_BUF_BYTES (10 * ONE_MB)

/* Use this macro to do the equivalent of multi-dimension array indexing.
 * We need to do this manually because we are dynamically
 * allocating the database table.
 */
#define DBOFFSET(bm, bk, wm, wk) \
	((wk) + \
	 ((wm) * (MAXPIECE + 1)) + \
	 ((bk) * (MAXPIECE + 1) * (MAXPIECE + 1)) + \
	 ((bm) * (MAXPIECE + 1) * (MAXPIECE + 1) * (MAXPIECE + 1)))

/* The number of pointers in the cprsubdatabase array. */
#define DBSIZE ((MAXPIECE + 1) * (MAXPIECE + 1) * (MAXPIECE + 1) * (MAXPIECE + 1))


#if (INDEXSIZE == 4)
typedef unsigned int INDEX;
#else
typedef int64 INDEX;
#endif

/* Upper and lower words of a 64-bit int, for large file access. */
typedef union {
	int64 word64;
	struct {
		unsigned long low32;
		unsigned long high32;
	} words32;
} I64_HIGH_LOW;

typedef struct {
	char is_present;
	char pieces;			/* number of pieces in this db file. */
	char nbm;
	char nbk;
	char nwm;
	char nwk;
	char autoload;			/* statically load the whole file if true. */
	char name[20];			/* db filename prefix. */
	int first_idx_block;	/* probably always 0, can delete? */
	int num_idx_blocks;		/* number of index blocks in this db file. */
	int num_l3_blocks;		/* number of cache blocks in this db file. */
	unsigned char *file_cache;/* if not null the whole db file is here. */
	HANDLE fp;
	int num_data_offsets;
	int *data_offset;		/* array of offset to the first data byte in each index/IDX_BLOCKSIZE block. */
	int *l3_bufferi;		/* An array of indices into l3_cache_buffers[], indexed by L3 block number. */
#if LOG_HITS
	int hits;
#endif
} DBFILE;

/* L3 cache control block type. */
typedef struct {
	int forward;			/* index of next node */
	int backward;			/* index of previous node */
	int l3_block;			/* the l3 block number within database file. */
	DBFILE *dbfile;			/* which database file the block is for. */
	unsigned char *data;	/* data of this block. */
} CCB;

typedef struct {
	char db_filepath[MAXFILENAME];	/* Path to database files. */
	int dbpieces;
	int kings1side;					/* limit, >= 0 */
	int cacheblocks;				/* Total number of db cache blocks. */
	void (*log_msg_fn)(char *);		/* for status and error messages. */
	DBFILE *dbfiles[MAXPIECE + 1][MAXPIECE + 1][MAXPIECE + 1][MAXPIECE + 1];	/* index each file using [nbm][nbk][nwm][nwk] */
	CCB *l3_ccbs;
	int l3_top;				/* index into l3_ccbs[] of least recently used block. */
	DBFILE *linear_files[MAXFILES];	/* a consecutive list of all the files. */
	int numdbfiles;
	EGDB_STATS lookup_stats;
} SLICE32_DATA;

typedef struct {
	char *filename;
	unsigned int crc;
} DBCRC;

typedef struct {
	char nbm;
	char nbk;
	char nwm;
	char nwk;
} SLICE;

static unsigned int pow3[] = {
	1, 3, 9, 27, 81
};

/* A table of crc values for each database file. */
static DBCRC dbcrc[] = {
	{"OOOOvO.cpr", 0x80c9d46f},
	{"OOOOvOO.cpr", 0xd3d86931},
	{"OOOOvX.cpr", 0xa573aee0},
	{"OOOOvXO.cpr", 0x51d33e8e},
	{"OOOOvXX.cpr", 0xfe166e26},
	{"OOOvO.kr_cpr", 0x057a8ee7},
	{"OOOvOO.cpr", 0x8977db0f},
	{"OOOvOOO.cpr", 0x5c0904f1},
	{"OOOvX.kr_cpr", 0x5a8d23d4},
	{"OOOvXO.cpr", 0x2f928ab8},
	{"OOOvXOO.cpr", 0x8d505b5d},
	{"OOOvXX.cpr", 0xd76a6381},
	{"OOOvXXO.cpr", 0xa33fe4d6},
	{"OOOvXXX.cpr", 0x8bf00057},
	{"OOvO.kr_cpr", 0xd3389535},
	{"OOvOO.kr_cpr", 0x26e1e187},
	{"OOvOOO.cpr", 0x80a5903c},
	{"OOvOOOO.cpr", 0xcb60e909},
	{"OOvX.kr_cpr", 0xe2847075},
	{"OOvXO.kr_cpr", 0x92573e30},
	{"OOvXOO.cpr", 0x679d3783},
	{"OOvXOOO.cpr", 0x54df8c1f},
	{"OOvXX.cpr", 0x440398a9},
	{"OOvXXO.cpr", 0x54cb1432},
	{"OOvXXOO.cpr", 0xb135a943},
	{"OOvXXX.cpr", 0x7b4850fc},
	{"OOvXXXO.cpr", 0xe1e42592},
	{"OOvXXXX.cpr", 0x18d2827b},
	{"OvO.kr_cpr", 0x7ad68977},
	{"OvOO.kr_cpr", 0xda1c7854},
	{"OvOOO.kr_cpr", 0xa0d231eb},
	{"OvOOOO.cpr", 0x14de337f},
	{"OvX.kr_cpr", 0xb02a782d},
	{"OvXO.kr_cpr", 0xa0cf1948},
	{"OvXOO.kr_cpr", 0x4440da06},
	{"OvXOOO.cpr", 0x3ebbcfd1},
	{"OvXX.kr_cpr", 0x70fdd7e8},
	{"OvXXO.kr_cpr", 0x92f0938b},
	{"OvXXOO.cpr", 0xca363f5d},
	{"OvXXX.kr_cpr", 0xaa7876d5},
	{"OvXXXO.cpr", 0x98438ad4},
	{"OvXXXX.cpr", 0x5023135f},
	{"XOOOvO.cpr", 0x8fd53c61},
	{"XOOOvOO.cpr", 0x5309e988},
	{"XOOOvX.cpr", 0xc202337a},
	{"XOOOvXO.cpr", 0xd8aae0e4},
	{"XOOOvXX.cpr", 0x9d1b5b25},
	{"XOOvO.kr_cpr", 0x27e8a799},
	{"XOOvOO.cpr", 0xfdc6d1ac},
	{"XOOvOOO.cpr", 0x371b6456},
	{"XOOvX.kr_cpr", 0x9fdd9ca6},
	{"XOOvXO.cpr", 0xd5b50bbd},
	{"XOOvXOO.cpr", 0x84c0a4ad},
	{"XOOvXX.cpr", 0x5714acff},
	{"XOOvXXO.cpr", 0x697cc989},
	{"XOOvXXX.cpr", 0x499b1812},
	{"XOvO.kr_cpr", 0xf99177d2},
	{"XOvOO.kr_cpr", 0xbcf91db8},
	{"XOvOOO.cpr", 0xbafcdd7d},
	{"XOvOOOO.cpr", 0x1297833b},
	{"XOvX.kr_cpr", 0x8021c449},
	{"XOvXO.kr_cpr", 0xd5b4c777},
	{"XOvXOO.cpr", 0x0ffdeff0},
	{"XOvXOOO.cpr", 0x5a64aadb},
	{"XOvXX.kr_cpr", 0x4328e5aa},
	{"XOvXXO.cpr", 0x594184f7},
	{"XOvXXOO.cpr", 0xae70c3fa},
	{"XOvXXX.cpr", 0x9a249671},
	{"XOvXXXO.cpr", 0xc06a54fb},
	{"XOvXXXX.cpr", 0x8f27c151},
	{"XvO.kr_cpr", 0x27a733fa},
	{"XvOO.kr_cpr", 0x3f8b625c},
	{"XvOOO.kr_cpr", 0x2dfdac86},
	{"XvOOOO.cpr", 0xc6da7ed9},
	{"XvX.kr_cpr", 0x4c256de2},
	{"XvXO.kr_cpr", 0x655e1167},
	{"XvXOO.kr_cpr", 0xb87cc825},
	{"XvXOOO.cpr", 0x8aa28284},
	{"XvXX.kr_cpr", 0xf2a02ca5},
	{"XvXXO.kr_cpr", 0x396bc946},
	{"XvXXOO.cpr", 0xbeef2fa9},
	{"XvXXX.kr_cpr", 0x99912f0a},
	{"XvXXXO.cpr", 0x4cf9c8c2},
	{"XvXXXX.cpr", 0x44111be4},
	{"XXOOvO.cpr", 0xb20df6a4},
	{"XXOOvOO.cpr", 0x9abd9b3f},
	{"XXOOvX.cpr", 0xbe7ab2d3},
	{"XXOOvXO.cpr", 0x2471b9ad},
	{"XXOOvXX.cpr", 0x52db6726},
	{"XXOvO.kr_cpr", 0x7a5fc3c7},
	{"XXOvOO.cpr", 0xa0e0d178},
	{"XXOvOOO.cpr", 0xce6b3d07},
	{"XXOvX.kr_cpr", 0xc9c462da},
	{"XXOvXO.cpr", 0x0e8295b4},
	{"XXOvXOO.cpr", 0x08986e38},
	{"XXOvXX.cpr", 0x949f3339},
	{"XXOvXXO.cpr", 0x7a453b0b},
	{"XXOvXXX.cpr", 0xe44a1b26},
	{"XXvO.kr_cpr", 0xd8366104},
	{"XXvOO.kr_cpr", 0x84224fb3},
	{"XXvOOO.cpr", 0x4010858c},
	{"XXvOOOO.cpr", 0xf995e72e},
	{"XXvX.kr_cpr", 0x254e0333},
	{"XXvXO.kr_cpr", 0x3d0af628},
	{"XXvXOO.cpr", 0x830c06df},
	{"XXvXOOO.cpr", 0x0cefeffa},
	{"XXvXX.kr_cpr", 0xa15d9cf9},
	{"XXvXXO.cpr", 0x381753b0},
	{"XXvXXOO.cpr", 0x93a1a701},
	{"XXvXXX.cpr", 0x9452fc09},
	{"XXvXXXO.cpr", 0xd7de279f},
	{"XXvXXXX.cpr", 0xa2561334},
	{"XXXOvO.cpr", 0x06fca0a3},
	{"XXXOvOO.cpr", 0x695bd927},
	{"XXXOvX.cpr", 0xb7344fe6},
	{"XXXOvXO.cpr", 0x0f608efd},
	{"XXXOvXX.cpr", 0xcc04be13},
	{"XXXvO.kr_cpr", 0x7d0cbd31},
	{"XXXvOO.cpr", 0x5cccf5ea},
	{"XXXvOOO.cpr", 0x977cbfd8},
	{"XXXvX.kr_cpr", 0x3d530c47},
	{"XXXvXO.cpr", 0x7ebb559e},
	{"XXXvXOO.cpr", 0x20e20e24},
	{"XXXvXX.cpr", 0x831da5df},
	{"XXXvXXO.cpr", 0xe66dbb7e},
	{"XXXvXXX.cpr", 0xefe15301},
	{"XXXXvO.cpr", 0x4a83be67},
	{"XXXXvOO.cpr", 0xa97431da},
	{"XXXXvX.cpr", 0x61fe8005},
	{"XXXXvXO.cpr", 0x113d922d},
	{"XXXXvXX.cpr", 0x8cdb4694},
};



/* Function prototypes. */
int parseindexfile(SLICE32_DATA *, DBFILE *, int *allocated_bytes);
void build_file_table(SLICE32_DATA *hdat);


static void reset_db_stats(EGDB_DRIVER *handle)
{
	SLICE32_DATA *hdat = (SLICE32_DATA *)handle->internal_data;
#if LOG_HITS
	int i, k;
	DBP *p;

	for (i = 0; i < sizeof(hdat->dbfiles) / sizeof(hdat->dbfiles[0]); ++i) {
		if (hdat->dbfiles[i].pieces > hdat->dbpieces)
			continue;

		if (!hdat->dbfiles[i].is_present)
			continue;
		hdat->dbfiles[i].hits = 0;
	}
	for (i = 0; i < DBSIZE; ++i) {
		p = hdat->cprsubdatabase + i;
		if (p->subdb != NULL) {
			for (k = 0; k < p->num_subslices; ++k) {
				p->subdb[k].hits = 0;
			}
		}
	}

#endif
	memset(&hdat->lookup_stats, 0, sizeof(hdat->lookup_stats));
}


static EGDB_STATS *get_db_stats(EGDB_DRIVER *handle)
{
	SLICE32_DATA *hdat = (SLICE32_DATA *)handle->internal_data;
#if LOG_HITS
	int i, k, count;
	int nb, nw, bk, wk, bm, wm, pieces, color;
	DBFILE *f;
	DBP *p;
	double hitrate;
	char msg[100];

	count = 0;
	for (i = 0; i < sizeof(hdat->dbfiles) / sizeof(hdat->dbfiles[0]); ++i) {
		f = hdat->dbfiles + i;
		if (f->pieces > hdat->dbpieces)
			continue;

		if (!f->is_present)
			continue;
		count += f->hits;
	}

	for (i = 0; i < sizeof(hdat->dbfiles) / sizeof(hdat->dbfiles[0]); ++i) {
		f = hdat->dbfiles + i;
		if (f->hits) {
			hitrate = 10000.0 * (double)f->hits / ((double)count * (double)f->num_idx_blocks);
			sprintf(msg, "%s %d hits, %f hits/iblock\n",
					f->name,
					f->hits,
					hitrate);
			(*hdat->log_msg_fn)(msg);
		}
	}
	(*hdat->log_msg_fn)("\n");
	for (pieces = 7; pieces <= MAXPIECES; ++pieces) {
		for (nb = 1; nb <= MAXPIECE; ++nb) {
			nw = pieces - nb;
			if (nw > nb)
				continue;
			for (bk = 0; bk <= nb; ++bk) {
				bm = nb - bk;
				for (wk = 0; wk <= nw; ++wk) {
					wm = nw - wk;
					if (bm + bk == wm + wk && wk > bk)
						continue;
					for (color = 0; color < 2; ++color) {
						p = hdat->cprsubdatabase + DBOFFSET(bm, bk, wm, wk, color);
						if (p->subdb != NULL) {
							for (k = 0; k < p->num_subslices; ++k) {
								hitrate = 10000.0 * (double)p->subdb[k].hits / ((double)count * (double)p->subdb[k].num_idx_blocks);
								if (hitrate > .001) {
									sprintf(msg, "%d-%d%d%d%d.%d%c  %d hits, %f hits/iblock, %.1fmb\n",
											nb + nw,
											bm, bk, wm, wk, k,
											color == EGDB_BLACK ? 'b' : 'w',
											p->subdb[k].hits,
											hitrate,
											(double)p->subdb[k].num_idx_blocks * (double)IDX_BLOCKSIZE / (double)ONE_MB);
									(*hdat->log_msg_fn)(msg);
								}
							}
						}
					}
				}
			}
		}
	}
#endif
	return(&hdat->lookup_stats);
}


static void read_l3_block_from_file(SLICE32_DATA *hdat, CCB *ccb)
{
	I64_HIGH_LOW filepos;
	DWORD bytes_read;
	BOOL stat;

	filepos.word64 = (int64)(ccb->l3_block * L3_BLOCKSIZE + ccb->dbfile->data_offset[0]);

	/* Seek the start position. */
	if (SetFilePointer(ccb->dbfile->fp, filepos.words32.low32, (long *)&filepos.words32.high32, FILE_BEGIN) == -1) {
		(*hdat->log_msg_fn)("\nseek failed\n");
		return;
	}
	stat = ReadFile(ccb->dbfile->fp, ccb->data, L3_BLOCKSIZE + EXTRA_BYTES, &bytes_read, NULL);
	if (!stat)
		(*hdat->log_msg_fn)("\nError reading file\n");
}


/*
 * Return a pointer to an l3 cache block.
 * Get the least recently used cache
 * block and load it into that.
 * Update the LRU list to make this block the most recently used.
 */
static CCB *load_l3_block(SLICE32_DATA *hdat, DBFILE *db, int l3_block)
{
	int old_l3_block;
	CCB *ccbp;

	++hdat->lookup_stats.lru_cache_loads;

	/* Not cached, need to load this block from disk. */
	ccbp = hdat->l3_ccbs + hdat->l3_top;
	if (ccbp->l3_block != UNDEFINED_BLOCK_ID) {

		/* The LRU block is in use.
		 * Unhook this block from the subdb that was using it.
		 */
		old_l3_block = ccbp->l3_block;
		ccbp->dbfile->l3_bufferi[old_l3_block] = UNDEFINED_BLOCK_ID;
	}

	/* The l3_top block is now free for use. */
	db->l3_bufferi[l3_block] = hdat->l3_top;
	ccbp->dbfile = db;
	ccbp->l3_block = l3_block;

	/* Read this block from disk into l3_top's ccb. */
	read_l3_block_from_file(hdat, hdat->l3_ccbs + hdat->l3_top);

	/* Fix linked list to point to the next oldest entry */
	hdat->l3_top = hdat->l3_ccbs[hdat->l3_top].forward;
	return(ccbp);
}


/*
 * This l3 block was just accessed.
 * Update the lru to make this the most recently used.
 */
static CCB *update_lru(SLICE32_DATA *hdat, DBFILE *db, int ccbi)
{
	int forward, backward;
	CCB *ccbp;

	++hdat->lookup_stats.lru_cache_hits;

	/* This l3 block is already cached.  Update the lru linked list. */
	ccbp = hdat->l3_ccbs + ccbi;

	/* Unlink this ccb and insert as the most recently used.
	 * If this is the head of the list, then just move the lru
	 * list pointer forward one node, effectively moving newest
	 * to the end of the list.
	 */
	if (ccbi == hdat->l3_ccbs[hdat->l3_top].backward)
		/* Nothing to do, this is already the most recently used block. */
		;
	else if (ccbi == hdat->l3_top)
		hdat->l3_top = hdat->l3_ccbs[hdat->l3_top].forward;
	else {

		/* remove ccbi from the lru list. */
		backward = hdat->l3_ccbs[ccbi].backward;
		forward = hdat->l3_ccbs[ccbi].forward;
		hdat->l3_ccbs[backward].forward = forward;
		hdat->l3_ccbs[forward].backward = backward;
		
		/* Insert ccbi at the end of the lru list. */
		backward = hdat->l3_ccbs[hdat->l3_top].backward;
		hdat->l3_ccbs[backward].forward = ccbi;
		hdat->l3_ccbs[ccbi].backward = backward;
		hdat->l3_ccbs[ccbi].forward = hdat->l3_top;
		hdat->l3_ccbs[hdat->l3_top].backward = ccbi;
	}
	return(ccbp);
}


/*
 * Read a file in 64K blocks.  ReadFile cannot read larger sizes than this.
 */
static int read_file_in_chunks(HANDLE fp, unsigned char *buf, int size, int pagesize)
{
	int stat;
	int bytes_read;
	int request_size;

	while (size > 0) {
		if (size >= CHUNKSIZE)
			request_size = CHUNKSIZE;
		else
			request_size = ROUND_UP(size, pagesize);
		stat = ReadFile(fp, buf, request_size, (unsigned long *)&bytes_read, NULL);
		if (bytes_read < request_size)
			return(0);
		if (!stat)
			return(0);
		buf += bytes_read;
		size -= bytes_read;
	}
	return(1);
}


/*
 * Return the maximum number of cacheblocks that could be used if 
 * we had unlimited ram.
 */
static int needed_cache_buffers(SLICE32_DATA *hdat)
{
	int i;
	int count;

	count = 0;
	for (i = 0; i < hdat->numdbfiles; ++i)
		if (hdat->linear_files[i]->pieces <= hdat->dbpieces && !hdat->linear_files[i]->file_cache)
			count += hdat->linear_files[i]->num_l3_blocks;

	return(count);
}


static uint32 position_to_index_dam(EGDB_POSITION *board, SLICE *s)
{
	int square0, piece, bitnum;
	int fields, offset, lp;
	uint32 index;
	BITBOARD bitmap, bmmask, bkmask, wmmask, wkmask;
	uint32 bmindex, bkindex, wmindex, wkindex;
	uint32 wmrange, bkrange, wkrange;

	bmmask = board->black & ~board->king;
	bkmask = board->black & board->king;
	wmmask = board->white & ~board->king;
	wkmask = board->white & board->king;

	/* set bm, bk, wm, wk. */
	s->nbm = bitcount64(bmmask);
	s->nwm = bitcount64(wmmask);
	s->nbk = bitcount64(bkmask);
	s->nwk = bitcount64(wkmask);
	
	/* Set the index for the black men. */
	bitmap = bmmask;
	fields = 45;
	offset = 0;
	for (bmindex = 0, piece = 1; bitmap; ++piece) {

		/* Get square of next black man. */
		bitnum = LSB64(bitmap);

		/* Remove him from the bitmap of black men. */
		bitmap = bitmap ^ ((BITBOARD)1 << bitnum);

		/* Add to his index. */
		square0 = bitnum_to_square0(bitnum);
		lp = square0 - offset;
		bmindex += (bicoef[fields][s->nbm - piece + 1] - bicoef[fields  - lp][s->nbm - piece + 1]);
		fields -= lp + 1;
		offset = square0 + 1;
	}

	/* Set the index for the white men,
	 * accounting for any interferences from the black men.
	 */
	bitmap = wmmask;
	fields = 45;
	offset = 0;
	for (wmindex = 0, piece = 1; bitmap; ++piece) {
		
		/* Get square of most advanced white man. */
		bitnum = LSB64(bitmap);

		/* Remove him from the bitmap of white men. */
		bitmap = bitmap ^ ((BITBOARD)1 << bitnum);

		/* Get the count of black men that are more advanced than this
		 * white man.
		 */
		square0 = bitnum_to_square0(bitnum);
		square0 = square0 - 5 - bitcount64(bmmask & ~ROW0 & (((BITBOARD)1 << bitnum) - 1));

		/* Add to his index. */
		lp = square0 - offset;
		wmindex += (bicoef[fields][s->nwm - piece + 1] - bicoef[fields  - lp][s->nwm - piece + 1]);
		fields -= lp + 1;
		offset = square0 + 1;
	}

	/* Set the index for the black kings, accounting for any interferences
	 * from the black men and white men.
	 */
	bitmap = bkmask;
	fields = 50 - s->nbm - s->nwm;
	offset = 0;
	for (piece = 1, bkindex = 0; bitmap; ++piece) {

		/* Get square of the next black king. */
		bitnum = LSB64(bitmap);

		/* Remove him from the bitmap of black kings. */
		bitmap = bitmap ^ ((BITBOARD)1 << bitnum);

		/* Subtract from his square the count of men on 
		 * squares 0....square-1.
		 * square-1 of a 0000010000000 number is 0000000111111111
		 */
		square0 = bitnum_to_square0(bitnum);
		square0 -= bitcount64((bmmask | wmmask) & (((BITBOARD)1 << bitnum) - 1)); 

		/* Add to his index. */
		lp = square0 - offset;
		bkindex += (bicoef[fields][s->nbk - piece + 1] - bicoef[fields  - lp][s->nbk - piece + 1]);
		fields -= lp + 1;
		offset = square0 + 1;
	}

	/* Set the index for the white kings, accounting for any interferences
	 * from the black men, white men, and black kings.
	 */
	bitmap = wkmask;
	fields = 50 - s->nbm - s->nwm - s->nbk;
	offset = 0;
	for (piece = 1, wkindex = 0; bitmap; ++piece) {

		/* Get square of the next white king. */
		bitnum = LSB64(bitmap);

		/* Remove him from the bitmap of white kings. */
		bitmap = bitmap ^ ((BITBOARD)1 << bitnum);

		/* Subtract from his square the count of interfering pieces
		 * on squares 0....square-1.
		 */
		square0 = bitnum_to_square0(bitnum);
		square0 -= bitcount64((bmmask | bkmask | wmmask) & (((BITBOARD)1 << bitnum) - 1));

		/* Add to his index. */
		lp = square0 - offset;
		wkindex += (bicoef[fields][s->nwk - piece + 1] - bicoef[fields  - lp][s->nwk - piece + 1]);
		fields -= lp + 1;
		offset = square0 + 1;
	}

	wmrange = 1;
	if (s->nwm)
		wmrange = bicoef[45][s->nwm];

	bkrange = 1;
	if (s->nbk)
		bkrange = bicoef[50 - s->nbm - s->nwm][s->nbk];

	wkrange = 1;
	if (s->nwk)
		wkrange = bicoef[50 - s->nbm - s->nwm - s->nbk][s->nwk];

	index = wkindex + bkindex * wkrange + wmindex * wkrange * bkrange + bmindex * wkrange * bkrange * wmrange;

	return(index);
}


/*
 * Returns EGDB_WIN, EGDB_LOSS, EGDB_DRAW, EGDB_UNKNOWN, or EGDB_NOT_IN_CACHE
 * for a position in a database lookup from a compressed database. 
 * First this function computes the index of the current position. 
 * Then it loads the index file of that database to determine in which
 * 1K-block that index resides. 
 * Finally it reads and decompresses that block to find
 * the value of the position.
 * If the value is not already in a cache buffer, the action depends on
 * the argument cl.  If cl is true, DB_NOT_IN_CACHE is returned, 
 * otherwise the disk block is read and cached and the value is obtained.
 */
static int dblookup(EGDB_DRIVER *handle, EGDB_POSITION *p, int color, int cl)
{
	SLICE32_DATA *hdat = (SLICE32_DATA *)handle->internal_data;
	SLICE slice;
	uint32 index;
	int i;
	int idx_blocknum, data_offset;	
	int l3_block;
	unsigned char *diskblock;
	unsigned char byte, value;
	EGDB_POSITION revboard;
	DBFILE *dbfile;

	/* Start tracking db stats here. */
	++hdat->lookup_stats.db_requests;

	if (color == EGDB_BLACK) {
		revboard.black = reverse_board50(p->white);
		revboard.white = reverse_board50(p->black);
		revboard.king = reverse_board50(p->king);
		index = position_to_index_dam(&revboard, &slice);
	}
	else
		index = position_to_index_dam(p, &slice);

	/* if one side has nothing, return appropriate value
	 * this means you can call lookupcompressed with positions
	 * where one side has no material - unlike chinook db.
	 */
	if ((slice.nbm + slice.nbk) == 0) {
		++hdat->lookup_stats.db_returns;
		return(EGDB_WIN);
	}
	if ((slice.nwm + slice.nwk) == 0) {
		++hdat->lookup_stats.db_returns;
		return(EGDB_LOSS);
	}

	if ((slice.nbm + slice.nwm + slice.nwk + slice.nbk > MAXPIECES) || (slice.nbm + slice.nbk > MAXPIECE) || (slice.nwm + slice.nwk > MAXPIECE)) {
		++hdat->lookup_stats.db_not_present_requests;
		return EGDB_UNKNOWN;
	}

	/* get pointer to dbfile. */
	dbfile = hdat->dbfiles[slice.nbm][slice.nbk][slice.nwm][slice.nwk];

	/* check presence. */
	if (dbfile == 0 || !dbfile->is_present) {
		++hdat->lookup_stats.db_not_present_requests;
		return(EGDB_UNKNOWN);
	}

#if LOG_HITS
	++dbfile->hits;
#endif

	/* We know the index and the database, so look in 
	 * the indices array to find the right index block.
	 */
	data_offset = dbfile->data_offset[index / IDX_BLOCKSIZE] - dbfile->data_offset[0];
	idx_blocknum = data_offset / IDX_BLOCKSIZE;

	/* See if this is an autoloaded block. */
	if (dbfile->file_cache) {
		++hdat->lookup_stats.autoload_hits;
		diskblock = dbfile->file_cache + IDX_BLOCKSIZE *
							(dbfile->first_idx_block + idx_blocknum);
	}
	else {
		int ccbi;
		CCB *ccbp;

		/* See if blocknumber is already in cache. */
		l3_block = (dbfile->first_idx_block + idx_blocknum) / IDX_BLOCKS_PER_L3_BLOCK;

		/* Is this block already cached? */
		ccbi = dbfile->l3_bufferi[l3_block];
		if (ccbi != UNDEFINED_BLOCK_ID) {

			/* Already cached.  Update the lru list. */
			ccbp = update_lru(hdat, dbfile, ccbi);
		}
		else {

			/* we must load it.
			 * if the lookup was a "conditional lookup", we don't load the block.
			 */
			if (cl)
				return(EGDB_NOT_IN_CACHE);

			/* If necessary load this block from disk, update lru list. */
			ccbp = load_l3_block(hdat, dbfile, l3_block);
		}

		diskblock = ccbp->data + ((dbfile->first_idx_block + idx_blocknum) %
						IDX_BLOCKS_PER_L3_BLOCK) * IDX_BLOCKSIZE;
	}

	/* The block we were looking for is now pointed to by diskblock.
	 * now we decompress the memory block
	 * Do a linear search for the byte within the block.
	 */
	diskblock += data_offset % IDX_BLOCKSIZE;

	i = index % IDX_BLOCKSIZE;
	do
	{
		byte = *diskblock++;
		if (byte <= 242) {
			i -= 5;
		}
		else if (byte <= 246) {
			if (byte == 246)
				i -= *diskblock++ * 5;
			else
				i -= (byte - 241) * 5;
			byte = 0;
		}
		else if (byte <= 250) {
			if (byte == 250)
				i -= *diskblock++ * 5;
			else
				i -= (byte - 245) * 5;
			byte = 121;
		}
		else if (byte <= 254) {
			if (byte == 254)
				i -= *diskblock++ * 5;
			else
				i -= (byte - 249) * 5;
			byte = 242;
		}
		else {
			i -= *diskblock++ * 5;
			byte = *diskblock++;
		}
	} while (i >= 0);

	++hdat->lookup_stats.db_returns;
	value = (byte / pow3[4 + (i + 1) % 5]) % 3;
	switch (value) {
	case DB_WIN:
		return(EGDB_WIN);
	case DB_DRAW:
		return(EGDB_DRAW);
	case DB_LOSE:
		return(EGDB_LOSS);
	case DB_UNKNOWN:
		return(EGDB_UNKNOWN);
	}

	return(EGDB_UNKNOWN);
}


static void preload_subdb(SLICE32_DATA *hdat, DBFILE *subdb, int *preloaded_cacheblocks)
{
	int i;
	int first_blocknum;
	int last_blocknum;

	first_blocknum = subdb->first_idx_block / IDX_BLOCKS_PER_L3_BLOCK;
	last_blocknum = (subdb->first_idx_block + subdb->num_idx_blocks - 1) / IDX_BLOCKS_PER_L3_BLOCK;
	for (i = first_blocknum; i <= last_blocknum && *preloaded_cacheblocks < hdat->cacheblocks; ++i) {
		if (subdb->l3_bufferi[i] == UNDEFINED_BLOCK_ID) {
			load_l3_block(hdat, subdb, i);
			++(*preloaded_cacheblocks);
		}
	}
}


static void preload_slice(SLICE32_DATA *hdat, SLICE *s, int *preloaded_cacheblocks)
{
	int i;
	DBFILE *f;
	char msg[MAXMSG];

	/* There are only subdbs with white to move. */
	f = hdat->dbfiles[s->nbm][s->nbk][s->nwm][s->nwk];
	if (f != NULL) {
		if (f->autoload)
			return;
		if (f->pieces > hdat->dbpieces)
			return;

		sprintf(msg, "preload db%d-%d%d%d%d\n",
				f->pieces, s->nbm, s->nbk, s->nwm, s->nwk);
		(*hdat->log_msg_fn)(msg);

		for (i = 0; i < f->num_l3_blocks && *preloaded_cacheblocks < hdat->cacheblocks; ++i) {
			if (f->l3_bufferi[i] == UNDEFINED_BLOCK_ID) {
				load_l3_block(hdat, f, i);
				++(*preloaded_cacheblocks);
			}
		}
	}
}


/*
 * Open the endgame db driver for the slice32 database.
 * 
 * pieces is the maximum number of pieces to do lookups for.
 * cache_mb is the amount of ram to use for the driver, in bytes * 1E6.
 * filepath is the path to the database files.
 * msg_fn is a pointer to a function which will log status and error messages.
 * A non-zero return value means some kind of error occurred.  The nature of
 * any errors are communicated through the msg_fn.
 */
static int initdblookup(SLICE32_DATA *hdat, int pieces, int kings1side, int cache_mb, char *filepath, void (*msg_fn)(char *))
{
	int i, j;
	int nbm, nbk, nwm, nwk, nk, npieces;
	int t0, t1;
	char dbname[MAXFILENAME];
	char msg[MAXMSG];
	int allocated_bytes;		/* keep track of heap allocations in bytes. */
	int autoload_bytes;			/* keep track of autoload allocations in bytes. */
	int cache_mb_avail;
	int max_autoload;
	__int64 total_dbsize;
	int size;
	int count;
	DBFILE *f;
	SYSTEM_INFO sysinfo;
	MEMORYSTATUS memstat;
	unsigned char *blockp;		/* Base address of an allocate group of cache buffers. */

	/* Save off some global data. */
	strcpy(hdat->db_filepath, filepath);

	/* Add trailing backslash if needed. */
	size = (int)strlen(hdat->db_filepath);
	if (size && (hdat->db_filepath[size - 1] != '\\'))
		strcat(hdat->db_filepath, "\\");

	hdat->dbpieces = pieces;
	hdat->kings1side = kings1side;
	hdat->log_msg_fn = msg_fn;
	allocated_bytes = 0;
	autoload_bytes = 0;

	/* Get the pagesize info. */
	GetSystemInfo(&sysinfo);
	GlobalMemoryStatus(&memstat);
	sprintf(msg, "\nThere are %I64d kbytes of ram available.\n", (__int64)memstat.dwAvailPhys / 1024);
	(*hdat->log_msg_fn)(msg);

	/* initialize binomial coefficients. */
	initbicoef();

	/* Build table of db filenames. */
	build_file_table(hdat);

	/* Parse index files. */
	for (i = 0; i < hdat->numdbfiles; ++i) {
		int stat;

		/* Dont do more pieces than he asked for. */
		if (hdat->linear_files[i]->pieces > pieces)
			break;

		stat = parseindexfile(hdat, hdat->linear_files[i], &allocated_bytes);

		/* Check for errors from parseindexfile. */
		if (stat)
			return(1);

		/* Calculate the number of cache blocks. */
		if (hdat->linear_files[i]->is_present) {
			hdat->linear_files[i]->num_l3_blocks = hdat->linear_files[i]->num_idx_blocks / IDX_BLOCKS_PER_L3_BLOCK;
			if (hdat->linear_files[i]->num_idx_blocks > hdat->linear_files[i]->num_l3_blocks * IDX_BLOCKS_PER_L3_BLOCK)
				++hdat->linear_files[i]->num_l3_blocks;
		}
	}

	/* Find the total size of all the files that will be used. */
	total_dbsize = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		if (hdat->linear_files[i]->is_present)
			total_dbsize += (__int64)hdat->linear_files[i]->num_l3_blocks * L3_BLOCKSIZE;
	}

#define MIN_AUTOLOAD_RATIO .18
#define MAX_AUTOLOAD_RATIO .35

	/* Calculate how much cache mb to autoload. */
	cache_mb_avail = (cache_mb - allocated_bytes / ONE_MB);
	if (total_dbsize / ONE_MB - cache_mb_avail < 20)
		max_autoload = (int)(total_dbsize / ONE_MB);
	else if (cache_mb_avail < 15) {
		cache_mb_avail = 15;
		max_autoload = (int)(cache_mb_avail * MIN_AUTOLOAD_RATIO);
	}
	else if (cache_mb_avail > 1000)
		max_autoload = (int)(cache_mb_avail * MAX_AUTOLOAD_RATIO);
	else
		max_autoload = (int)((float)cache_mb_avail * (float)(MIN_AUTOLOAD_RATIO + cache_mb_avail * (MAX_AUTOLOAD_RATIO - MIN_AUTOLOAD_RATIO) / 1000.0));

	/* Select the files that will be autoloaded.  Autoload files with the least number of kings first,
	 * and use number of pieces as a second criterea.
	 * First force autoload everything up to 5 pieces;
	 */
#define MIN_AUTOLOAD_PIECES 4
	size = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		f = hdat->linear_files[i];
		if (f && f->is_present && f->pieces <= MIN_AUTOLOAD_PIECES) {
			size += f->num_l3_blocks;
			f->autoload = 1;
			sprintf(msg, "autoload %s\n", f->name);
			(*hdat->log_msg_fn)(msg);
		}
	}

	for (nk = 0; nk < MAXPIECES; ++nk) {
		for (npieces = MIN_AUTOLOAD_PIECES + 1; npieces <= hdat->dbpieces; ++npieces) {
			for (nbk = 0; nbk <= nk && nbk <= npieces; ++nbk) {
				nwk = nk - nbk;
				for (nbm = 0; nbm <= npieces - nbk - nwk; ++nbm) {
					nwm = npieces - nbk - nwk - nbm;
					if (nbm + nbk == 0 || nwm + nwk == 0)
						continue;
					f = hdat->dbfiles[nbm][nbk][nwm][nwk];
					if (!f)
						continue;
					if (!f->is_present || f->autoload)
						continue;
					size += f->num_l3_blocks;
					if ((int)((__int64)size * L3_BLOCKSIZE / ONE_MB) > max_autoload)
						continue;
					f->autoload = 1;
					sprintf(msg, "autoload %s\n", f->name);
					(*hdat->log_msg_fn)(msg);
				}
			}
		}
	}

	/* Open file pointers for each db: keep open all the time. */
	for (i = 0; i < hdat->numdbfiles; ++i) {
		DBFILE *dbf = hdat->linear_files[i];

		if (!dbf->is_present)
			continue;

		if (dbf->pieces <= 4)
			sprintf(dbname, "%s%s", PATH_4PIECES, dbf->name);
		else
			sprintf(dbname, "%s%s", hdat->db_filepath, dbf->name);
		dbf->fp = CreateFile((LPCTSTR)dbname, GENERIC_READ, FILE_SHARE_READ,
						NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY /*| FILE_FLAG_NO_BUFFERING*/,
						NULL);
		if (dbf->fp == INVALID_HANDLE_VALUE) {
			sprintf(msg, "Cannot open %s\n", dbname);
			(*hdat->log_msg_fn)(msg);
			return(1);
		}

		/* Allocate buffers and read files for autoloaded dbs. */
		if (dbf->autoload) {
			size = dbf->num_l3_blocks * L3_BLOCKSIZE + EXTRA_BYTES;
			dbf->file_cache = (unsigned char *)VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
			allocated_bytes += size;
			autoload_bytes += size;
			if (dbf->file_cache == NULL) {
				(*hdat->log_msg_fn)("Cannot allocate memory for autoload array\n");
				return(1);
			}

			/* Seek to the first data byte of the file. */
			if (SetFilePointer(dbf->fp, dbf->data_offset[0], NULL, FILE_BEGIN) == -1) {
				(*hdat->log_msg_fn)("ERROR: autoload seek failed\n");
				return(-1);
			}

			read_file_in_chunks(dbf->fp, dbf->file_cache, size, sysinfo.dwPageSize);

			/* Close the db file, we are done with it. */
			CloseHandle(dbf->fp);
			dbf->fp = INVALID_HANDLE_VALUE;
		}
		else {
			/* These slices are not autoloaded.
			 * Allocate the array of indices into l3_ccbs[].
			 * Each array entry is either an index into l3_ccbs or -1 if that L3 block is not loaded.
			 */
			size = dbf->num_l3_blocks * sizeof(dbf->l3_bufferi[0]);
			dbf->l3_bufferi = (int *)malloc(size);
			allocated_bytes += size;
			if (dbf->l3_bufferi == NULL) {
				(*hdat->log_msg_fn)("Cannot allocate memory for l3_bufferi array\n");
				return(-1);
			}

			/* Init these L3 buffer indices to UNDEFINED_BLOCK_ID, means that L3 cache block is not loaded. */
			for (j = 0; j < dbf->num_l3_blocks; ++j)
				dbf->l3_bufferi[j] = UNDEFINED_BLOCK_ID;
		}
	}

	sprintf(msg, "Allocated %dkb for indexing\n", (allocated_bytes - autoload_bytes) / 1024);
	(*hdat->log_msg_fn)(msg);
	sprintf(msg, "Allocated %dkb for permanent slice caches\n", autoload_bytes / 1024);
	(*hdat->log_msg_fn)(msg);

	/* Figure out how much ram is left for lru cache buffers.
	 * If we autoloaded everything then no need for lru cache buffers.
	 */
	i = needed_cache_buffers(hdat);
	if (i > 0) {
		if ((allocated_bytes + MIN_CACHE_BUF_BYTES) / ONE_MB >= cache_mb) {

			/* We need more memory than he gave us, allocate 10mb of cache
			 * buffers if we can use that many.
			 */
			hdat->cacheblocks = min(MIN_CACHE_BUF_BYTES / L3_BLOCKSIZE, i);
			sprintf(msg, "Allocating the minimum %d cache buffers\n",
							hdat->cacheblocks);
			(*hdat->log_msg_fn)(msg);
		}
		else {
			hdat->cacheblocks = (cache_mb * ONE_MB - allocated_bytes) / 
					(L3_BLOCKSIZE + EXTRA_BYTES + sizeof(CCB));
			hdat->cacheblocks = min(hdat->cacheblocks, i);
		}

		/* Allocate the CCB array. */
		hdat->l3_ccbs = (CCB *)malloc(hdat->cacheblocks * sizeof(CCB));
		if (!hdat->l3_ccbs) {
			(*hdat->log_msg_fn)("Cannot allocate memory for l3_ccbs\n");
			return(1);
		}

		/* Init the lru list. */
		for (i = 0; i < hdat->cacheblocks; ++i) {
			hdat->l3_ccbs[i].forward = i + 1;
			hdat->l3_ccbs[i].backward = i - 1;
			hdat->l3_ccbs[i].l3_block = UNDEFINED_BLOCK_ID;
		}
		hdat->l3_ccbs[hdat->cacheblocks - 1].forward = 0;
		hdat->l3_ccbs[0].backward = hdat->cacheblocks - 1;
		hdat->l3_top = 0;

		if (hdat->cacheblocks > 0) {
			sprintf(msg, "Allocating %d cache buffers of size %d\n",
						hdat->cacheblocks, L3_BLOCKSIZE + EXTRA_BYTES);
			(*hdat->log_msg_fn)(msg);
		}

		/* Allocate the cache buffers in groups of CACHE_ALLOC_COUNT at a time. */
		for (i = 0; i < hdat->cacheblocks; i += CACHE_ALLOC_COUNT) {
			count = min(CACHE_ALLOC_COUNT, hdat->cacheblocks - i);
			size = count * (L3_BLOCKSIZE + EXTRA_BYTES) * sizeof(unsigned char);
			blockp = (unsigned char *)malloc(size);
			if (blockp == NULL) {
				GlobalMemoryStatus(&memstat);
				i = GetLastError();
				(*hdat->log_msg_fn)("ERROR: malloc failure on cache buffers\n");
				return(-1);
			}

			/* Assign the blockinfo[] data pointers. */
			for (j = 0; j < count; ++j)
				hdat->l3_ccbs[i + j].data = blockp + j * (L3_BLOCKSIZE + EXTRA_BYTES) * sizeof(unsigned char);
		}

		/* Preload the cache blocks with data. */
		t0 = GetTickCount();
		count = 0;				/* keep count of cacheblocks that are preloaded. */

		for (nk = 0; nk < MAXPIECES; ++nk) {
			for (npieces = MIN_AUTOLOAD_PIECES + 1; npieces <= hdat->dbpieces; ++npieces) {
				for (nbk = 0; nbk <= nk && nbk <= npieces; ++nbk) {
					nwk = nk - nbk;
					for (nbm = 0; nbm <= npieces - nbk - nwk; ++nbm) {
						if (count >= hdat->cacheblocks)
							break;

						nwm = npieces - nbk - nwk - nbm;
						if (nbm + nbk == 0 || nwm + nwk == 0)
							continue;
						f = hdat->dbfiles[nbm][nbk][nwm][nwk];
						if (!f)
							continue;
						if (!f->is_present || f->autoload)
							continue;

						sprintf(msg, "preload %s\n", f->name);
						(*hdat->log_msg_fn)(msg);

						for (j = 0; j < f->num_l3_blocks && count < hdat->cacheblocks; ++j) {
							/* It might already be cached. */
							if (f->l3_bufferi[j] == UNDEFINED_BLOCK_ID) {
								load_l3_block(hdat, f, j);
								++count;
							}
						}
					}
				}
			}
		}
		t1 = GetTickCount();
		sprintf(msg, "Read %d buffers in %d sec, %.3f msec/buffer\n", 
					hdat->cacheblocks, (t1 - t0) / 1000, 
					(double)(t1 - t0) / (double)hdat->cacheblocks);
		(*hdat->log_msg_fn)(msg);
	}
	else
		hdat->cacheblocks = 0;

	GlobalMemoryStatus(&memstat);
	sprintf(msg, "There are %I64d kbytes of ram available.\n", (__int64)memstat.dwAvailPhys / 1024);
	(*hdat->log_msg_fn)(msg);

	return(0);
}


/* return the number of database indices for this subdb.
 * needs binomial coefficients in the array bicoef[][] = choose from n, k
 */
static int64 getdatabasesize_slice_dam(int bm, int bk, int wm, int wk)
{
	int64 size;

	/* Compute the number of men configurations first.  Place the black men, summing separately
	 * the contributions of 0, 1, 2, 3, or 4 black men on the backrank.
	 */
	size = 0;
	size = bicoef[50 - bm - wm - bk][wk] * bicoef[50 - bm - wm][bk] * bicoef[45][wm] * bicoef[45][bm];

	return(size);
}


/*
 * Parse an db file and write all information in cprsubdatabase[].
 * A nonzero return value means some kind of error occurred.
 */
static int parseindexfile(SLICE32_DATA *hdat, DBFILE *f, int *allocated_bytes)
{
	int i, readcount, values_read, first_data_offset, datasize;
	char name[MAXFILENAME];
	char msg[MAXMSG];
	INDEX num_indexes;
	FILE *fp;

	/* Open the compressed data file. */
	if (f->pieces <= 4)
		sprintf(name, "%s%s", PATH_4PIECES, f->name);
	else
		sprintf(name, "%s%s", hdat->db_filepath, f->name);
	fp = fopen(name, "rb");
	if (!fp) {

		/* We can't find the compressed data file.  Its ok as long as 
		 * this is for more pieces than REQUIRED_PIECES pieces.
		 */
		if (f->pieces > REQUIRED_PIECES) {
			sprintf(msg, "%s not present\n", name);
			(*hdat->log_msg_fn)(msg);
			return(0);
		}
		else {
			sprintf(msg, "\nCannot open %s\n", name);
			(*hdat->log_msg_fn)(msg);
			return(1);
		}
	}

	num_indexes = (INDEX)getdatabasesize_slice_dam(f->nbm, f->nbk, f->nwm, f->nwk);
	f->num_data_offsets = num_indexes / IDX_BLOCKSIZE;
	if ((INDEX)f->num_data_offsets * IDX_BLOCKSIZE < num_indexes)
		++f->num_data_offsets;

	f->is_present = 1;

	/* Allocate the data_offset array. */
	f->data_offset = (int *)calloc(sizeof(f->data_offset[0]), f->num_data_offsets);
	if (f->data_offset == NULL) {
		sprintf(msg, "\nCannot allocate memory for data offset array\n");
		(*hdat->log_msg_fn)(msg);
		return(1);
	}
	*allocated_bytes += sizeof(f->data_offset[0]) * f->num_data_offsets;

	if (f->pieces >= 6)
		readcount = 4;
	else
		readcount = 3;

	/* Read the offsets for each index block. */
	for (i = 0; i < f->num_data_offsets; ++i) {
		values_read = (int)fread(f->data_offset + i, readcount, 1, fp);
		if (values_read != 1) {
			(*hdat->log_msg_fn)("\nError reading file\n");
			return(1);
		}
	}

	first_data_offset = ftell(fp);
	if (first_data_offset != f->data_offset[0]) {
		sprintf(msg, "first data offsets dont agree: %d %d\n", first_data_offset, f->data_offset[f->num_data_offsets - 1]);
		(*hdat->log_msg_fn)(msg);
	}

	/* Find the size of the file. */
	fseek(fp, 0, SEEK_END);
	datasize = ftell(fp) - first_data_offset;
	f->num_idx_blocks = datasize / IDX_BLOCKSIZE;
	if (f->num_idx_blocks * IDX_BLOCKSIZE < datasize)
		++f->num_idx_blocks;

	fclose(fp);

	return(0);
}


static char *database_name(int nbm, int nbk, int nwm, int nwk)
{
	static char name[40];
	int a, i;

	a = 0;

	for (i = 0; i < nwk; i++)
		name[a++] = 'X';
	for (i = 0; i < nwm; i++)
		name[a++] = 'O';
	name[a++] = 'v';
	for (i = 0; i < nbk; i++)
		name[a++] = 'X';
	for (i = 0; i < nbm; i++)
		name[a++] = 'O';
	name[a] = 0;
	return(name);
}


/*
 * Build the table of db filenames.
 */
static void build_file_table(SLICE32_DATA *hdat)
{
	int count;
	int pieces;
	int nb, nw;
	int bm, bk, wm, wk;
	char *ext;

	count = 0;
	for (pieces = 2; pieces <= MAXPIECES; ++pieces) {
		if (pieces > 4)
			ext = "cpr";
		else
			ext = "kr_cpr";
		for (nb = 1; nb <= MAXPIECE; ++nb) {
			nw = pieces - nb;
			for (bk = 0; bk <= nb; ++bk) {
				bm = nb - bk;
				for (wk = 0; wk <= nw; ++wk) {
					wm = nw - wk;
					if (nb == 0 || (wm + wk) == 0)
						continue;
					if (hdat->kings1side >= 0 && (bk > hdat->kings1side || wk > hdat->kings1side))
						if (pieces >= hdat->dbpieces)
							continue;
					hdat->linear_files[count] = (DBFILE *)calloc(1, sizeof(DBFILE));
					sprintf(hdat->linear_files[count]->name, "%s.%s", database_name(bm, bk, wm, wk), ext);
					hdat->linear_files[count]->pieces = pieces;
					hdat->linear_files[count]->nbm = nb - bk;
					hdat->linear_files[count]->nbk = bk;
					hdat->linear_files[count]->nwm = nw - wk;
					hdat->linear_files[count]->nwk = wk;
					hdat->dbfiles[nb - bk][bk][nw - wk][wk] = hdat->linear_files[count];

					++count;
				}
			}
		}
	}
	hdat->numdbfiles = count;
}


static int egdb_close_slice32(EGDB_DRIVER *handle)
{
	SLICE32_DATA *hdat = (SLICE32_DATA *)handle->internal_data;
	DBFILE *f;
	int i, count, size;

	/* Free the cache buffers in groups of CACHE_ALLOC_COUNT at a time. */
	for (i = 0; i < hdat->cacheblocks; i += CACHE_ALLOC_COUNT) {
		count = min(CACHE_ALLOC_COUNT, hdat->cacheblocks - i);
		size = count * (L3_BLOCKSIZE + EXTRA_BYTES) * sizeof(unsigned char);
		free(hdat->l3_ccbs[i].data);
	}

	/* Free the cache control blocks. */
	free(hdat->l3_ccbs);

	for (i = 0; i < ARRAY_SIZE(hdat->linear_files); ++i) {
		f = hdat->linear_files[i];
		if (!f)
			continue;

		if (f->file_cache) {
			VirtualFree(f->file_cache, 0, MEM_RELEASE);
			f->file_cache = 0;
		}
		else {
			free(f->l3_bufferi);
			f->l3_bufferi = 0;
		}

		if (f->data_offset) {
			free(f->data_offset);
			f->data_offset = 0;
		}

		if (f->fp != INVALID_HANDLE_VALUE)
			CloseHandle(f->fp);
		f->fp = INVALID_HANDLE_VALUE;

		f->num_idx_blocks = 0;
		f->num_l3_blocks = 0;
		free(f);
	}
	free(hdat);
	free(handle);
	return(0);
}


static DBCRC *find_file_crc(char *name, DBCRC *table, int size)
{
	int i;

	for (i = 0; i < size; ++i)
		if (!_stricmp(name, table[i].filename))
			return(table + i);

	return(0);
}


static int verify_crc(EGDB_DRIVER *handle, void (*msg_fn)(char *), int *abort, EGDB_VERIFY_MSGS *msgs)
{
	int i;
	int status;
	unsigned int crc;
	DBCRC *pcrc;
	FILE *fp;
	DBFILE *f;
	char filename[MAXFILENAME];
	char msg[MAXMSG];
	SLICE32_DATA *hdat = (SLICE32_DATA *)handle->internal_data;

	status = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		f = hdat->linear_files[i];
		if (!f)
			continue;
		if (f->pieces > hdat->dbpieces)
			continue;

		/* Build the full filename. */
		if (f->pieces <= 4)
			sprintf(filename, "%s%s", PATH_4PIECES, f->name);
		else
			sprintf(filename, "%s%s", hdat->db_filepath, f->name);
		fp = fopen(filename, "rb");
		if (!fp) {
			sprintf(msg, "Cannot open file %s for crc check.\n", filename);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}
		sprintf(msg, "%s\n", filename);
		(*hdat->log_msg_fn)(msg);

		crc = file_crc_calc(fp, abort);
		pcrc = find_file_crc(f->name, dbcrc, ARRAY_SIZE(dbcrc));
		if (crc != pcrc->crc) {
			sprintf(msg, "file %s failed crc check, expected %08x, got %08x\n",
				filename, pcrc->crc, crc);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}
		fclose(fp);

		sprintf(msg, "%s\n", filename);
		(*hdat->log_msg_fn)(msg);

		crc = file_crc_calc(fp, abort);
		if (crc != pcrc->crc) {
			sprintf(msg, "file %s failed crc check, expected %08x, got %08x\n",
				filename, pcrc->crc, crc);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}
		fclose(fp);
	}
	return(status);
}


static int get_pieces(EGDB_DRIVER *handle, int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side)
{
	int i;
	DBFILE *f;
	SLICE32_DATA *hdat = (SLICE32_DATA *)handle->internal_data;

	*max_pieces = 0;
	*max_pieces_1side = 0;
	*max_9pc_kings = 0;
	*max_8pc_kings_1side = 0;

	if (!handle) 
		return(0);

	for (i = 0; i < hdat->numdbfiles; ++i) {
		f = hdat->linear_files[i];
		if (!f)
			continue;
		if (!f->is_present)
			continue;
		if (f->pieces > *max_pieces)
			*max_pieces = f->pieces;
		if (f->nbm + f->nbk > *max_pieces_1side)
			*max_pieces_1side = f->nbm + f->nbk;
		if (f->nwm + f->nwk > *max_pieces_1side)
			*max_pieces_1side = f->nwm + f->nwk;
	}
	return(0);
}

#define TEST 0

#if TEST
#include <dd.h>
#include <engine_api.h>

static int dblookup_temp(EGDB_DRIVER *handle, EGDB_POSITION *p, int color, int cl)
{
	int dd_val, nbm, nbk, nwm, nwk, value, diff;
	extern void kr_to_dd(BOARD *krboard, BOARDTYPE ddboard[93]);

	value = dblookup(handle, p, color, 0);

	nwm = bitcount64(p->white & ~p->king);
	nwk = bitcount64(p->white & p->king);
	nbm = bitcount64(p->black & ~p->king);
	nbk = bitcount64(p->black & p->king);

	kr_to_dd((BOARD *)p, board);
	dd_val = egdb_read_value(color == EGDB_BLACK ? DD_BLACK : DD_WHITE, nbm, nbk, nwm, nwk);
	switch (value) {
	case EGDB_WIN:
		if (dd_val != DB_WIN)
			diff = 1;
		break;

	case EGDB_LOSS:
		if (dd_val != DB_LOSE)
			diff = 1;
		break;

	case EGDB_DRAW:
		if (dd_val != DB_DRAW)
			diff = 1;
		break;

	case EGDB_UNKNOWN:
		if (dd_val != DB_UNKNOWN)
			diff = 1;
		break;
	}
	if (diff) {
		dd_val = egdb_read_value(color == EGDB_BLACK ? DD_BLACK : DD_WHITE, nbm, nbk, nwm, nwk);
		value = dblookup(handle, p, color, 0);
		return(value);
	}
	else
		return(value);
}
#endif



EGDB_DRIVER *egdb_open_dam_wld(int pieces, int kings1side,
				int cache_mb, char *directory, void (*msg_fn)(char *))
{
	int status;
	EGDB_DRIVER *handle;

	handle = (EGDB_DRIVER *)calloc(1, sizeof(EGDB_DRIVER));
	if (!handle) {
		(*msg_fn)("Cannot allocate memory for driver handle.\n");
		return(0);
	}
	handle->internal_data = calloc(1, sizeof(SLICE32_DATA));
	if (!handle->internal_data) {
		(*msg_fn)("Cannot allocate memory for driver handle.\n");
		return(0);
	}
	status = initdblookup((SLICE32_DATA *)handle->internal_data, pieces, kings1side, cache_mb, directory, msg_fn);
	if (status) {
		free(handle->internal_data);
		free(handle);
		return(0);
	}

	handle->get_stats = get_db_stats;
	handle->reset_stats = reset_db_stats;
	handle->verify = verify_crc;
	handle->close = egdb_close_slice32;
	handle->get_pieces = get_pieces;

#if TEST
init_var();
init_databases();		/* simply resets all variables. */
discover_databases(pieces, directory, msg_fn);
	handle->lookup = dblookup_temp;
#else
	handle->lookup = dblookup;
#endif



	return(handle);
}



