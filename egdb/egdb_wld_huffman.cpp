#include "builddb/compression_tables.h"
#include "builddb/indexing.h"
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
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <utility>

namespace egdb_interface {

#define USE_AUTOLOAD 1

#define MAX_HUFFCODE_BITS 14

#define MAXPIECES 9

#define LOG_HITS 0

#define SAME_PIECES_ONE_FILE 4
#define MIN_AUTOLOAD_PIECES 4

#define NUM_SUBINDICES 64
#define IDX_BLOCK_MULT 1
#define FILE_IDX_BLOCKSIZE 4096
#define IDX_BLOCKSIZE (FILE_IDX_BLOCKSIZE * IDX_BLOCK_MULT)
#define IDX_BLOCKS_PER_CACHE_BLOCK 1
#define CACHE_BLOCKSIZE (IDX_BLOCKSIZE * IDX_BLOCKS_PER_CACHE_BLOCK)
#define SUBINDEX_BLOCKSIZE (IDX_BLOCKSIZE / NUM_SUBINDICES)

#define MAXFILES 200		/* This is enough for an 8pc database. */

/* For subindicies, we need to know the first and second values and the bit offset.
 * bits 0 - 5: bit offset of the start of this subindex.
 * bits 6 - 8: first value.
 * bits 9 - 11: second value.
 */
#define SET_SUBINDEX_INFO(bitoffset, lastvalue, value) ((bitoffset) | ((lastvalue) << 6) | ((value) << 9))

/* Having types with the same name as types in other files confuses the debugger. */
#define DBFILE DBFILE_HUFF
#define CPRSUBDB CPRSUBDB_HUFF
#define DBP DBP_HUFF
#define CCB CCB_HUFF
#define DBHANDLE DBHANDLE_HUFF


typedef unsigned short SUBINDEX_INFO;

typedef union {
	unsigned char bytes[8];
	int64_t word64;
} WORD64_BYTES;

typedef union {
	unsigned char bytes[4];
	unsigned int word32;
} WORD32_BYTES;

typedef struct {
	char is_present;
	char pieces;			/* number of pieces in this db file. */
	char max_pieces_1side;
	char autoload;			/* statically load the whole file if true. */
	char name[20];			/* db filename prefix. */
	int num_idx_blocks;		/* number of index blocks in this db file. */
	int num_cacheblocks;	/* number of cache blocks in this db file. */
	unsigned char *file_cache;/* if not null the whole db file is here. */
	FILE_HANDLE fp;
	int *cache_bufferi;		/* An array of indices into cache_buffers[], indexed by block number. */
#if LOG_HITS
	int hits;
#endif
} DBFILE;

// definition of a structure for compressed databases
typedef struct CPRSUBDB {
	char singlevalue;				/* WIN/LOSS/DRAW if single value, else NOT_SINGLEVALUE. */
	char haspartials;				/* has EGDB_WINorDRAW, EGDB_DRAWorLOSS, EGDB_UNKNOWN values. */
	unsigned char first_subidx_block;	/* modulus NUM_SUBINDICES */
	unsigned char last_subidx_block;	/* modulus NUM_SUBINDICES */
	unsigned char v0;
	unsigned char v1;
	short int startbyte;			/* offset into first index block of first data. */
	int num_idx_blocks;				/* number of index blocks in this subdb. */
	int first_idx_block;			/* index block containing the first data value. */
	INDEX *indices;					/* array of first index number in each index block */
									/* allocated dynamically, to size [num_idx_blocks] */
	unsigned char *start_values;	/* first value and (2nd value << 4) of each index bock. */
	INDEX *autoload_subindices;		/* subindices for autoloaded files. */
	SUBINDEX_INFO *autoload_subindex_info;	/* bit offset, 1st and 2nd values of subindicies. */
	DBFILE *file;					/* file info for this subdb */
	struct CPRSUBDB *next;			/* previous subdb in the same file that is not all one value. */
	struct CPRSUBDB *prev;			/* next subdb in the same file that is not all one value. */
#if LOG_HITS
	int hits;
#endif
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
	CPRSUBDB *subdb;		/* which subdb the block is for; there may be more than 1. */
	unsigned char *data;	/* data of this block. */
	INDEX subindices[NUM_SUBINDICES];
	SUBINDEX_INFO subindex_info[NUM_SUBINDICES];
} CCB;

typedef struct {
	EGDB_TYPE db_type;
	char db_filepath[MAXFILENAME];	/* Path to database files. */
	int dbpieces;
	int kings_1side_8pcs;			/* limit, >= 0 */
	int cacheblocks;				/* Total number of db cache blocks. */
	void (*log_msg_fn)(char const*);		/* for status and error messages. */
	DBP *cprsubdatabase;
	CCB *ccbs;
	int ccbs_top;		/* index into ccbs[] of least recently used block. */
	int numdbfiles;
	DBFILE dbfiles[MAXFILES];
	DBFILE *files_autoload_order[MAXFILES];
	EGDB_STATS lookup_stats;
} DBHANDLE;

typedef struct {
	char const *filename;
	unsigned int crc;
} DBCRC;

static LOCK_TYPE egdb_lock;

/* A table of crc values for each database file. */
static DBCRC dbcrc[] = {
{"db2.idx", 0xa833eebf},
{"db3.idx", 0x82f1a44e},
{"db4.idx", 0xc5f47d67},
{"db5-0302.idx", 0xedd66a70},
{"db5-3020.idx", 0xeee459ed},
};

unsigned char mispredict_table[4][4];

unsigned short huffman_decompress_table[1 << MAX_HUFFCODE_BITS] = {
#include "huffman_decompress.txt"
};

/* Function prototypes. */
static int parseindexfile(DBHANDLE *, DBFILE *, int64_t *allocated_bytes);
static void build_file_table(DBHANDLE *hdat);
static void build_autoload_list(DBHANDLE *hdat);
static void assign_subindices(DBHANDLE *hdat, CPRSUBDB *subdb, CCB *ccbp);

namespace detail {

static void reset_db_stats(EGDB_DRIVER *handle)
{
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
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
	std::memset(&hdat->lookup_stats, 0, sizeof(hdat->lookup_stats));
}

}	// namespace detail

void build_mispredict_table()
{
	int v0, v1;

	for (v0 = 0; v0 < 4; ++v0) {
		for (v1 = 0; v1 < 4; ++v1) {
			switch (v1) {
			case EGDB_WIN:
				if (v0 == EGDB_DRAW)
					mispredict_table[v0][v1] = EGDB_LOSS;
				else
					mispredict_table[v0][v1] = EGDB_DRAW;
				break;

			case EGDB_LOSS:
				if (v0 == EGDB_DRAW)
					mispredict_table[v0][v1] = EGDB_WIN;
				else
					mispredict_table[v0][v1] = EGDB_DRAW;
				break;

			case EGDB_DRAW:
				if (v0 == EGDB_WIN)
					mispredict_table[v0][v1] = EGDB_LOSS;
				else
					mispredict_table[v0][v1] = EGDB_WIN;
				break;
			}
		}
	}
}

namespace detail {

static EGDB_STATS *get_db_stats(EGDB_DRIVER const *handle)
{
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
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
			std::sprintf(msg, "%s %d hits, %f hits/iblock\n",
					f->name,
					f->hits,
					hitrate);
			(*hdat->log_msg_fn)(msg);
		}
	}
	(*hdat->log_msg_fn)("\n");
	for (pieces = SAME_PIECES_ONE_FILE; pieces <= MAXPIECES; ++pieces) {
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
									std::sprintf(msg, "%d-%d%d%d%d.%d%c  %d hits, %f hits/iblock, %.1fmb\n",
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

}	// namespace detail

static void read_blocknum_from_file(DBHANDLE *hdat, CCB *ccb)
{
	int64_t filepos;
	int stat;

	filepos = (int64_t)ccb->blocknum * CACHE_BLOCKSIZE;

	/* Seek the start position. */
	if (set_file_pointer(ccb->subdb->file->fp, filepos)) {
		(*hdat->log_msg_fn)("seek failed\n");
		return;
	}
	stat = read_file(ccb->subdb->file->fp, ccb->data, CACHE_BLOCKSIZE, CACHE_BLOCKSIZE);
	if (!stat)
		(*hdat->log_msg_fn)("Error reading file\n");
}


/*
 * Returns EGDB_WIN, EGDB_LOSS, EGDB_DRAW, EGDB_UNKNOWN, or EGDB_NOT_IN_CACHE.
 * If the position is in an 'incomplete' subdivision, like 5men vs. 4men, it
 * might also return EGDB_DRAW_OR_LOSS or EEGDB_WIN_OR_DRAW.
 * First it converts the position to an index. 
 * Then it determines which block contains the index.
 * It may load that block from disk if it's not already in cache.
 * Finally it reads and decompresses the block to find
 * the value of the position.
 * If the value is not already in a cache buffer, the action depends on
 * the argument cl.  If cl is true, DB_NOT_IN_CACHE is returned, 
 * otherwise the disk block is read and cached and the value is obtained.
 */
static int dblookup(EGDB_DRIVER *handle, EGDB_POSITION const *p, int color, int cl)
{
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
	uint32_t index;
	int64_t index64;
	int bm, bk, wm, wk;
	int i, subslicenum;
	int idx_blocknum, subidx_blocknum;	
	int blocknum;
	unsigned char *diskblock;
	INDEX n_idx;
	INDEX *indices;
	EGDB_POSITION revpos;
	DBP *dbp;
	CPRSUBDB *dbpointer;
	int bitoffset;
	unsigned short value, lastvalue, temp, code, codelength, runlength;
	WORD32_BYTES tablei;
	WORD64_BYTES tablei64;

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
		return(color == EGDB_BLACK ? EGDB_LOSS : EGDB_WIN);
	}
	if ((wm + wk) == 0) {
		++hdat->lookup_stats.db_returns;
		return(color == EGDB_WHITE ? EGDB_LOSS : EGDB_WIN);
	}

	if ((bm + wm + wk + bk > MAXPIECES) || (bm + bk > MAXPIECE) || (wm + wk > MAXPIECE)) {
		++hdat->lookup_stats.db_not_present_requests;
		return EGDB_UNKNOWN;
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
		return(EGDB_UNKNOWN);
	}

	dbpointer += subslicenum;
#if LOG_HITS
	++dbpointer->file->hits;
	++dbpointer->hits;
#endif

	/* check if the db contains only a single value. */
	if (dbpointer->singlevalue != NOT_SINGLEVALUE) {
		++hdat->lookup_stats.db_returns;
		return(dbpointer->singlevalue);
	}

	/* See if this is an autoloaded block. */
	if (dbpointer->file->file_cache) {
		++hdat->lookup_stats.autoload_hits;

		/* Do a binary search to find the exact subindex. */
		indices = dbpointer->autoload_subindices;
		subidx_blocknum = find_block(dbpointer->first_subidx_block,
				dbpointer->num_idx_blocks * NUM_SUBINDICES - 
								(NUM_SUBINDICES - 1 - dbpointer->last_subidx_block),
				indices,
				index);
		diskblock = dbpointer->file->file_cache + (subidx_blocknum * SUBINDEX_BLOCKSIZE) +
					dbpointer->first_idx_block * IDX_BLOCKSIZE;
		n_idx = indices[subidx_blocknum];
		i = 0;
		if (subidx_blocknum == dbpointer->first_subidx_block) {
			bitoffset = 8 * (dbpointer->startbyte - subidx_blocknum * SUBINDEX_BLOCKSIZE);
			lastvalue = dbpointer->v0;
			value = dbpointer->v1;
		}
		else {
			bitoffset = dbpointer->autoload_subindex_info[subidx_blocknum] & 0x3f;
			lastvalue = (dbpointer->autoload_subindex_info[subidx_blocknum] >> 6) & 7;
			value = (dbpointer->autoload_subindex_info[subidx_blocknum] >> 9) & 7;
		}
	}
	else {		/* Not an autoloaded block. */
		int ccbi;
		CCB *ccbp;

		/* We know the index and the database, so look in 
		 * the indices array to find the right index block.
		 */
		indices = dbpointer->indices;
		idx_blocknum = find_block(0, dbpointer->num_idx_blocks, indices, index);

		/* See if blocknumber is already in cache. */
		blocknum = (dbpointer->first_idx_block + idx_blocknum) / IDX_BLOCKS_PER_CACHE_BLOCK;

		{ // BEGIN CRITICAL_SECTION
		        std::lock_guard<LOCK_TYPE> guard(egdb_lock);

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
                                if (cl) return(EGDB_NOT_IN_CACHE);

                                /* If necessary load this block from disk, update lru list. */
                                ccbp = load_blocknum<CCB>(hdat, dbpointer, blocknum);
                        }

                        /* Do a binary search to find the exact subindex.  This is complicated a bit by the
                         * problem that there may be a boundary between the end of one subdb and the start of
                         * the next in this block.  For any subindex block that contains one of these
                         * boundaries, the subindex stored is the ending block (0 is implied for the
                         * starting block).  Therefore the binary search cannot use the first subindex of
                         * a subdb.  We check for this separately.
                         */
                        indices = ccbp->subindices;
                        if (idx_blocknum == 0 && ((dbpointer->num_idx_blocks == 1 && dbpointer->first_subidx_block == dbpointer->last_subidx_block) ||
                                                        dbpointer->first_subidx_block == NUM_SUBINDICES - 1 ||
                                                        indices[dbpointer->first_subidx_block + 1] > index)) {
                                subidx_blocknum = dbpointer->first_subidx_block;
                                n_idx = 0;
                                i = dbpointer->startbyte - subidx_blocknum * SUBINDEX_BLOCKSIZE;
                                bitoffset = 8 * (dbpointer->startbyte - subidx_blocknum * SUBINDEX_BLOCKSIZE);
                                lastvalue = dbpointer->v0;
                                value = dbpointer->v1;
                        }
                        else {
                                int first, last;

                                if (idx_blocknum == 0)
                                        first = dbpointer->first_subidx_block + 1;
                                else
                                        first = 0;
                                if (idx_blocknum == (dbpointer->num_idx_blocks - 1) / IDX_BLOCKS_PER_CACHE_BLOCK)
                                        last = dbpointer->last_subidx_block + 1;
                                else
                                        last = NUM_SUBINDICES;
                                subidx_blocknum = find_block(first, last, indices, index);
                                n_idx = indices[subidx_blocknum];
                                i = 0;
                                bitoffset = ccbp->subindex_info[subidx_blocknum] & 0x3f;
                                lastvalue = (ccbp->subindex_info[subidx_blocknum] >> 6) & 7;
                                value = (ccbp->subindex_info[subidx_blocknum] >> 9) & 7;
                        }
                        diskblock = ccbp->data + subidx_blocknum * SUBINDEX_BLOCKSIZE;
		} // END CRITICAL SECTION
	}

