#include "builddb/indexing.h"
#include "builddb/compression_tables.h"
#include "egdb/crc.h"
#include "egdb/egdb_common.h"
#include "egdb/egdb_intl.h"
#include "egdb/platform.h"
#include "engine/bicoef.h"
#include "engine/bitcount.h"
#include "engine/board.h"
#include "engine/bool.h"
#include "engine/project.h"	// ARRAY_SIZE
#include "engine/reverse.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <utility>

#define MAXPIECES 8
#define MAXPIECE 5

#define LOG_HITS 0

#define SAME_PIECES_ONE_FILE 4

#define IDX_BLOCK_MULT 4
#define FILE_IDX_BLOCKSIZE 4096
#define IDX_BLOCKSIZE (FILE_IDX_BLOCKSIZE * IDX_BLOCK_MULT)
#define IDX_BLOCKS_PER_CACHE_BLOCK 1
#define CACHE_BLOCKSIZE (IDX_BLOCKSIZE * IDX_BLOCKS_PER_CACHE_BLOCK)

#define MAXFILES 200		/* This is enough for an 8pc database. */

/* Having types with the same name as types in other files confuses the debugger. */
#define DBFILE DBFILE_MTC
#define CPRSUBDB CPRSUBDB_MTC
#define DBP DBP_MTC
#define CCB CCB_MTC
#define DBHANDLE DBHANDLE_MTC

typedef struct {
	char is_present;
	char pieces;			/* number of pieces in this db file. */
	char max_pieces_1side;
	char name[20];			/* db filename prefix. */
	int num_idx_blocks;		/* number of index blocks in this db file. */
	int num_cacheblocks;	/* number of cache blocks in this db file. */
	FILE_HANDLE fp;
	int *cache_bufferi;		/* An array of indices into cache_buffers[], indexed by block number. */
} DBFILE;

// definition of a structure for compressed databases
typedef struct {
	char value;				// WIN/LOSS/DRAW if single value, UNKNOWN == 0 else
	unsigned short int startbyte;	// at this byte
	int num_idx_blocks;		// how many index blocks does this db need?
	int first_idx_block;	// where is the first block in it's db?
	INDEX *indices;			// array of first index number in each index block 
							// allocated dynamically, to size n
	DBFILE *file;			// file info for this subdb
} CPRSUBDB;

typedef struct {
	CPRSUBDB *subdb;		/* an array of subdb pointers indexed by subslicenum. */
	int num_subslices;
} DBP;

/* cache control block type. */
typedef struct {
	int next;				/* index of next node */
	int prev;				/* index of previous node */
	int blocknum;			/* the block number within database file. */
	DBFILE *dbfile;			/* which database file the block is for. */
	unsigned char *data;	/* data of this block. */
} CCB;

typedef struct {
	EGDB_TYPE db_type;
	char db_filepath[MAXFILENAME];	/* Path to database files. */
	int dbpieces;
	int cacheblocks;				/* Total number of db cache blocks. */
	void (*log_msg_fn)(char const*);		/* for status and error messages. */
	DBP *cprsubdatabase;
	CCB *ccbs;
	int ccbs_top;		/* index into ccbs[] of least recently used block. */
	int numdbfiles;
	DBFILE dbfiles[MAXFILES];
	EGDB_STATS lookup_stats;
} DBHANDLE;

typedef struct {
	char const *filename;
	unsigned int crc;
} DBCRC;

/* A table of crc values for each database file. */
static DBCRC dbcrc[] = {
	"", 0
};


/* Function prototypes. */
static int parseindexfile(DBHANDLE *, DBFILE *, int *allocated_bytes);
static void build_file_table(DBHANDLE *hdat);


static void reset_db_stats(EGDB_DRIVER *handle)
{
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
	std::memset(&hdat->lookup_stats, 0, sizeof(hdat->lookup_stats));
}


static EGDB_STATS *get_db_stats(EGDB_DRIVER *handle)
{
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
	return(&hdat->lookup_stats);
}


static void read_blocknum_from_file(DBHANDLE *hdat, CCB *ccb)
{
	int64_t filepos;
	int stat;

	filepos = (int64_t)ccb->blocknum * CACHE_BLOCKSIZE;

	/* Seek the start position. */
	if (set_file_pointer(ccb->dbfile->fp, filepos)) {
		(*hdat->log_msg_fn)("seek failed\n");
		return;
	}
	stat = read_file(ccb->dbfile->fp, ccb->data, CACHE_BLOCKSIZE, CACHE_BLOCKSIZE);
	if (!stat)
		(*hdat->log_msg_fn)("Error reading file\n");
}