	/* The subindex block we were looking for is now pointed to by diskblock.
	 * Do a linear search for the index within the block.
	 */
	tablei.bytes[3] = 0;
	while (n_idx <= index) {
		tablei.bytes[0] = diskblock[bitoffset / 8];
		tablei.bytes[1] = diskblock[1 + bitoffset / 8];
		tablei.bytes[2] = diskblock[2 + bitoffset / 8];
		tablei.word32 >>= (bitoffset % 8);
		code = huffman_decompress_table[tablei.word32 & 0x3fff];
		codelength = code & 0x0f;
		runlength = code >> 4;
		if (runlength == 0) {
			switch (tablei.word32 & ((1 << codelength) - 1)) {
			case ARB_LENGTH16:
				bitoffset += codelength;
				tablei.bytes[0] = diskblock[bitoffset / 8];
				tablei.bytes[1] = diskblock[1 + bitoffset / 8];
				tablei.bytes[2] = diskblock[2 + bitoffset / 8];
				tablei.word32 >>= (bitoffset % 8);
				n_idx += tablei.word32 & 0xffff;
				bitoffset += 16;
				temp = lastvalue;
				lastvalue = value;
				value = temp;
				break;

			case ARB_LENGTH32:
				bitoffset += codelength;
				tablei64.bytes[0] = diskblock[bitoffset / 8];
				tablei64.bytes[1] = diskblock[1 + bitoffset / 8];
				tablei64.bytes[2] = diskblock[2 + bitoffset / 8];
				tablei64.bytes[3] = diskblock[3 + bitoffset / 8];
				tablei64.bytes[4] = diskblock[4 + bitoffset / 8];
				tablei64.word64 >>= (bitoffset % 8);
				n_idx += (unsigned int)tablei64.word64 & 0xffffffff;
				bitoffset += 32;
				temp = lastvalue;
				lastvalue = value;
				value = temp;
				break;

			case MISPREDICT_UNKNOWN:
				value = EGDB_UNKNOWN;
				bitoffset += codelength;
				break;

			case MISPREDICT_WIN:
				value = EGDB_WIN;
				bitoffset += codelength;
				break;

			case MISPREDICT_LOSS:
				value = EGDB_LOSS;
				bitoffset += codelength;
				break;

			case MISPREDICT_DRAW:
				if (dbpointer->haspartials)
					value = EGDB_DRAW;
				else
					value = mispredict_table[lastvalue][value];
				bitoffset += codelength;
				break;

			case MISPREDICT_DRAWorLOSS:
				value = EGDB_DRAW_OR_LOSS;
				bitoffset += codelength;
				break;

			case MISPREDICT_WINorDRAW:
				value = EGDB_WIN_OR_DRAW;
				bitoffset += codelength;
				break;

			case 0:
				/* We're at the end of a 4k index block. This should not happen here. */
				{
					char msg[MAXMSG];

					std::sprintf(msg, "db block array index outside block bounds: %d\nFile %s\n",
								 i, dbpointer->file->name);
					(*hdat->log_msg_fn)(msg);
					return EGDB_UNKNOWN;
				}
				break;
			}
		}
		else {
			n_idx += runlength;
			bitoffset += codelength;
			temp = lastvalue;
			lastvalue = value;
			value = temp;
		}
	}
		
	++hdat->lookup_stats.db_returns;
	return(value);
}

static int init_autoload_subindices(DBHANDLE *hdat, DBFILE *file, int *allocated_bytes)
{
	int i, k, bitoffset, size, diff, mispredict;
	int first_subi, num_subi, subi, idxblock;
	unsigned short value, lastvalue, temp, code, codelength, runlength;
	DBP *p;
	INDEX index;
	unsigned char *datap;
	WORD32_BYTES tablei;
	WORD64_BYTES tablei64;

	*allocated_bytes = 0;
	tablei.bytes[3] = 0;
	for (i = 0; i < DBSIZE; ++i) {
		p = hdat->cprsubdatabase + i;
		if (p->subdb != NULL) {
			for (k = 0; k < p->num_subslices; ++k) {
				if (p->subdb[k].file == file && p->subdb[k].singlevalue == NOT_SINGLEVALUE) {

					/* This subdb is autoloaded. */
					first_subi = p->subdb[k].first_subidx_block;
					num_subi = p->subdb[k].num_idx_blocks * NUM_SUBINDICES - 
								(NUM_SUBINDICES - 1 - p->subdb[k].last_subidx_block);

					size = num_subi * sizeof(INDEX);
					p->subdb[k].autoload_subindices = (INDEX *)std::malloc(size);
					if (p->subdb[k].autoload_subindices == NULL) {
						(*hdat->log_msg_fn)("Cannot allocate memory for autoload subindices array\n");
						return(1);
					}
					*allocated_bytes += size;

					size = num_subi * sizeof(SUBINDEX_INFO);
					p->subdb[k].autoload_subindex_info = (SUBINDEX_INFO *)std::malloc(size);
					if (p->subdb[k].autoload_subindex_info == NULL) {
						(*hdat->log_msg_fn)("Cannot allocate memory for autoload subindices array\n");
						return(1);
					}
					*allocated_bytes += size;

					/* Zero all subindices up to first_subi. */
					for (subi = 0; subi <= first_subi; ++subi)
						p->subdb[k].autoload_subindices[subi] = 0;

					datap = p->subdb[k].file->file_cache + IDX_BLOCKSIZE * p->subdb[k].first_idx_block;

					index = 0;
					subi = first_subi;
					idxblock = (subi * SUBINDEX_BLOCKSIZE) / IDX_BLOCKSIZE;
					lastvalue = p->subdb[k].start_values[0] & 0x0f;
					value = p->subdb[k].start_values[0] >> 4;
					bitoffset = (p->subdb[k].startbyte * 8);
					p->subdb[k].autoload_subindices[subi] = index;
					p->subdb[k].autoload_subindex_info[subi] = SET_SUBINDEX_INFO(bitoffset % (SUBINDEX_BLOCKSIZE * 8), lastvalue, value);
					mispredict = 0;
					for ( ; subi < num_subi - 1; ) {

						/* Every IDX_BLOCKSIZE we have to check for gaps. */
						diff = IDX_BLOCKSIZE * 8 - bitoffset;
						if (diff < MAX_HUFFCODE_BITS && !mispredict) {
							++subi;
							++idxblock;
							if (subi >= num_subi)
								break;
							bitoffset = 0;
							datap += IDX_BLOCKSIZE;
							lastvalue = p->subdb[k].start_values[idxblock] & 0x0f;
							value = p->subdb[k].start_values[idxblock] >> 4;
							p->subdb[k].autoload_subindices[subi] = index;
							p->subdb[k].autoload_subindex_info[subi] = SET_SUBINDEX_INFO(0, lastvalue, value);
						}
						else {
							/* Check for crossing a minor subindex boundary. */
							if ((subi % NUM_SUBINDICES) != bitoffset / (8 * SUBINDEX_BLOCKSIZE)) {
								++subi;
								if (subi >= num_subi)
									break;
								p->subdb[k].autoload_subindices[subi] = index;
								p->subdb[k].autoload_subindex_info[subi] = SET_SUBINDEX_INFO(bitoffset % (SUBINDEX_BLOCKSIZE * 8), lastvalue, value);
							}
						}

						tablei.bytes[0] = datap[bitoffset / 8];
						tablei.bytes[1] = datap[1 + bitoffset / 8];
						tablei.bytes[2] = datap[2 + bitoffset / 8];
						tablei.word32 >>= (bitoffset % 8);
						code = huffman_decompress_table[temp = (tablei.word32 & 0x3fff)];
						codelength = code & 0x0f;
						runlength = code >> 4;
						mispredict = 0;
						if (runlength == 0) {
							switch (tablei.word32 & ((1 << codelength) - 1)) {
							case ARB_LENGTH16:
								bitoffset += codelength;
								tablei.bytes[0] = datap[bitoffset / 8];
								tablei.bytes[1] = datap[1 + bitoffset / 8];
								tablei.bytes[2] = datap[2 + bitoffset / 8];
								tablei.word32 >>= (bitoffset % 8);
								index += tablei.word32 & 0xffff;
								bitoffset += 16;
								temp = lastvalue;
								lastvalue = value;
								value = temp;
								break;

							case ARB_LENGTH32:
								bitoffset += codelength;
								tablei64.bytes[0] = datap[bitoffset / 8];
								tablei64.bytes[1] = datap[1 + bitoffset / 8];
								tablei64.bytes[2] = datap[2 + bitoffset / 8];
								tablei64.bytes[3] = datap[3 + bitoffset / 8];
								tablei64.bytes[4] = datap[4 + bitoffset / 8];
								tablei64.word64 >>= (bitoffset % 8);
								index += (unsigned int)tablei64.word64 & 0xffffffff;
								bitoffset += 32;
								temp = lastvalue;
								lastvalue = value;
								value = temp;
								break;

							case MISPREDICT_UNKNOWN:
								value = EGDB_UNKNOWN;
								bitoffset += codelength;
								mispredict = 1;
								break;

							case MISPREDICT_WIN:
								value = EGDB_WIN;
								bitoffset += codelength;
								mispredict = 1;
								break;

							case MISPREDICT_LOSS:
								value = EGDB_LOSS;
								bitoffset += codelength;
								mispredict = 1;
								break;

							case MISPREDICT_DRAW:
								if (p->subdb[k].haspartials)
									value = EGDB_DRAW;
								else
									value = mispredict_table[lastvalue][value];
								bitoffset += codelength;
								mispredict = 1;
								break;

							case MISPREDICT_DRAWorLOSS:
								value = EGDB_DRAW_OR_LOSS;
								bitoffset += codelength;
								mispredict = 1;
								break;

							case MISPREDICT_WINorDRAW:
								value = EGDB_WIN_OR_DRAW;
								bitoffset += codelength;
								mispredict = 1;
								break;

							case 0:
								/* We're at the end of a 4k index block. Advance to the next subi. */
								++subi;
								++idxblock;
								if (subi >= num_subi)
									break;
								bitoffset = 0;
								datap += IDX_BLOCKSIZE;
								lastvalue = p->subdb[k].start_values[idxblock] & 0x0f;
								value = p->subdb[k].start_values[idxblock] >> 4;
								p->subdb[k].autoload_subindices[subi] = index;
								p->subdb[k].autoload_subindex_info[subi] = SET_SUBINDEX_INFO(0, lastvalue, value);
								break;
							}
						}
						else {
							index += runlength;
							bitoffset += codelength;
							temp = lastvalue;
							lastvalue = value;
							value = temp;
						}
					}
				}
			}
		}
	}
	return(0);
}