/*
 * Return a pointer to a cache block.
 * Get the least recently used cache
 * block and load it into that.
 * Update the LRU list to make this block the most recently used.
 */
static CCB *load_blocknum_mtc(DBHANDLE *hdat, DBFILE *db, int blocknum)
{
	int old_blocknum;
	CCB *ccbp;

	++hdat->lookup_stats.lru_cache_loads;

	/* Not cached, need to load this block from disk. */
	ccbp = hdat->ccbs + hdat->ccbs_top;
	if (ccbp->blocknum != UNDEFINED_BLOCK_ID) {

		/* The LRU block is in use.
		 * Unhook this block from the subdb that was using it.
		 */
		old_blocknum = ccbp->blocknum;
		ccbp->dbfile->cache_bufferi[old_blocknum] = UNDEFINED_BLOCK_ID;
	}

	/* The ccbs_top block is now free for use. */
	db->cache_bufferi[blocknum] = hdat->ccbs_top;
	ccbp->dbfile = db;
	ccbp->blocknum = blocknum;

	/* Read this block from disk into ccbs_top's ccb. */
	read_blocknum_from_file(hdat, hdat->ccbs + hdat->ccbs_top);

	/* Fix linked list to point to the next oldest entry */
	hdat->ccbs_top = hdat->ccbs[hdat->ccbs_top].next;
	return(ccbp);
}


/*
 * Return the maximum number of cacheblocks that could be used if 
 * we had unlimited ram.
 */
static int needed_cache_buffers(DBHANDLE *hdat)
{
	int i;
	int count;

	count = 0;
	for (i = 0; i < hdat->numdbfiles; ++i)
		if (hdat->dbfiles[i].pieces <= hdat->dbpieces)
			count += hdat->dbfiles[i].num_cacheblocks;

	return(count);
}