/*
 * We just loaded a block of data into a cacheblock.  Assign the subindices
 * for each subdb that has data in the block.
 */
static void assign_subindices(DBHANDLE *hdat, CPRSUBDB *subdb, CCB *ccbp)
{
	int subi, end_subi, first_blocknum, idx_blocknum;
	unsigned int bitoffset;
	unsigned short value, lastvalue, temp, code, codelength, runlength;
	INDEX index;
	WORD32_BYTES tablei;
	WORD64_BYTES tablei64;

	/* Find the first subdb that has some data in this block.  Use the linked list of 
	 * subdbs in this file.
	 */
	while (subdb->first_idx_block / IDX_BLOCKS_PER_CACHE_BLOCK == ccbp->blocknum && subdb->startbyte > 0)
		subdb = subdb->prev;

	/* For each subdb that has data in this block. */
	do {
		first_blocknum = subdb->first_idx_block / IDX_BLOCKS_PER_CACHE_BLOCK;
		if (first_blocknum == ccbp->blocknum) {
			subi = subdb->first_subidx_block;
			bitoffset = subdb->startbyte * 8;
			index = 0;
			lastvalue = subdb->start_values[0] & 0x0f;
			value = subdb->start_values[0] >> 4;
		}
		else {
			subi = 0;
			bitoffset = 0;
			idx_blocknum = (ccbp->blocknum - subdb->first_idx_block) / IDX_BLOCKS_PER_CACHE_BLOCK;
			index = subdb->indices[idx_blocknum];
			lastvalue = subdb->start_values[idx_blocknum] & 0x0f;
			value = subdb->start_values[idx_blocknum] >> 4;
			ccbp->subindices[subi] = index;
			ccbp->subindex_info[subi] = SET_SUBINDEX_INFO(bitoffset % (SUBINDEX_BLOCKSIZE * 8), lastvalue, value);
		}

		if (subdb->first_idx_block + subdb->num_idx_blocks - 1 > ccbp->blocknum)
			end_subi = NUM_SUBINDICES - 1;
		else
			end_subi = subdb->last_subidx_block;
		for ( ; subi < end_subi; ) {

			/* Check for crossing a minor subindex boundary. */
			if ((subi % NUM_SUBINDICES) != bitoffset / (8 * SUBINDEX_BLOCKSIZE)) {
				++subi;
				if (subi > end_subi)
					break;
				ccbp->subindices[subi] = index;
				ccbp->subindex_info[subi] = SET_SUBINDEX_INFO(bitoffset % (SUBINDEX_BLOCKSIZE * 8), lastvalue, value);
			}

			tablei.bytes[0] = ccbp->data[bitoffset / 8];
			tablei.bytes[1] = ccbp->data[1 + bitoffset / 8];
			tablei.bytes[2] = ccbp->data[2 + bitoffset / 8];
			tablei.word32 >>= (bitoffset % 8);
			code = huffman_decompress_table[temp = (tablei.word32 & 0x3fff)];
			codelength = code & 0x0f;
			runlength = code >> 4;
			if (runlength == 0) {
				switch (tablei.word32 & ((1 << codelength) - 1)) {
				case ARB_LENGTH16:
					bitoffset += codelength;
					tablei.bytes[0] = ccbp->data[bitoffset / 8];
					tablei.bytes[1] = ccbp->data[1 + bitoffset / 8];
					tablei.bytes[2] = ccbp->data[2 + bitoffset / 8];
					tablei.word32 >>= (bitoffset % 8);
					index += tablei.word32 & 0xffff;
					bitoffset += 16;
					temp = lastvalue;
					lastvalue = value;
					value = temp;
					break;

				case ARB_LENGTH32:
					bitoffset += codelength;
					tablei64.bytes[0] = ccbp->data[bitoffset / 8];
					tablei64.bytes[1] = ccbp->data[1 + bitoffset / 8];
					tablei64.bytes[2] = ccbp->data[2 + bitoffset / 8];
					tablei64.bytes[3] = ccbp->data[3 + bitoffset / 8];
					tablei64.bytes[4] = ccbp->data[4 + bitoffset / 8];
					tablei64.word64 >>= (bitoffset % 8);
					index += (unsigned int)tablei64.word64 & 0xffffffff;
					bitoffset += 32;
					temp = lastvalue;
					lastvalue = value;
					value = temp;
					break;

				case MISPREDICT_UNKNOWN:
					value = EGDB_UNKNOWN;
					bitoffset += codelength;
					break;

				case MISPREDICT_WIN:
					value = EGDB_WIN;
					bitoffset += codelength;
					break;

				case MISPREDICT_LOSS:
					value = EGDB_LOSS;
					bitoffset += codelength;
					break;

				case MISPREDICT_DRAW:
					if (subdb->haspartials)
						value = EGDB_DRAW;
					else
						value = mispredict_table[lastvalue][value];
					bitoffset += codelength;
					break;

				case MISPREDICT_DRAWorLOSS:
					value = EGDB_DRAW_OR_LOSS;
					bitoffset += codelength;
					break;

				case MISPREDICT_WINorDRAW:
					value = EGDB_WIN_OR_DRAW;
					bitoffset += codelength;
					break;

				case 0:
					/* We're at the end of a 4k index block. Advance to the next subi. */
					++subi;
					if (subi < end_subi)
						std::printf("Error end of block but not at end_subi.");
					break;
				}
			}
			else {
				index += runlength;
				bitoffset += codelength;
				temp = lastvalue;
				lastvalue = value;
				value = temp;
			}
		}
		subdb = subdb->next;
	}
	while (subdb && (subdb->first_idx_block / IDX_BLOCKS_PER_CACHE_BLOCK == ccbp->blocknum));
}