/*
 * Returns DB_WIN, DB_LOSS, DB_DRAW, DB_UNKNOWN, or DB_NOT_IN_CACHE
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
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
	uint32_t index;
	int64_t index64;
	int bm, bk, wm, wk;
	int i, subslicenum;
	int idx_blocknum;	
	int blocknum;
	int reverse_search;
	int returnvalue;
	unsigned char *diskblock;
	INDEX n_idx;
	INDEX *indices;
	EGDB_POSITION revpos;
	DBP *dbp;
	CPRSUBDB *dbpointer;

	/* Start tracking db stats here. */
	++hdat->lookup_stats.db_requests;

	/* set bm, bk, wm, wk. */
	bm = bitcount64(p->black & ~p->king);
	wm = bitcount64(p->white & ~p->king);
	bk = bitcount64(p->black & p->king);
	wk = bitcount64(p->white & p->king);
	
	/* If one side has nothing, return appropriate value. */
	if ((bm + bk) == 0) {
		++hdat->lookup_stats.db_returns;
		return(MTC_LESS_THAN_THRESHOLD);
	}
	if ((wm + wk) == 0) {
		++hdat->lookup_stats.db_returns;
		return(MTC_LESS_THAN_THRESHOLD);
	}

	if ((bm + wm + wk + bk > MAXPIECES) || (bm + bk > MAXPIECE) || (wm + wk > MAXPIECE)) {
		++hdat->lookup_stats.db_not_present_requests;
		return(MTC_LESS_THAN_THRESHOLD);
	}

	/* Reverse the position if material is dominated by white. */
	if (needs_reversal(bm, bk, wm, wk, color)) {
		reverse((BOARD *)&revpos, (BOARD *)p);
		p = &revpos;
		color = OTHER_COLOR(color);
		using std::swap;
		swap(bm, wm);
		swap(bk, wk);
	}

	index64 = position_to_index_slice(p, bm, bk, wm, wk);

	subslicenum = (int)(index64 / (int64_t)MAX_SUBSLICE_INDICES);
	index = (uint32_t)(index64 - (int64_t)subslicenum * (int64_t)MAX_SUBSLICE_INDICES);

	/* get pointer to db. */
	dbp = hdat->cprsubdatabase + DBOFFSET(bm, bk, wm, wk, color);
	dbpointer = dbp->subdb;

	/* check presence. */
	if (dbpointer == 0) {
		++hdat->lookup_stats.db_not_present_requests;
		return(MTC_LESS_THAN_THRESHOLD);
	}

	dbpointer += subslicenum;

	/* Check that there is data for this subslice. */
	if (dbpointer->indices == 0) {			/* ******* note: this may be missing from Ital or English driver. */
		++hdat->lookup_stats.db_not_present_requests;
		return(MTC_LESS_THAN_THRESHOLD);
	}

	/* check if the db contains only a single value. */
	if (dbpointer->value != EGDB_UNKNOWN) {
		++hdat->lookup_stats.db_returns;
		return(dbpointer->value);
	}

	/* We know the index and the database, so look in 
	 * the indices array to find the right index block.
	 */
	indices = dbpointer->indices;
	idx_blocknum = find_block(0, dbpointer->num_idx_blocks, indices, index);
	{
		int ccbi;
		CCB *ccbp;

		/* See if blocknumber is already in cache. */
		blocknum = (dbpointer->first_idx_block + idx_blocknum) / IDX_BLOCKS_PER_CACHE_BLOCK;

		/* Is this block already cached? */
		ccbi = dbpointer->file->cache_bufferi[blocknum];
		if (ccbi != UNDEFINED_BLOCK_ID) {

			/* Already cached.  Update the lru list. */
			ccbp = update_lru<CCB>(hdat, dbpointer->file, ccbi);
		}
		else {

			/* we must load it.
			 * if the lookup was a "conditional lookup", we don't load the block.
			 */
			if (cl)
				return(MTC_LESS_THAN_THRESHOLD);

			/* If necessary load this block from disk, update lru list. */
			ccbp = load_blocknum_mtc(hdat, dbpointer->file, blocknum);
		}

		diskblock = ccbp->data + ((dbpointer->first_idx_block + idx_blocknum) %
						IDX_BLOCKS_PER_CACHE_BLOCK) * IDX_BLOCKSIZE;
	}

	/* The block we were looking for is now pointed to by diskblock.
	 * Do a linear search for the byte within the block.
	 * Search from the end backwards if closer to the end.
	 */
	reverse_search = 0;
	if (dbpointer->num_idx_blocks > idx_blocknum + 1) {

		/* If we are not at the last block. */
		if (indices[idx_blocknum + 1] - index < index - indices[idx_blocknum])
				reverse_search = 1;
	}

	if (reverse_search) {
		n_idx = indices[idx_blocknum + 1];
		i = IDX_BLOCKSIZE - 1;
		while (n_idx > index) {
			n_idx -= runlength_mtc[diskblock[i]];
			i--;
		}
		i++;
	}
	else {
		n_idx = indices[idx_blocknum];

		/* and move through the bytes until we overshoot index
		 * set the start byte in this block: if it's block 0 of a db,
		 * this can be !=0.
		 */
		i = 0;
		if (idx_blocknum == 0)
			i = dbpointer->startbyte;

		while (n_idx <= index) {
			n_idx += runlength_mtc[diskblock[i]];
			i++;
		}

		/* once we are out here, n>index
		 * we overshot again, move back:
		 */
		i--;
		n_idx -= runlength_mtc[diskblock[i]];
	}

	/* Do some simple error checking. */
	if (i < 0 || i >= IDX_BLOCKSIZE) {
		char msg[MAXMSG];

		std::sprintf(msg, "db block array index outside block bounds: %d\nFile %s\n",
					 i, dbpointer->file->name);
		(*hdat->log_msg_fn)(msg);
		return(MTC_LESS_THAN_THRESHOLD);
	}
	
	/* finally, we have found the byte which describes the position we
	 * wish to look up. it is diskblock[i].
	 */
	if (diskblock[i] < MTC_SKIPS) {

		/* It is a compressed byte. Return 1, the value which means
		 * "less than MTC_THRESHOLD moves to conversion". 
		 */
		returnvalue = MTC_LESS_THAN_THRESHOLD;
	}
	else {
		returnvalue = MTC_DECODE(diskblock[i]);

	}

	++hdat->lookup_stats.db_returns;
	return(returnvalue);
}


/*
 * Open the endgame db driver.
 * pieces is the maximum number of pieces to do lookups for.
 * cache_mb is the amount of ram to use for the driver.
 * filepath is the path to the database files.
 * msg_fn is a pointer to a function which will log status and error messages.
 * A non-zero return value means some kind of error occurred.  The nature of
 * any errors are communicated through the msg_fn.
 */
static int initdblookup(DBHANDLE *hdat, int pieces, int cache_mb, char const *filepath, void (*msg_fn)(char const*))
{
	int i, j;
	int t0, t1;
	char dbname[MAXFILENAME];
	char msg[MAXMSG];
	int allocated_bytes;		/* keep track of heap allocations in bytes. */
	int size;
	int count;
	DBFILE *f;
	unsigned char *blockp;		/* Base address of an allocate group of cache buffers. */

	/* Save off some global data. */
	strcpy(hdat->db_filepath, filepath);

	/* Add trailing backslash if needed. */
	size = (int)strlen(hdat->db_filepath);
	if (size && (hdat->db_filepath[size - 1] != '\\'))
		strcat(hdat->db_filepath, "\\");

	hdat->dbpieces = pieces;
	hdat->log_msg_fn = msg_fn;
	allocated_bytes = 0;

	std::sprintf(msg, "Available RAM: %dmb\n", get_mem_available_mb());
	(*hdat->log_msg_fn)(msg);

	init_bitcount();

	/* initialize binomial coefficients. */
	initbicoef();

	/* initialize runlength and lookup array. */
	init_compression_tables();

	/* initialize man index base table. */
	build_man_index_base();

	/* Allocate the cprsubdatabase array. */
	hdat->cprsubdatabase = (DBP *)std::calloc(DBSIZE, sizeof(DBP));
	if (!hdat->cprsubdatabase) {
		(*hdat->log_msg_fn)("Out of memory allocating cprsubdatabase.\n");
		return(1);
	}
	allocated_bytes += DBSIZE * sizeof(DBP);

	/* Build table of db filenames. */
	build_file_table(hdat);

	/* Parse index files. */
	for (i = 0; i < hdat->numdbfiles; ++i) {
		int stat;

		/* Dont do more pieces than he asked for. */
		if (hdat->dbfiles[i].pieces > pieces)
			break;

		stat = parseindexfile(hdat, hdat->dbfiles + i, &allocated_bytes);

		/* Check for errors from parseindexfile. */
		if (stat)
			return(1);

		/* Calculate the number of cache blocks. */
		if (hdat->dbfiles[i].is_present) {
			hdat->dbfiles[i].num_cacheblocks = hdat->dbfiles[i].num_idx_blocks / IDX_BLOCKS_PER_CACHE_BLOCK;
			if (hdat->dbfiles[i].num_idx_blocks > hdat->dbfiles[i].num_cacheblocks * IDX_BLOCKS_PER_CACHE_BLOCK)
				++hdat->dbfiles[i].num_cacheblocks;
		}
	}

	/* Open file handles for each db; leave them open for quick access. */
	for (i = 0; i < hdat->numdbfiles; ++i) {

		/* Dont do more pieces than he asked for. */
		if (hdat->dbfiles[i].pieces > pieces)
			break;

		if (!hdat->dbfiles[i].is_present)
			continue;

		std::sprintf(dbname, "%s%s.cpr_mtc", hdat->db_filepath, hdat->dbfiles[i].name);
		hdat->dbfiles[i].fp = open_file(dbname);

		/* If the file is not there, no problem.  A lot of slices have
		 * no useful mtc data.
		 */
		if (hdat->dbfiles[i].fp == nullptr)
			continue;

		/* Allocate the array of indices into ccbs[].
		 * Each array entry is either an index into ccbs or -1 if that cache block is not loaded.
		 */
		size = hdat->dbfiles[i].num_cacheblocks * sizeof(hdat->dbfiles[i].cache_bufferi[0]);
		hdat->dbfiles[i].cache_bufferi = (int *)std::malloc(size);
		allocated_bytes += size;
		if (hdat->dbfiles[i].cache_bufferi == NULL) {
			(*hdat->log_msg_fn)("Cannot allocate memory for cache_bufferi array\n");
			return(-1);
		}

		/* Init these buffer indices to UNDEFINED_BLOCK_ID, means that cache block is not loaded. */
		for (j = 0; j < hdat->dbfiles[i].num_cacheblocks; ++j)
			hdat->dbfiles[i].cache_bufferi[j] = UNDEFINED_BLOCK_ID;
	}

	std::sprintf(msg, "Allocated %dkb for indexing\n", allocated_bytes / 1024);
	(*hdat->log_msg_fn)(msg);

	/* Figure out how much ram is left for lru cache buffers. */
	i = needed_cache_buffers(hdat);
	if (i > 0) {
		if ((allocated_bytes + MIN_CACHE_BUF_BYTES) / ONE_MB >= cache_mb) {

			/* We need more memory than he gave us, allocate 10mb of cache
			 * buffers if we can use that many.
			 */
			hdat->cacheblocks = (std::min)(MIN_CACHE_BUF_BYTES / CACHE_BLOCKSIZE, i);
			std::sprintf(msg, "Allocating the minimum %d cache buffers\n",
							hdat->cacheblocks);
			(*hdat->log_msg_fn)(msg);
		}
		else
			hdat->cacheblocks = (cache_mb * ONE_MB - allocated_bytes) / 
					(CACHE_BLOCKSIZE + sizeof(CCB));
	}
	else {
		assert(false);
		hdat->cacheblocks = 0;
	}

	/* Allocate the CCB array. */
	if (hdat->cacheblocks) {
		hdat->ccbs = (CCB *)std::malloc(hdat->cacheblocks * sizeof(CCB));
		if (!hdat->ccbs) {
			(*hdat->log_msg_fn)("Cannot allocate memory for ccbs\n");
			return(1);
		}

		/* Init the lru list. */
		for (i = 0; i < hdat->cacheblocks; ++i) {
			hdat->ccbs[i].next = i + 1;
			hdat->ccbs[i].prev = i - 1;
			hdat->ccbs[i].blocknum = UNDEFINED_BLOCK_ID;
		}
		hdat->ccbs[hdat->cacheblocks - 1].next = 0;
		hdat->ccbs[0].prev = hdat->cacheblocks - 1;
		hdat->ccbs_top = 0;

		if (hdat->cacheblocks > 0) {
			std::sprintf(msg, "Allocating %d cache buffers of size %d\n",
						hdat->cacheblocks, CACHE_BLOCKSIZE);
			(*hdat->log_msg_fn)(msg);
		}

		/* Allocate the cache buffers in groups of CACHE_ALLOC_COUNT at a time. */
		for (i = 0; i < hdat->cacheblocks; i += CACHE_ALLOC_COUNT) {
			count = (std::min)(CACHE_ALLOC_COUNT, hdat->cacheblocks - i);
			size = count * CACHE_BLOCKSIZE * sizeof(unsigned char);
			blockp = (unsigned char *)aligned_large_alloc(size);
			if (blockp == NULL) {
				(*hdat->log_msg_fn)("aligned_large_alloc failure on cache buffers\n");
				return(-1);
			}

			/* Assign the ccb data pointers. */
			for (j = 0; j < count; ++j)
				hdat->ccbs[i + j].data = blockp + j * CACHE_BLOCKSIZE * sizeof(unsigned char);
		}

		/* Preload the cache blocks with data.
		 * First do the slices from the preload table.
		 */
		t0 = std::clock();
		count = 0;				/* keep count of cacheblocks that are preloaded. */

		/* Preload files */
		for (i = 0; i < hdat->numdbfiles && count < hdat->cacheblocks; ++i) {
			f = hdat->dbfiles + i;
			if (!f || !f->is_present)
				continue;

			if (f->fp == nullptr)
				continue;

			std::sprintf(msg, "preload %s\n", f->name);
			(*hdat->log_msg_fn)(msg);

			for (j = 0; j < f->num_cacheblocks && count < hdat->cacheblocks; ++j) {
				/* It might already be cached. */
				if (f->cache_bufferi[j] == UNDEFINED_BLOCK_ID) {
					load_blocknum_mtc(hdat, f, j);
					++count;
				}
			}
			if (count >= hdat->cacheblocks)
				break;
		}
		t1 = std::clock();
		std::sprintf(msg, "Read %d buffers in %d sec, %.3f msec/buffer\n", 
					hdat->cacheblocks, (t1 - t0) / 1000, 
					(double)(t1 - t0) / (double)hdat->cacheblocks);
		(*hdat->log_msg_fn)(msg);
	}

	std::sprintf(msg, "Available RAM: %dmb\n", get_mem_available_mb());
	(*hdat->log_msg_fn)(msg);

	return(0);
}