/*
 * Find a subdb in this file that uses this cache block.
 */
static CPRSUBDB *find_first_subdb(DBHANDLE *hdat, DBFILE *file, int blocknumnum)
{
	int i, k;
	int first_blocknum, last_blocknum;
	DBP *p;

	for (i = 0; i < DBSIZE; ++i) {
		p = hdat->cprsubdatabase + i;
		if (p->subdb != NULL) {
			for (k = 0; k < p->num_subslices; ++k) {
				if (p->subdb[k].file == file) {
					first_blocknum = p->subdb[k].first_idx_block / IDX_BLOCKS_PER_CACHE_BLOCK;
					last_blocknum = (p->subdb[k].first_idx_block + p->subdb[k].num_idx_blocks - 1) / IDX_BLOCKS_PER_CACHE_BLOCK;
					if (blocknumnum >= first_blocknum && blocknumnum <= last_blocknum)
						return(p->subdb + k);
				}
			}
		}
	}
	return(0);
}


/*
 * Open the endgame db driver.
 * pieces is the maximum number of pieces to do lookups for.
 * cache_mb is the amount of ram to use for the driver, in bytes * 1E6.
 * filepath is the path to the database files.
 * msg_fn is a pointer to a function which will log status and error messages.
 * A non-zero return value means some kind of error occurred.  The nature of
 * any errors are communicated through the msg_fn.
 */