/*
 * Parse an index file and write all information in cprsubdatabase[].
 * A nonzero return value means some kind of error occurred.
 *
 * Some sample lines from an index file:
 *
 * BASE2,2,0,2,1,0,b:37859/350
 * BASE2,2,0,2,1,0,w:37859/727
 * 188142
 * 2187878
 * BASE2,2,0,2,0,0,b:+
 *
 */
static int parseindexfile(DBHANDLE *hdat, DBFILE *f, int *allocated_bytes)
{
	int stat0, stat;
	char name[MAXFILENAME];
	char msg[MAXMSG];
	FILE *fp;
	char c, colorchar;
	int bm, bk, wm, wk, subslicenum, color;
	int startbyte;
	INDEX indices[IDX_READBUFSIZE];				/* Use temp stack buffer to avoid fragmenting the heap. */
	int i, count, linecount;
	INDEX block_index_start;
	int first_idx_block;
	CPRSUBDB *dbpointer;
	int size;
	int64_t filesize;
	FILE_HANDLE cprfp;
	DBP *dbp;

	/* Open the compressed data file. */
	std::sprintf(name, "%s%s.cpr_mtc", hdat->db_filepath, f->name);
	cprfp = open_file(name);

	/* No problem if we cant open a file.  Most slices dont have any
	 * useful mtc data.
	 */
	if (cprfp == nullptr)
		return(0);

	/* Get the size in bytes and index blocks. */
	filesize = get_file_size(cprfp);
	f->num_idx_blocks = (int)(filesize / IDX_BLOCKSIZE);
	if (filesize % IDX_BLOCKSIZE)
		++f->num_idx_blocks;

	/* Close the file. */
	if (!close_file(cprfp)) {
		std::sprintf(msg, "Error from close_file\n");
		(*hdat->log_msg_fn)(msg);
		return(1);
	}

	std::sprintf(name, "%s%s.idx_mtc", hdat->db_filepath, f->name);
	fp = std::fopen(name, "r");
	if (fp == nullptr) {
		std::sprintf(msg, "cannot open index file %s\n", name);
		(*hdat->log_msg_fn)(msg);
		return(1);
	}

	f->is_present = 1;
	while (1) {

		/* At this point it has to be a BASE line or else end of file. */
		stat = std::fscanf(fp, " BASE%i,%i,%i,%i,%i,%c:",
				&bm, &bk, &wm, &wk, &subslicenum, &colorchar);

		/* Done if end-of-file reached. */
		if (stat <= 0)
			break;
		else if (stat < 6) {
			std::sprintf(msg, "Error parsing %s\n", name);
			(*hdat->log_msg_fn)(msg);
			return(1);
		}

		/* Decode the color. */
		if (colorchar == 'b')
			color = EGDB_BLACK;
		else
			color = EGDB_WHITE;

		/* Get the subdb node. */
		dbp = hdat->cprsubdatabase + DBOFFSET(bm, bk, wm, wk, color);

		/* If we have not allocated a subslice table for this subdb, then
		 * do it now.
		 */
		if (!dbp->subdb) {
			dbp->num_subslices = get_num_subslices(bm, bk, wm, wk);
			dbp->subdb = (CPRSUBDB *)std::calloc(dbp->num_subslices, sizeof(CPRSUBDB));
			*allocated_bytes += dbp->num_subslices * sizeof(CPRSUBDB);
			if (!dbp->subdb) {
				(*hdat->log_msg_fn)("Cannot allocate subslice subdb\n");
				return(1);
			}
		}
		dbpointer = dbp->subdb + subslicenum;

		/* Get the rest of the line.  For mtc index files it has to be n/n. */
		stat = std::fgetc(fp);
		if (std::isdigit(stat)) {
			std::ungetc(stat, fp);
			stat0 = std::fscanf(fp, "%d/%d", &first_idx_block, &startbyte);
			if (stat0 < 2) {
				stat = std::fscanf(fp, "%c", &c);
				if (stat < 1) {
					std::sprintf(msg, "Bad line in %s\n", name);
					(*hdat->log_msg_fn)(msg);
					return(1);
				}
			}
            dbpointer->first_idx_block = first_idx_block / IDX_BLOCK_MULT;       /* which block in db. */
			dbpointer->value = 0;
			dbpointer->startbyte = startbyte + (first_idx_block % IDX_BLOCK_MULT) * FILE_IDX_BLOCKSIZE;
			dbpointer->file = f;

			/* Check for one or more info lines beginning with a '#'. */
			while (1) {
				stat = std::fgetc(fp);
				if (stat == '\n')
					continue;
				if (stat == '#') {
					char line[120];

					std::fgets(line, sizeof(line), fp);
				}
				else {
					if (stat != EOF)
						std::ungetc(stat, fp);
					break;
				}
			}

			/* We got the first line, maybe there are more.
			 * Assign the array of first indices of each block.
			 */
			count = 0;
			i = 1;
			linecount = first_idx_block % IDX_BLOCK_MULT;
			indices[0] = 0;		/* first block is index 0. */
			while (std::fscanf(fp, "%d", &block_index_start) == 1) {
				if (++linecount >= IDX_BLOCK_MULT) {
					linecount = 0;
					indices[i] = block_index_start;
					i++;

					/* If the local buffer is full then allocate more space
					 * and copy.
					 */
					if (i == IDX_READBUFSIZE) {
						dbpointer->indices = (INDEX *)std::realloc(dbpointer->indices, (count + IDX_READBUFSIZE) * sizeof(dbpointer->indices[0]));
						if (dbpointer->indices == NULL) {
							(*hdat->log_msg_fn)("std::malloc error for indices array!\n");
							return(1);
						}
						std::memcpy(dbpointer->indices + count, indices, IDX_READBUFSIZE * sizeof(dbpointer->indices[0]));
						count += IDX_READBUFSIZE;
						i = 0;
					}
				}
			}

			/* We stopped reading numbers.  Resize the indices array and
			 * copy index numbers from the local buffer.
			 */
			if (i > 0) {
				dbpointer->indices = (INDEX *)std::realloc(dbpointer->indices, (count + i) * sizeof(dbpointer->indices[0]));
				if (dbpointer->indices == NULL) {
					(*hdat->log_msg_fn)("std::malloc error for indices array!\n");
					return(1);
				}
				std::memcpy(dbpointer->indices + count, indices, i * sizeof(dbpointer->indices[0]));
			}
			count += i;
			dbpointer->num_idx_blocks = count;
			size = count * sizeof(dbpointer->indices[0]);
			*allocated_bytes += ROUND_UP(size, MALLOC_ALIGNSIZE);
		}
		else {
			(*hdat->log_msg_fn)("Error parsing mtc index file.\n");
			return(1);
		}
	}
	std::fclose(fp);

	/* Check the total number of index blocks in this database. */
	if (f->num_idx_blocks != count + dbpointer->first_idx_block) {
		std::sprintf(msg, "count of index blocks dont match: %d %d\n",
				f->num_idx_blocks, count + dbpointer->first_idx_block);
		(*hdat->log_msg_fn)(msg);
	}
	std::sprintf(msg, "%10d index blocks: %s\n", count + dbpointer->first_idx_block, name);
	(*hdat->log_msg_fn)(msg);

	return(0);
}


/*
 * Build the table of db filenames.
 */
static void build_file_table(DBHANDLE *hdat)
{
	int count;
	int npieces;
	int nb, nw;
	int nbm, nbk, nwm, nwk;

	count = 0;
	for (npieces = 2; npieces <= MAXPIECES; ++npieces) {
		if (npieces <= SAME_PIECES_ONE_FILE) {
			std::sprintf(hdat->dbfiles[count].name, "db%d", npieces);
			hdat->dbfiles[count].pieces = npieces;
			hdat->dbfiles[count].max_pieces_1side = (std::min)(npieces - 1, MAXPIECE);
			hdat->dbfiles[count].fp = nullptr;
			++count;
		}
		else {
			if (npieces > hdat->dbpieces)
				continue;
			for (nb = 1; nb < npieces; ++nb) {
				if (nb > MAXPIECE)
					continue;
				nw = npieces - nb;
				if (nw > nb)
					continue;
				for (nbk = 0; nbk <= nb; ++nbk) {
					nbm = nb - nbk;
					for (nwk = 0; nwk <= nw; ++nwk) {
						nwm = nw - nwk;
						if (nbm + nbk == nwm + nwk && nwk > nbk)
							continue;

						std::sprintf(hdat->dbfiles[count].name, "db%d-%d%d%d%d",
									npieces, nbm, nbk, nwm, nwk);
						hdat->dbfiles[count].pieces = npieces;
						hdat->dbfiles[count].max_pieces_1side = nbm + nbk;
						hdat->dbfiles[count].fp = nullptr;
						++count;
					}
				}
			}
		}
	}
	hdat->numdbfiles = count;
}