static int initdblookup(DBHANDLE *hdat, int pieces, int kings_1side_8pcs, int cache_mb, char const *filepath, void (*msg_fn)(char const*))
{
	int i, j, stat;
	int t0, t1;
	char dbname[MAXFILENAME];
	char msg[MAXMSG];
	int64_t allocated_bytes;			/* keep track of heap allocations in bytes. */
	int64_t autoload_bytes;			/* keep track of autoload allocations in bytes. */
	int cache_mb_avail;
	int max_autoload;
	int64_t total_dbsize;
	int size;
	int count;
	DBFILE *f;
	CPRSUBDB *subdb;
	unsigned char *blockp;		/* Base address of an allocate group of cache buffers. */

	/* Save off some global data. */
	strcpy(hdat->db_filepath, filepath);

	/* Add trailing backslash if needed. */
	size = (int)std::strlen(hdat->db_filepath);
	if (size && (hdat->db_filepath[size - 1] != '/'))
		strcat(hdat->db_filepath, "/");

	hdat->dbpieces = pieces;
	hdat->kings_1side_8pcs = kings_1side_8pcs;
	hdat->log_msg_fn = msg_fn;
	allocated_bytes = 0;
	autoload_bytes = 0;

	std::sprintf(msg, "Available RAM: %dmb\n", get_mem_available_mb());
	(*hdat->log_msg_fn)(msg);

	//init_lock(egdb_lock);
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

	build_mispredict_table();

	/* Parse index files. */
	for (i = 0; i < hdat->numdbfiles; ++i) {

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

	/* Find the total size of all the files that will be used. */
	total_dbsize = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		if (hdat->dbfiles[i].is_present)
			total_dbsize += (int64_t)hdat->dbfiles[i].num_cacheblocks * CACHE_BLOCKSIZE;
	}

#define MIN_AUTOLOAD_RATIO .18
#define MAX_AUTOLOAD_RATIO .35

	/* Calculate how much cache mb to autoload. */
	cache_mb_avail = (int)(cache_mb - allocated_bytes / ONE_MB);
	if (total_dbsize / ONE_MB - cache_mb_avail < 20)
		max_autoload = 1 + (int)(total_dbsize / ONE_MB);		/* Autoload everything. */
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
	build_autoload_list(hdat);
#if USE_AUTOLOAD
	size = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		f = hdat->files_autoload_order[i];
		if (!f || !f->is_present || f->autoload)
			continue;
		size += f->num_cacheblocks;
		if (f->pieces <= MIN_AUTOLOAD_PIECES || (int)((int64_t)((int64_t)size * (int64_t)CACHE_BLOCKSIZE) / ONE_MB) <= max_autoload) {
			f->autoload = 1;
			std::sprintf(msg, "autoload %s\n", f->name);
			(*hdat->log_msg_fn)(msg);
		}
	}
#endif
	/* Open file handles for each db; leave them open for quick access. */
	for (i = 0; i < hdat->numdbfiles; ++i) {

		/* Dont do more pieces than he asked for. */
		if (hdat->dbfiles[i].pieces > pieces)
			break;

		if (!hdat->dbfiles[i].is_present)
			continue;

		std::sprintf(dbname, "%s%s.cpr", hdat->db_filepath, hdat->dbfiles[i].name);
		hdat->dbfiles[i].fp = open_file(dbname);
		if (hdat->dbfiles[i].fp == NULLPTR) {
			std::sprintf(msg, "Cannot open %s\n", dbname);
			(*hdat->log_msg_fn)(msg);
			return(1);
		}

		/* Allocate buffers and read files for autoloaded dbs. */
		if (hdat->dbfiles[i].autoload) {
			size = hdat->dbfiles[i].num_cacheblocks * CACHE_BLOCKSIZE;
			hdat->dbfiles[i].file_cache = (unsigned char *)aligned_large_alloc(size);
			allocated_bytes += size;
			autoload_bytes += size;
			if (hdat->dbfiles[i].file_cache == NULL) {
				(*hdat->log_msg_fn)("Cannot allocate memory for autoload array\n");
				return(1);
			}

			/* Seek to the beginning of the file. */
			if (set_file_pointer(hdat->dbfiles[i].fp, 0)) {
				(*hdat->log_msg_fn)("ERROR: autoload seek failed\n");
				return(-1);
			}

			read_file(hdat->dbfiles[i].fp, hdat->dbfiles[i].file_cache, size, get_page_size());

			/* Close the db file, we are done with it. */
			close_file(hdat->dbfiles[i].fp);
			hdat->dbfiles[i].fp = NULLPTR;

			/* Allocate the subindices. */
			stat = init_autoload_subindices(hdat, hdat->dbfiles + i, &size);
			if (stat)
				return(1);

			allocated_bytes += size;
			autoload_bytes += size;
		}
		else {
			/* These slices are not autoloaded.
			 * Allocate the array of indices into ccbs[].
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
	}

	std::sprintf(msg, "Allocated %dkb for indexing\n", (int)((allocated_bytes - autoload_bytes) / 1024));
	(*hdat->log_msg_fn)(msg);
	std::sprintf(msg, "Allocated %dkb for permanent slice caches\n", (int)(autoload_bytes / 1024));
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
			hdat->cacheblocks = (std::min)(MIN_CACHE_BUF_BYTES / CACHE_BLOCKSIZE, i);
			std::sprintf(msg, "Allocating the minimum %d cache buffers\n",
							hdat->cacheblocks);
			(*hdat->log_msg_fn)(msg);
		}
		else {
			hdat->cacheblocks = (int)(((int64_t)cache_mb * (int64_t)ONE_MB - (int64_t)allocated_bytes) / 
					(int64_t)(CACHE_BLOCKSIZE + sizeof(CCB)));
			hdat->cacheblocks = (std::min)(hdat->cacheblocks, i);
		}

		/* Allocate the CCB array. */
		size = hdat->cacheblocks * sizeof(CCB);
		hdat->ccbs = (CCB *)std::malloc(size);
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

		/* Now preload any of the files that we did not have space for 
		 * in the autoload file table.
		 */
		for (i = 0; i < hdat->numdbfiles && count < hdat->cacheblocks; ++i) {
			f = hdat->files_autoload_order[i];
			if (!f || !f->is_present || f->autoload)
				continue;

			std::sprintf(msg, "preload %s\n", f->name);
			(*hdat->log_msg_fn)(msg);

			for (j = 0; j < f->num_cacheblocks && count < hdat->cacheblocks; ++j) {
				/* It might already be cached. */
				if (f->cache_bufferi[j] == UNDEFINED_BLOCK_ID) {
					subdb = find_first_subdb(hdat, f, j);
					load_blocknum<CCB>(hdat, subdb, j);
					++count;
				}
			}
			if (count >= hdat->cacheblocks)
				break;
		}
		t1 = std::clock();
		std::sprintf(msg, "Read %d buffers in %.0f sec, %.3f msec/buffer\n", 
					hdat->cacheblocks, tdiff_secs(t1, t0), 
					1000.0 * tdiff_secs(t1, t0) / hdat->cacheblocks);
		(*hdat->log_msg_fn)(msg);
	}
	else
		hdat->cacheblocks = 0;

	std::sprintf(msg, "Available RAM: %dmb\n", get_mem_available_mb());
	(*hdat->log_msg_fn)(msg);

	return(0);
}