static int egdb_close(EGDB_DRIVER *handle)
{
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
	int i, k;
	DBP *p;

	/* Free the cache buffers in groups of CACHE_ALLOC_COUNT at a time. */
	for (i = 0; i < hdat->cacheblocks; i += CACHE_ALLOC_COUNT) {
		virtual_free(hdat->ccbs[i].data);
	}

	/* Free the cache control blocks. */
	std::free(hdat->ccbs);

	for (i = 0; i < sizeof(hdat->dbfiles) / sizeof(hdat->dbfiles[0]); ++i) {
		if (hdat->dbfiles[i].pieces > hdat->dbpieces)
			continue;

		if (!hdat->dbfiles[i].is_present)
			continue;

		std::free(hdat->dbfiles[i].cache_bufferi);
		hdat->dbfiles[i].cache_bufferi = 0;

		if (hdat->dbfiles[i].fp != nullptr)
			close_file(hdat->dbfiles[i].fp);

		hdat->dbfiles[i].num_idx_blocks = 0;
		hdat->dbfiles[i].num_cacheblocks = 0;
		hdat->dbfiles[i].fp = nullptr;
	}
	std::memset(hdat->dbfiles, 0, sizeof(hdat->dbfiles));

	for (i = 0; i < DBSIZE; ++i) {
		p = hdat->cprsubdatabase + i;
		if (p->subdb != NULL) {
			for (k = 0; k < p->num_subslices; ++k) {
				if (p->subdb[k].indices)
					std::free(p->subdb[k].indices);
			}
			std::free(p->subdb);
		}
	}
	std::free(hdat->cprsubdatabase);
	std::free(hdat);
	std::free(handle);
	return(0);
}


static int verify_crc(EGDB_DRIVER *handle, void (*msg_fn)(char const*), int *abort, EGDB_VERIFY_MSGS *msgs)
{
	int i;
	int status;
	unsigned int crc;
	DBCRC *pcrc;
	FILE *fp;
	char filename[MAXFILENAME];
	char msg[MAXMSG];
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;

	status = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		if (hdat->dbfiles[i].pieces > hdat->dbpieces)
			continue;

		/* Build the index filename. */
		std::sprintf(filename, "%s.idx", hdat->dbfiles[i].name);
		pcrc = find_file_crc(filename, dbcrc, ARRAY_SIZE(dbcrc));
		if (!pcrc) {
			std::sprintf(msg, "No crc value known for file %s\n", filename);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}
		std::sprintf(filename, "%s%s.idx", hdat->db_filepath, hdat->dbfiles[i].name);
		fp = std::fopen(filename, "rb");
		if (!fp) {
			std::sprintf(msg, "Cannot open file %s for crc check.\n", filename);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}
		std::sprintf(msg, "%s\n", filename);
		(*hdat->log_msg_fn)(msg);

		crc = file_crc_calc(fp, abort);
		if (crc != pcrc->crc) {
			std::sprintf(msg, "file %s failed crc check, expected %08x, got %08x\n",
				filename, pcrc->crc, crc);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}
		std::fclose(fp);

		/* Build the data filename. */
		std::sprintf(filename, "%s.cpr", hdat->dbfiles[i].name);
		pcrc = find_file_crc(filename, dbcrc, ARRAY_SIZE(dbcrc));
		if (!pcrc) {
			std::sprintf(msg, "No crc value known for file %s\n", filename);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}
		std::sprintf(filename, "%s%s.cpr", hdat->db_filepath, hdat->dbfiles[i].name);
		fp = std::fopen(filename, "rb");
		if (!fp) {
			std::sprintf(msg, "Cannot open file %s for crc check.\n", filename);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}

		std::sprintf(msg, "%s\n", filename);
		(*hdat->log_msg_fn)(msg);

		crc = file_crc_calc(fp, abort);
		if (crc != pcrc->crc) {
			std::sprintf(msg, "file %s failed crc check, expected %08x, got %08x\n",
				filename, pcrc->crc, crc);
			(*hdat->log_msg_fn)(msg);
			status = 1;
			continue;
		}
		std::fclose(fp);
	}
	return(status);
}


static int get_pieces(EGDB_DRIVER *handle, int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side)
{
	int i;
	DBFILE *f;
	DBP *p;
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;

	*max_pieces = 0;
	*max_pieces_1side = 0;
	*max_9pc_kings = 0;
	*max_8pc_kings_1side = 0;
	if (!handle) 
		return(0);

	for (i = 0; i < hdat->numdbfiles; ++i) {
		f = hdat->dbfiles + i;
		if (!f)
			continue;
		if (!f->is_present)
			continue;
		if (f->pieces > *max_pieces)
			*max_pieces = f->pieces;
		if (f->max_pieces_1side > *max_pieces_1side)
			*max_pieces_1side = f->max_pieces_1side;
	}
	if (*max_pieces >= 9) {
		p = hdat->cprsubdatabase + DBOFFSET(4, 1, 4, 0, EGDB_BLACK);
		if (p && p->subdb != NULL)
			*max_9pc_kings = 1;
		p = hdat->cprsubdatabase + DBOFFSET(5, 0, 3, 1, EGDB_BLACK);
		if (p && p->subdb != NULL)
			*max_9pc_kings = 1;
	}
	return(0);
}


EGDB_DRIVER *egdb_open_mtc_runlen(int pieces, int kings_1side_8pcs,
				int cache_mb, char const *directory, void (*msg_fn)(char const*), EGDB_TYPE db_type)
{
	int status;
	EGDB_DRIVER *handle;

	handle = (EGDB_DRIVER *)std::calloc(1, sizeof(EGDB_DRIVER));
	if (!handle) {
		(*msg_fn)("Cannot allocate memory for driver handle.\n");
		return(0);
	}
	handle->internal_data = std::calloc(1, sizeof(DBHANDLE));
	if (!handle->internal_data) {
		(*msg_fn)("Cannot allocate memory for driver handle.\n");
		return(0);
	}
	((DBHANDLE *)(handle->internal_data))->db_type = db_type;
	status = initdblookup((DBHANDLE *)handle->internal_data, pieces, cache_mb, directory, msg_fn);
	if (status) {
		std::free(handle->internal_data);
		std::free(handle);
		return(0);
	}

	handle->lookup = dblookup;
	
	handle->get_stats = get_db_stats;
	handle->reset_stats = reset_db_stats;
	handle->verify = verify_crc;
	handle->close = egdb_close;
	handle->get_pieces = get_pieces;
	return(handle);
}