/*
 * Parse an index file and write all information in cprsubdatabase[].
 * A nonzero return value means some kind of error occurred.
 */
static int parseindexfile(DBHANDLE *hdat, DBFILE *f, int64_t *allocated_bytes)
{
	int stat0, stat;
	char name[MAXFILENAME];
	char msg[MAXMSG];
	FILE *fp;
	char c, colorchar;
	int bm, bk, wm, wk, subslicenum, color;
	int singlevalue, startbyte, v0, v1;
	INDEX indices[IDX_READBUFSIZE];				/* Use temp stack buffer to avoid fragmenting the heap. */
	unsigned char start_values[IDX_READBUFSIZE];
	int i, count;
	INDEX block_index_start;
	int first_idx_block;
	CPRSUBDB *dbpointer, *prev;
	int size;
	int64_t filesize;
	FILE_HANDLE cprfp;
	DBP *dbp;

	/* Open the compressed data file. */
	std::sprintf(name, "%s%s.cpr", hdat->db_filepath, f->name);
	cprfp = open_file(name);
	if (cprfp == NULLPTR) {

		/* We can't find the compressed data file.  Its ok as long as 
		 * this is for more pieces than SAME_PIECES_ONE_FILE pieces.
		 */
		if (f->pieces > SAME_PIECES_ONE_FILE) {
			std::sprintf(msg, "%s not present\n", name);
			(*hdat->log_msg_fn)(msg);
			return(0);
		}
		else {
			std::sprintf(msg, "Cannot open %s\n", name);
			(*hdat->log_msg_fn)(msg);
			return(1);
		}
	}

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

	std::sprintf(name, "%s%s.idx", hdat->db_filepath, f->name);
	fp = std::fopen(name, "r");
	if (fp == 0) {
		std::sprintf(msg, "cannot open index file %s\n", name);
		(*hdat->log_msg_fn)(msg);
		return(1);
	}

	f->is_present = 1;
	prev = 0;
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

		/* Get the rest of the line.  It could be a n/n, or it could just
		 * be a single character that is '+', '=', or '-'.
		 */
		stat = std::fgetc(fp);
		if (std::isdigit(stat)) {
			std::ungetc(stat, fp);
			stat0 = std::fscanf(fp, "%d,%d:%d,%d", &first_idx_block, &startbyte, &v0, &v1);
			if (stat0 < 4) {
				stat = std::fscanf(fp, "%c", &c);
				if (stat < 1) {
					std::sprintf(msg, "Bad line in %s\n", name);
					(*hdat->log_msg_fn)(msg);
					return(1);
				}
			}

            dbpointer->first_idx_block = first_idx_block / IDX_BLOCK_MULT;       /* which block in db. */
			dbpointer->singlevalue = NOT_SINGLEVALUE;
			dbpointer->startbyte = startbyte + (first_idx_block % IDX_BLOCK_MULT) * FILE_IDX_BLOCKSIZE;
			dbpointer->file = f;
			dbpointer->v0 = v0;
			dbpointer->v1 = v1;
			start_values[0] = v0 | (v1 << 4);

			/* Create the doubly linked list of subdbs.  We are excluding the subdbs that
			 * are all one value because we dont create subindices for those.
			 */
			dbpointer->prev = prev;
			dbpointer->next = NULL;
			if (prev)
				prev->next = dbpointer;
			prev = dbpointer;

			/* Assign first_subidx_block and last_subidx_block. 
			 * This takes care of every subdb, except the very last subdb will not have
			 * its last_subidx_block field set.  Assign that one when we are done with
			 * this index file.
			 */
			dbpointer->first_subidx_block = dbpointer->startbyte / SUBINDEX_BLOCKSIZE;
			if (dbpointer->prev) {
				if ((dbpointer->startbyte % SUBINDEX_BLOCKSIZE) == 0) {
					if (dbpointer->first_subidx_block > 0)
						dbpointer->prev->last_subidx_block = dbpointer->first_subidx_block - 1;
					else
						dbpointer->prev->last_subidx_block = NUM_SUBINDICES - 1;
				}
				else
					dbpointer->prev->last_subidx_block = dbpointer->first_subidx_block;
			}

			/* Check for one or more info lines beginning with a '#'. */
			while (1) {
				stat = std::fgetc(fp);
				if (std::isspace(stat))
					continue;
				if (stat == '#') {
					char line[120];

					std::fgets(line, sizeof(line), fp);
					if (std::strstr(line, "haspartials"))
						dbpointer->haspartials = 1;
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
			indices[0] = 0;		/* first block is index 0. */
			while (std::fscanf(fp, "%d:%d,%d", &block_index_start, &v0, &v1) == 3) {
				indices[i] = block_index_start;
				start_values[i] = v0 | (v1 << 4);
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

					dbpointer->start_values = (unsigned char *)std::realloc(dbpointer->start_values, (count + IDX_READBUFSIZE) * sizeof(dbpointer->start_values[0]));
					if (dbpointer->start_values == NULL) {
						(*hdat->log_msg_fn)("std::malloc error for start_values array!\n");
						return(1);
					}
					std::memcpy(dbpointer->start_values + count, start_values, IDX_READBUFSIZE * sizeof(dbpointer->start_values[0]));
					count += IDX_READBUFSIZE;
					i = 0;
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

				dbpointer->start_values = (unsigned char *)std::realloc(dbpointer->start_values, (count + i) * sizeof(dbpointer->start_values[0]));
				if (dbpointer->start_values == NULL) {
					(*hdat->log_msg_fn)("std::malloc error for start_values array!\n");
					return(1);
				}
				std::memcpy(dbpointer->start_values + count, start_values, i * sizeof(dbpointer->start_values[0]));
			}
			count += i;
			dbpointer->num_idx_blocks = count;
			size = count * sizeof(dbpointer->indices[0]);
			*allocated_bytes += ROUND_UP(size, MALLOC_ALIGNSIZE);
			size = count * sizeof(dbpointer->start_values[0]);
			*allocated_bytes += ROUND_UP(size, MALLOC_ALIGNSIZE);
		}
		else {
			switch (stat) {
			case '.':
				singlevalue = EGDB_UNKNOWN;
				break;
			case '+':
				singlevalue = EGDB_WIN;
				break;
			case '=':
				singlevalue = EGDB_DRAW;
				break;
			case '-':
				singlevalue = EGDB_LOSS;
				break;
			default:
				std::sprintf(msg, "Bad singlevalue line in %s\n", name);
				(*hdat->log_msg_fn)(msg);
				return(1);
			}
			dbpointer->first_idx_block = 0;
			dbpointer->indices = NULL;
			dbpointer->start_values = NULL;
			dbpointer->num_idx_blocks = 0;
			dbpointer->singlevalue = singlevalue;
			dbpointer->file = f;
		}
	}
	std::fclose(fp);

	if (prev) {
		prev->last_subidx_block = (unsigned char)(((LOWORD32(filesize) - 1) % IDX_BLOCKSIZE) / SUBINDEX_BLOCKSIZE);
	
		/* Check the total number of index blocks in this database. */
		if (f->num_idx_blocks != count + prev->first_idx_block) {
			std::sprintf(msg, "count of index blocks dont match: %d %d\n",
					f->num_idx_blocks, count + prev->first_idx_block);
			(*hdat->log_msg_fn)(msg);
		}
		std::sprintf(msg, "%10d index blocks: %s\n", count + prev->first_idx_block, name);
		(*hdat->log_msg_fn)(msg);
	}

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

						if (hdat->kings_1side_8pcs >= 0 && npieces == 8)
							if (nbk > hdat->kings_1side_8pcs || nwk > hdat->kings_1side_8pcs)
								continue;
						std::sprintf(hdat->dbfiles[count].name, "db%d-%d%d%d%d",
									npieces, nbm, nbk, nwm, nwk);
						hdat->dbfiles[count].pieces = npieces;
						hdat->dbfiles[count].max_pieces_1side = nbm + nbk;
						++count;
					}
				}
			}
		}
	}
	hdat->numdbfiles = count;
}


static void build_autoload_list(DBHANDLE *hdat)
{
	int i, count;
	int npieces, nk, nbm, nbk, nwm, nwk;
	DBP *dbp;
	DBFILE *f;

	/* Build the the autoload ordered list. */
	count = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		f = hdat->dbfiles + i;
		if (f->pieces <= MIN_AUTOLOAD_PIECES)  {
			hdat->files_autoload_order[count] = f;
			++count;
		}
	}

	for (nk = 0; nk <= hdat->dbpieces; ++nk) {
		for (npieces = MIN_AUTOLOAD_PIECES + 1; npieces <= hdat->dbpieces; ++npieces) {
			for (nbk = 0; nbk <= nk && nbk <= npieces; ++nbk) {
				nwk = nk - nbk;
				for (nbm = 0; nbm <= npieces - nbk - nwk; ++nbm) {
					nwm = npieces - nbk - nwk - nbm;
					if (nbm + nbk == 0 || nwm + nwk == 0)
						continue;
					if (nwm + nwk > nbm + nbk)
						continue;
					if (nbm + nbk == nwm + nwk && nwk > nbk)
						continue;
					if (nbm + nbk > MAXPIECE)
						continue;
					dbp = hdat->cprsubdatabase + DBOFFSET(nbm, nbk, nwm, nwk, EGDB_BLACK);
					if (!dbp || !dbp->subdb)
						continue;
					f = dbp->subdb->file;
					if (!f->is_present)
						continue;
					hdat->files_autoload_order[count] = f;
					++count;
				}
			}
		}
	}
}

namespace detail {

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

		if (hdat->dbfiles[i].file_cache) {
			virtual_free(hdat->dbfiles[i].file_cache);
			hdat->dbfiles[i].file_cache = 0;
		}
		else {
			std::free(hdat->dbfiles[i].cache_bufferi);
			hdat->dbfiles[i].cache_bufferi = 0;
		}

		if (hdat->dbfiles[i].fp != NULLPTR)
			close_file(hdat->dbfiles[i].fp);

		hdat->dbfiles[i].num_idx_blocks = 0;
		hdat->dbfiles[i].num_cacheblocks = 0;
		hdat->dbfiles[i].fp = NULLPTR;
	}
	std::memset(hdat->dbfiles, 0, sizeof(hdat->dbfiles));

	for (i = 0; i < DBSIZE; ++i) {
		p = hdat->cprsubdatabase + i;
		if (p->subdb != NULL) {
			for (k = 0; k < p->num_subslices; ++k) {
				if (p->subdb[k].indices)
					std::free(p->subdb[k].indices);
				if (p->subdb[k].start_values)
					std::free(p->subdb[k].start_values);
				if (p->subdb[k].autoload_subindices)
					std::free(p->subdb[k].autoload_subindices);
				if (p->subdb[k].autoload_subindex_info)
					std::free(p->subdb[k].autoload_subindex_info);
			}
			std::free(p->subdb);
		}
	}
	std::free(hdat->cprsubdatabase);
	std::free(hdat);
	std::free(handle);
	return(0);
}


static int verify_crc(EGDB_DRIVER const *handle, void (*msg_fn)(char const*), int *abort, EGDB_VERIFY_MSGS *msgs)
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


static int get_pieces(EGDB_DRIVER const *handle, int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side)
{
	int i;
	DBFILE *f;
	DBP *p;
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;

	*max_pieces = 0;
	*max_pieces_1side = 0;
	*max_9pc_kings = 0;
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
	if (*max_pieces >= 8) {
		p = hdat->cprsubdatabase + DBOFFSET(0, 5, 3, 0, EGDB_BLACK);
		if (p && p->subdb != NULL)
			*max_8pc_kings_1side = 5;
		else {
			p = hdat->cprsubdatabase + DBOFFSET(0, 4, 4, 0, EGDB_BLACK);
			if (p && p->subdb != NULL)
				*max_8pc_kings_1side = 4;
			else {
				p = hdat->cprsubdatabase + DBOFFSET(1, 3, 4, 0, EGDB_BLACK);
				if (p && p->subdb != NULL)
					*max_8pc_kings_1side = 3;
				else {
					p = hdat->cprsubdatabase + DBOFFSET(2, 2, 4, 0, EGDB_BLACK);
					if (p && p->subdb != NULL)
						*max_8pc_kings_1side = 2;
					else {
						p = hdat->cprsubdatabase + DBOFFSET(3, 1, 4, 0, EGDB_BLACK);
						if (p && p->subdb != NULL)
							*max_8pc_kings_1side = 1;
						else
							*max_8pc_kings_1side = 0;
					}
				}
			}
		}
	}
	return(0);
}

}	// namespace detail

EGDB_DRIVER *egdb_open_wld_huff(int pieces, int kings_1side_8pcs,
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
	status = initdblookup((DBHANDLE *)handle->internal_data, pieces, kings_1side_8pcs, cache_mb, directory, msg_fn);
	if (status) {
		std::free(handle->internal_data);
		std::free(handle);
		return(0);
	}

	handle->lookup = dblookup;
	
	handle->get_stats = detail::get_db_stats;
	handle->reset_stats = detail::reset_db_stats;
	handle->verify = detail::verify_crc;
	handle->close = detail::egdb_close;
	handle->get_pieces = detail::get_pieces;
	return(handle);
}

}	// namespace egdb_interface
