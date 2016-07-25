// builddb
#include "compression_tables.h"
#include "indexing.h"
#include "tunstall_decompress.h"
#include "tunstall_decompress_v2.txt"

// egdb
#include "crc.h"
#include "egdb_common.h"
#include "egdb_intl.h"

// engine
#include "bicoef.h"
#include "bitcount.h"
#include "board.h"
#include "bool.h"
#include "lock.h"
#include "project.h"
#include "reverse.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <utility>

#define MAXPIECES 9

#define LOG_HITS 0

#define SAME_PIECES_ONE_FILE 5
#define MIN_AUTOLOAD_PIECES 5

#define NUM_SUBINDICES 64
#define IDX_BLOCKSIZE 4096
#define CACHE_BLOCKSIZE IDX_BLOCKSIZE
#define SUBINDEX_BLOCKSIZE (IDX_BLOCKSIZE / NUM_SUBINDICES)

#define MAXFILENAME MAX_PATH
#define MAXFILES 200		/* This is enough for an 8/9pc database. */

/* Having types with the same name as types in other files confuses the debugger. */
#define DBFILE DBFILE_TUN_V2
#define CPRSUBDB CPRSUBDB_TUN_V2
#define DBP DBP_TUN_V2
#define CCB CCB_TUN_V2
#define DBHANDLE DBHANDLE_TUN_V2

typedef struct {
	char is_present;
	char pieces;			/* number of pieces in this db file. */
	char max_pieces_1side;
	char autoload;			/* statically load the whole file if true. */
	char name[20];			/* db filename prefix. */
	int num_cacheblocks;	/* number of cache blocks in this db file. */
	unsigned char *file_cache;/* if not null the whole db file is here. */
	HANDLE fp;
	int *cache_bufferi;		/* An array of indices into cache_buffers[], indexed by block number. */
#if LOG_HITS
	int hits;
#endif
} DBFILE;

// definition of a structure for compressed databases
typedef struct CPRSUBDB {
	char singlevalue;				/* WIN/LOSS/DRAW if single value, else NOT_SINGLEVALUE. */
	unsigned char first_subidx_block;	/* modulus NUM_SUBINDICES */
	unsigned char single_subidx_block;	/* true if only one subidx block in this subdb. */
	unsigned char last_subidx_block;	/* modulus NUM_SUBINDICES */
	short int startbyte;			/* offset into first index block of first data. */
	int num_idx_blocks;				/* number of index blocks in this subdb. */
	int first_idx_block;			/* index block containing the first data value. */
	INDEX *indices;					/* array of first index number in each index block */
									/* allocated dynamically, to size [num_idx_blocks] */
	char *catalogidx;				/* array of catalog indices, one for each block. */
	unsigned char *vmap;			/* array of vmap descriptors, one for each block. */
	INDEX *autoload_subindices;		/* subindices for autoloaded files. */
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
} CCB;

typedef struct {
	EGDB_TYPE db_type;
	char db_filepath[MAXFILENAME];	/* Path to database files. */
	int dbpieces;
	int kings_1side_8pcs;			/* limit, >= 0 */
	int cacheblocks;				/* Total number of db cache blocks. */
	void (*log_msg_fn)(char *);		/* for status and error messages. */
	DBP *cprsubdatabase;
	CCB *ccbs;
	int ccbs_top;		/* index into ccbs[] of least recently used block. */
	int numdbfiles;
	DBFILE dbfiles[MAXFILES];
	DBFILE *files_autoload_order[MAXFILES];
	EGDB_STATS lookup_stats;
} DBHANDLE;

typedef struct {
	char *filename;
	unsigned int crc;
} DBCRC;

static LOCKT egdb_lock;

/* A table of crc values for each database file. */
static DBCRC dbcrc[] = {
	{"db2.cpr1", 0x0319ba8c},
	{"db2.idx1", 0x07a9f0f3},
	{"db3.cpr1", 0x098b476b},
	{"db3.idx1", 0x8e96b77d},
	{"db4.cpr1", 0x08b0249a},
	{"db4.idx1", 0xc3a84295},
	{"db5.cpr1", 0x3cda0517},
	{"db5.idx1", 0xc5912d8f},
	{"db6-0303.cpr1", 0x0857bd2e},
	{"db6-0303.idx1", 0x66a77b5e},
	{"db6-0312.cpr1", 0x4a7f099c},
	{"db6-0312.idx1", 0x2a6a803a},
	{"db6-0321.cpr1", 0x4a75f62a},
	{"db6-0321.idx1", 0xff3099f2},
	{"db6-0330.cpr1", 0x6d77cb4e},
	{"db6-0330.idx1", 0x17e6a321},
	{"db6-0402.cpr1", 0x7b9fdcbb},
	{"db6-0402.idx1", 0xc35bb8dd},
	{"db6-0411.cpr1", 0xaec0e214},
	{"db6-0411.idx1", 0x0901c9f3},
	{"db6-0420.cpr1", 0xfe951e16},
	{"db6-0420.idx1", 0x34d576d2},
	{"db6-0501.cpr1", 0xedbe6c56},
	{"db6-0501.idx1", 0xa13a3c37},
	{"db6-0510.cpr1", 0xadb50394},
	{"db6-0510.idx1", 0x4e29b458},
	{"db6-1212.cpr1", 0x5167eef6},
	{"db6-1212.idx1", 0x7e75ccbf},
	{"db6-1221.cpr1", 0x1f74f152},
	{"db6-1221.idx1", 0x3eb4b297},
	{"db6-1230.cpr1", 0xc3a0f6af},
	{"db6-1230.idx1", 0xe2b14f1c},
	{"db6-1302.cpr1", 0xe35159b9},
	{"db6-1302.idx1", 0x007d2e9a},
	{"db6-1311.cpr1", 0x11905cee},
	{"db6-1311.idx1", 0x2d9a34b4},
	{"db6-1320.cpr1", 0xf0a72015},
	{"db6-1320.idx1", 0x0491fc2d},
	{"db6-1401.cpr1", 0x09f88b44},
	{"db6-1401.idx1", 0x412ec755},
	{"db6-1410.cpr1", 0x3d6936d6},
	{"db6-1410.idx1", 0xbb08a038},
	{"db6-2121.cpr1", 0xf146e765},
	{"db6-2121.idx1", 0x684a1d48},
	{"db6-2130.cpr1", 0xb3eb7f42},
	{"db6-2130.idx1", 0x14d7317a},
	{"db6-2202.cpr1", 0x694cac31},
	{"db6-2202.idx1", 0xc9283792},
	{"db6-2211.cpr1", 0x1ad66915},
	{"db6-2211.idx1", 0x0c4de703},
	{"db6-2220.cpr1", 0xeb9ebd8f},
	{"db6-2220.idx1", 0xb1d6290c},
	{"db6-2301.cpr1", 0x5306ff65},
	{"db6-2301.idx1", 0x00d6ab62},
	{"db6-2310.cpr1", 0x448164de},
	{"db6-2310.idx1", 0xf0ccc1d5},
	{"db6-3030.cpr1", 0x5f3edd36},
	{"db6-3030.idx1", 0xc07467f2},
	{"db6-3102.cpr1", 0x82a1136a},
	{"db6-3102.idx1", 0xb4a44365},
	{"db6-3111.cpr1", 0x983163ba},
	{"db6-3111.idx1", 0x00382c88},
	{"db6-3120.cpr1", 0x01fe8f1f},
	{"db6-3120.idx1", 0x4685b1ea},
	{"db6-3201.cpr1", 0x50fef962},
	{"db6-3201.idx1", 0x5986a829},
	{"db6-3210.cpr1", 0x5a67ceeb},
	{"db6-3210.idx1", 0xe9099a66},
	{"db6-4002.cpr1", 0x8b84d19f},
	{"db6-4002.idx1", 0x3966bbcb},
	{"db6-4011.cpr1", 0x9d26720f},
	{"db6-4011.idx1", 0xc411faa6},
	{"db6-4020.cpr1", 0xd990979e},
	{"db6-4020.idx1", 0xfd0dc2b0},
	{"db6-4101.cpr1", 0x393fadf8},
	{"db6-4101.idx1", 0xa73e5449},
	{"db6-4110.cpr1", 0xdc0dc90a},
	{"db6-4110.idx1", 0x34b8cf3d},
	{"db6-5001.cpr1", 0x70de9ad7},
	{"db6-5001.idx1", 0x606b81c8},
	{"db6-5010.cpr1", 0x81f0431e},
	{"db6-5010.idx1", 0xd3903db1},
	{"db7-0403.cpr1", 0x6490dcd5},
	{"db7-0403.idx1", 0x936ac9e7},
	{"db7-0412.cpr1", 0x9d91aecd},
	{"db7-0412.idx1", 0xb13c5392},
	{"db7-0421.cpr1", 0xddef8aed},
	{"db7-0421.idx1", 0xe525ebb2},
	{"db7-0430.cpr1", 0x3b0d2f76},
	{"db7-0430.idx1", 0x67c07ed6},
	{"db7-0502.cpr1", 0xe20fcc6f},
	{"db7-0502.idx1", 0x091b332d},
	{"db7-0511.cpr1", 0xefb61781},
	{"db7-0511.idx1", 0xef8433f0},
	{"db7-0520.cpr1", 0xe6354566},
	{"db7-0520.idx1", 0xe03d817b},
	{"db7-1303.cpr1", 0x1194b92b},
	{"db7-1303.idx1", 0x2c9ca438},
	{"db7-1312.cpr1", 0x519863c9},
	{"db7-1312.idx1", 0x6a9f746d},
	{"db7-1321.cpr1", 0x8202a690},
	{"db7-1321.idx1", 0xdabd8495},
	{"db7-1330.cpr1", 0x5c11d800},
	{"db7-1330.idx1", 0x842b6b0e},
	{"db7-1402.cpr1", 0x26805d83},
	{"db7-1402.idx1", 0xcb3df439},
	{"db7-1411.cpr1", 0xb9727c39},
	{"db7-1411.idx1", 0x1cf1781c},
	{"db7-1420.cpr1", 0x5db5e2a0},
	{"db7-1420.idx1", 0x6f70a5be},
	{"db7-2203.cpr1", 0x5d342ad6},
	{"db7-2203.idx1", 0xe4e2a875},
	{"db7-2212.cpr1", 0xf5a2de0e},
	{"db7-2212.idx1", 0x49746884},
	{"db7-2221.cpr1", 0xbe1f8a2f},
	{"db7-2221.idx1", 0x5c752883},
	{"db7-2230.cpr1", 0xe03b5461},
	{"db7-2230.idx1", 0x7928fd22},
	{"db7-2302.cpr1", 0x039f1d89},
	{"db7-2302.idx1", 0xe8c4ecc7},
	{"db7-2311.cpr1", 0xa5479159},
	{"db7-2311.idx1", 0xec4d45ae},
	{"db7-2320.cpr1", 0x88a237b7},
	{"db7-2320.idx1", 0x6d17c8d6},
	{"db7-3103.cpr1", 0xe0aea818},
	{"db7-3103.idx1", 0x96c8172a},
	{"db7-3112.cpr1", 0x36b72231},
	{"db7-3112.idx1", 0x22051b9b},
	{"db7-3121.cpr1", 0x792cc98a},
	{"db7-3121.idx1", 0xc58c5536},
	{"db7-3130.cpr1", 0x1abec03e},
	{"db7-3130.idx1", 0xaba2d131},
	{"db7-3202.cpr1", 0xdaf7e3cc},
	{"db7-3202.idx1", 0xbd780c13},
	{"db7-3211.cpr1", 0x4a178090},
	{"db7-3211.idx1", 0x9aed03d3},
	{"db7-3220.cpr1", 0x983ef7ea},
	{"db7-3220.idx1", 0x75085f01},
	{"db7-4003.cpr1", 0xba6744ac},
	{"db7-4003.idx1", 0x128a3ea8},
	{"db7-4012.cpr1", 0xe54fe0a8},
	{"db7-4012.idx1", 0xca47a22c},
	{"db7-4021.cpr1", 0x4fa5ceae},
	{"db7-4021.idx1", 0x4b66052a},
	{"db7-4030.cpr1", 0x09c4ad17},
	{"db7-4030.idx1", 0x713fa989},
	{"db7-4102.cpr1", 0x8452277e},
	{"db7-4102.idx1", 0x16ec5c2a},
	{"db7-4111.cpr1", 0xc4b218bc},
	{"db7-4111.idx1", 0xb983eeb2},
	{"db7-4120.cpr1", 0xc7f0014a},
	{"db7-4120.idx1", 0x74982208},
	{"db7-5002.cpr1", 0x378d643b},
	{"db7-5002.idx1", 0xd2b2a94d},
	{"db7-5011.cpr1", 0x3092666c},
	{"db7-5011.idx1", 0x2c2c4468},
	{"db7-5020.cpr1", 0xc417c27c},
	{"db7-5020.idx1", 0xe371262f},
	{"db8-0404.cpr1", 0x24091667},
	{"db8-0404.idx1", 0xf5df83a8},
	{"db8-0413.cpr1", 0x1f4a764e},
	{"db8-0413.idx1", 0x2c3b9d60},
	{"db8-0422.cpr1", 0xdc8ee67c},
	{"db8-0422.idx1", 0x3b18a6eb},
	{"db8-0431.cpr1", 0xae9288a0},
	{"db8-0431.idx1", 0x56d8f330},
	{"db8-0440.cpr1", 0xcba207d9},
	{"db8-0440.idx1", 0xb87babfa},
	{"db8-0503.cpr1", 0xaf6e3578},
	{"db8-0503.idx1", 0x767e70a0},
	{"db8-0512.cpr1", 0x338aa41d},
	{"db8-0512.idx1", 0xf2fe3ae3},
	{"db8-0521.cpr1", 0x5bac9752},
	{"db8-0521.idx1", 0x90489139},
	{"db8-0530.cpr1", 0x028d8b48},
	{"db8-0530.idx1", 0x6e003f95},
	{"db8-1313.cpr1", 0x67824eec},
	{"db8-1313.idx1", 0x66eeefd6},
	{"db8-1322.cpr1", 0xe29ac258},
	{"db8-1322.idx1", 0x8b584314},
	{"db8-1331.cpr1", 0xdeace47d},
	{"db8-1331.idx1", 0x5e64bf94},
	{"db8-1340.cpr1", 0x1f0c78de},
	{"db8-1340.idx1", 0x92298302},
	{"db8-1403.cpr1", 0x2831b04c},
	{"db8-1403.idx1", 0xca687e77},
	{"db8-1412.cpr1", 0xff0af29f},
	{"db8-1412.idx1", 0x2aa36189},
	{"db8-1421.cpr1", 0xe0cd1dd5},
	{"db8-1421.idx1", 0xfb44b277},
	{"db8-1430.cpr1", 0xf0b44ee1},
	{"db8-1430.idx1", 0x11927875},
	{"db8-2222.cpr1", 0xd5aa5e8d},
	{"db8-2222.idx1", 0x9fdbcc6a},
	{"db8-2231.cpr1", 0xa5dc86c9},
	{"db8-2231.idx1", 0xf416c102},
	{"db8-2240.cpr1", 0xd560e888},
	{"db8-2240.idx1", 0xbf7195fa},
	{"db8-2303.cpr1", 0x079a0824},
	{"db8-2303.idx1", 0x047002c7},
	{"db8-2312.cpr1", 0xaf1b6654},
	{"db8-2312.idx1", 0x03b839fc},
	{"db8-2321.cpr1", 0x727a54ac},
	{"db8-2321.idx1", 0x8c9290dd},
	{"db8-2330.cpr1", 0xb18612ce},
	{"db8-2330.idx1", 0xed688d9b},
	{"db8-3131.cpr1", 0x80dc0dba},
	{"db8-3131.idx1", 0xdd82650d},
	{"db8-3140.cpr1", 0x572350aa},
	{"db8-3140.idx1", 0x56888a53},
	{"db8-3203.cpr1", 0xa06cd537},
	{"db8-3203.idx1", 0x8fa52097},
	{"db8-3212.cpr1", 0x96ba5e8e},
	{"db8-3212.idx1", 0xb57c8dbb},
	{"db8-3221.cpr1", 0xb9bfa419},
	{"db8-3221.idx1", 0x1fd8a747},
	{"db8-3230.cpr1", 0xe729a7a8},
	{"db8-3230.idx1", 0x30d5f0e8},
	{"db8-4040.cpr1", 0x43c27c60},
	{"db8-4040.idx1", 0x40993827},
	{"db8-4103.cpr1", 0x80c54d8f},
	{"db8-4103.idx1", 0xaa2975b2},
	{"db8-4112.cpr1", 0x61623a9c},
	{"db8-4112.idx1", 0x0a424ba2},
	{"db8-4121.cpr1", 0xc99e014e},
	{"db8-4121.idx1", 0x5b96e417},
	{"db8-4130.cpr1", 0x3695990f},
	{"db8-4130.idx1", 0x01069b39},
	{"db8-5003.cpr1", 0x45d4cdae},
	{"db8-5003.idx1", 0x503ca6a8},
	{"db8-5012.cpr1", 0xe5744e8b},
	{"db8-5012.idx1", 0x5b3012e2},
	{"db8-5021.cpr1", 0x57d4339b},
	{"db8-5021.idx1", 0xb78b6053},
	{"db8-5030.cpr1", 0xfded52ba},
	{"db8-5030.idx1", 0x49649ace},
	{"db9-5040.cpr1", 0xa1391053},
	{"db9-5040.idx1", 0xef847fc5},
};


/* Function prototypes. */
int parseindexfile(DBHANDLE *, DBFILE *, int64 *allocated_bytes);
void build_file_table(DBHANDLE *hdat);
void build_autoload_list(DBHANDLE *hdat);
void assign_subindices(DBHANDLE *hdat, CPRSUBDB *subdb, CCB *ccbp);


static char virtual_to_real[256][4];

void init_virtual_to_real(void)
{
	int vr0, vr1, vr2, vr3, index;

	for (vr0 = 0; vr0 < 4; ++vr0) {
		for (vr1 = 0; vr1 < 4; ++vr1) {
			if (vr1 == vr0)
				continue;
			for (vr2 = 0; vr2 < 4; ++vr2) {
				if (vr2 == vr1 || vr2 == vr0)
					continue;
				for (vr3 = 0; vr3 < 4; ++vr3) {
					if (vr3 == vr0 || vr3 == vr1 || vr3 == vr2)
						continue;
					index = vr0 + vr1 * 4 + vr2 * 16 + vr3 * 64;
					virtual_to_real[index][0] = vr0;
					virtual_to_real[index][1] = vr1;
					virtual_to_real[index][2] = vr2;
					virtual_to_real[index][3] = vr3;
				}
			}
		}
	}
}


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


static EGDB_STATS *get_db_stats(EGDB_DRIVER *handle)
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
			hitrate = 10000.0 * (double)f->hits / ((double)count * (double)f->num_cacheblocks);
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


static void read_blocknum_from_file(DBHANDLE *hdat, CCB *ccb)
{
	int64 filepos;
	int stat;

	filepos = (int64)ccb->blocknum * CACHE_BLOCKSIZE;

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
static int dblookup(EGDB_DRIVER *handle, EGDB_POSITION *p, int color, int cl)
{
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
	uint32 index;
	unsigned short *runlength;
	unsigned short value_runs_offset;
	int64 index64;
	int bm, bk, wm, wk;
	int i, subslicenum;
	int idx_blocknum, subidx_blocknum;	
	int blocknum;
	int returnvalue, virtual_value;
	unsigned char *diskblock;
	EGDB_POSITION revpos;
	INDEX n_idx;
	INDEX *indices;
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
		return(color == EGDB_BLACK ? EGDB_LOSS : EGDB_WIN);
	}
	if ((wm + wk) == 0) {
		++hdat->lookup_stats.db_returns;
		return(color == EGDB_WHITE ? EGDB_LOSS : EGDB_WIN);
	}

	if ((bm + wm + wk + bk > MAXPIECES) || (bm + bk > MAXPIECE) || (wm + wk > MAXPIECE)) {
		++hdat->lookup_stats.db_not_present_requests;
		return EGDB_SUBDB_UNAVAILABLE;
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

	subslicenum = (int)(index64 / (int64)MAX_SUBSLICE_INDICES);
	index = (uint32)(index64 - (int64)subslicenum * (int64)MAX_SUBSLICE_INDICES);

	/* get pointer to db. */
	dbp = hdat->cprsubdatabase + DBOFFSET(bm, bk, wm, wk, color);
	dbpointer = dbp->subdb;

	/* check presence. */
	if (dbpointer == 0) {
		++hdat->lookup_stats.db_not_present_requests;

		/* Determine if both side-to-move colors are unavailable. */
		dbp = hdat->cprsubdatabase + DBOFFSET(bm, bk, wm, wk, OTHER_COLOR(color));
		if (dbp->subdb)
			return(EGDB_UNKNOWN);
		else
			return(EGDB_SUBDB_UNAVAILABLE);
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
									dbpointer->num_idx_blocks * NUM_SUBINDICES - (NUM_SUBINDICES - 1 - dbpointer->last_subidx_block),
									indices, index);
		blocknum = subidx_blocknum / NUM_SUBINDICES + dbpointer->first_idx_block;
		diskblock = dbpointer->file->file_cache + (subidx_blocknum * (size_t)SUBINDEX_BLOCKSIZE) +
					dbpointer->first_idx_block * (size_t)IDX_BLOCKSIZE;
		n_idx = indices[subidx_blocknum];
		i = 0;
		if (subidx_blocknum == dbpointer->first_subidx_block)
			i = dbpointer->startbyte - subidx_blocknum * SUBINDEX_BLOCKSIZE;
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
		blocknum = dbpointer->first_idx_block + idx_blocknum;

		/* Is this block already cached? */
		take_lock(egdb_lock);
		ccbi = dbpointer->file->cache_bufferi[blocknum];
		if (ccbi != UNDEFINED_BLOCK_ID) {

			/* Already cached.  Update the lru list. */
			ccbp = update_lru<DBHANDLE, DBFILE, CCB>(hdat, dbpointer->file, ccbi);
		}
		else {

			/* we must load it.
			 * if the lookup was a "conditional lookup", we don't load the block.
			 */
			if (cl) {
				release_lock(egdb_lock);
				return(EGDB_NOT_IN_CACHE);
			}

			/* If necessary load this block from disk, update lru list. */
			ccbp = load_blocknum<DBHANDLE, CPRSUBDB, CCB>(hdat, dbpointer, blocknum);
		}

		/* Do a binary search to find the exact subindex.  This is complicated a bit by the
		 * problem that there may be a boundary between the end of one subdb and the start of
		 * the next in this block.  For any subindex block that contains one of these
		 * boundaries, the subindex stored is the ending block (0 is implied for the
		 * starting block).  Therefore the binary search cannot use the first subindex of
		 * a subdb.  We check for this separately.
		 */
		indices = ccbp->subindices;
		if (idx_blocknum == 0 && (dbpointer->single_subidx_block ||
						dbpointer->first_subidx_block == NUM_SUBINDICES - 1 ||
						indices[dbpointer->first_subidx_block + 1] > index)) {
			subidx_blocknum = dbpointer->first_subidx_block;
			n_idx = 0;
			i = dbpointer->startbyte - subidx_blocknum * SUBINDEX_BLOCKSIZE;
		}
		else {
			int first, last;
			if (idx_blocknum == 0)
				first = dbpointer->first_subidx_block + 1;
			else
				first = 0;
			if (idx_blocknum == dbpointer->num_idx_blocks - 1)
				last = dbpointer->last_subidx_block + 1;
			else
				last = NUM_SUBINDICES;
			subidx_blocknum = find_block(first, last, indices, index);
			n_idx = indices[subidx_blocknum];
			i = 0;
		}
		diskblock = ccbp->data + subidx_blocknum * SUBINDEX_BLOCKSIZE;
		release_lock(egdb_lock);
	}

	/* The subindex block we were looking for is now pointed to by diskblock.
	 * Do a linear search for the byte within the block.
	 */
	runlength = decompress_catalog_v2[dbpointer->catalogidx[blocknum - dbpointer->first_idx_block]].runlength_table;
	while (n_idx <= index) {
		n_idx += runlength[diskblock[i]];
		i++;
	}

	/* once we are out here, n>index
	 * we overshot again, move back:
	 */
	i--;
	n_idx -= runlength[diskblock[i]];

	/* Do some simple error checking. */
	if (i < 0 || i >= SUBINDEX_BLOCKSIZE) {
		char msg[MAXMSG];

		std::sprintf(msg, "db block array index outside block bounds: %d\nFile %s\n",
					 i, dbpointer->file->name);
		(*hdat->log_msg_fn)(msg);
		return EGDB_UNKNOWN;
	}

	/* finally, we have found the byte which describes the position we
	 * wish to look up. it is diskblock[i].
	 */
	value_runs_offset = decompress_catalog_v2[dbpointer->catalogidx[blocknum - dbpointer->first_idx_block]].value_runs[diskblock[i]];
	while (1) {
		n_idx += value_runs_v2[value_runs_offset + 1] + (value_runs_v2[value_runs_offset + 2] << 8);
		if (n_idx > index)
			break;
		value_runs_offset += 3;
	}
	virtual_value = value_runs_v2[value_runs_offset];
	returnvalue = virtual_to_real[dbpointer->vmap[blocknum - dbpointer->first_idx_block]][virtual_value];
	++hdat->lookup_stats.db_returns;

	return(returnvalue);
}


static void preload_subdb(DBHANDLE *hdat, CPRSUBDB *subdb, int *preloaded_cacheblocks)
{
	int i;
	int first_blocknum;
	int last_blocknum;

	if (subdb->singlevalue != NOT_SINGLEVALUE)
		return;

	first_blocknum = subdb->first_idx_block;
	last_blocknum = subdb->first_idx_block + subdb->num_idx_blocks - 1;
	for (i = first_blocknum; i <= last_blocknum && *preloaded_cacheblocks < hdat->cacheblocks; ++i) {
		if (subdb->file->cache_bufferi[i] == UNDEFINED_BLOCK_ID) {
			load_blocknum<DBHANDLE, CPRSUBDB, CCB>(hdat, subdb, i);
			++(*preloaded_cacheblocks);
		}
	}
}


static int init_autoload_subindices(DBHANDLE *hdat, DBFILE *file, size_t *allocated_bytes)
{
	int i, k, m, size;
	int first_subi, num_subi, subi, blocknum;
	DBP *p;
	INDEX index;
	unsigned char *datap;
	unsigned short *runlen_table;

	*allocated_bytes = 0;
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

					/* Zero all subindices up to first_subi. */
					for (subi = 0; subi <= first_subi; ++subi)
						p->subdb[k].autoload_subindices[subi] = 0;

					datap = p->subdb[k].file->file_cache + IDX_BLOCKSIZE * (size_t)p->subdb[k].first_idx_block;

					index = 0;
					subi = first_subi;
					blocknum = p->subdb[k].startbyte / CACHE_BLOCKSIZE;
					runlen_table = decompress_catalog_v2[p->subdb[k].catalogidx[blocknum]].runlength_table;
					for (m = p->subdb[k].startbyte; subi < num_subi; ++m) {

						if ((m % SUBINDEX_BLOCKSIZE) == 0) {
							subi = m / SUBINDEX_BLOCKSIZE;
							if (subi >= num_subi)
								break;
							p->subdb[k].autoload_subindices[subi] = index;

							blocknum = m / CACHE_BLOCKSIZE;
							runlen_table = decompress_catalog_v2[p->subdb[k].catalogidx[blocknum]].runlength_table;
						}
						index += runlen_table[datap[m]];
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
	unsigned int m;
	INDEX index;
	unsigned short *runlen_table;

	/* Find the first subdb that has some data in this block.  Use the linked list of 
	 * subdbs in this file.
	 */
	while (subdb->first_idx_block == ccbp->blocknum && subdb->startbyte > 0)
		subdb = subdb->prev;

	/* For each subdb that has data in this block. */
	do {
		first_blocknum = subdb->first_idx_block;
		if (first_blocknum == ccbp->blocknum) {
			subi = subdb->first_subidx_block;
			m = subdb->startbyte;
			index = 0;
			runlen_table = decompress_catalog_v2[subdb->catalogidx[0]].runlength_table;
		}
		else {
			subi = 0;
			m = 0;
			idx_blocknum = ccbp->blocknum - subdb->first_idx_block;
			runlen_table = decompress_catalog_v2[subdb->catalogidx[idx_blocknum]].runlength_table;
			index = subdb->indices[idx_blocknum];
		}

		if (subdb->first_idx_block + subdb->num_idx_blocks - 1 > ccbp->blocknum)
			end_subi = NUM_SUBINDICES - 1;
		else
			end_subi = subdb->last_subidx_block;

		for ( ; ; ++m) {
			if ((m % SUBINDEX_BLOCKSIZE) == 0) {
				subi = m / SUBINDEX_BLOCKSIZE;
				if (subi > end_subi)
					break;
				ccbp->subindices[subi] = index;
			}
			index += runlen_table[ccbp->data[m]];

		}
		subdb = subdb->next;
	}
	while (subdb && (subdb->first_idx_block == ccbp->blocknum));
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
					first_blocknum = p->subdb[k].first_idx_block;
					last_blocknum = p->subdb[k].first_idx_block + p->subdb[k].num_idx_blocks - 1;
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
static int initdblookup(DBHANDLE *hdat, int pieces, int kings_1side_8pcs, int cache_mb, char *filepath, void (*msg_fn)(char *))
{
	int i, j, stat;
	int t0, t1, t2, t3;
	char dbname[MAXFILENAME];
	char msg[MAXMSG];
	int64 allocated_bytes;		/* keep track of heap allocations in bytes. */
	int64 autoload_bytes;			/* keep track of autoload allocations in bytes. */
	int cache_mb_avail;
	int max_autoload;
	int64 total_dbsize;
	size_t size;
	int count;
	DBFILE *f;
	CPRSUBDB *subdb;
	unsigned char *blockp;		/* Base address of an allocate group of cache buffers. */

	t0 = GetTickCount();

	/* Save off some global data. */
	strcpy(hdat->db_filepath, filepath);

	/* Add trailing backslash if needed. */
	size = (int)strlen(hdat->db_filepath);
	if (size && (hdat->db_filepath[size - 1] != '\\'))
		strcat(hdat->db_filepath, "\\");

	hdat->dbpieces = pieces;
	hdat->kings_1side_8pcs = kings_1side_8pcs;
	hdat->log_msg_fn = msg_fn;
	allocated_bytes = 0;
	autoload_bytes = 0;

	std::sprintf(msg, "Available RAM: %dmb\n", get_mem_available_mb());
	(*hdat->log_msg_fn)(msg);

	init_lock(egdb_lock);
	init_bitcount();

	/* initialize binomial coefficients. */
	initbicoef();

	/* initialize runlength and lookup array. */
	init_compression_tables();
	init_virtual_to_real();

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

		/* Dont do more pieces than he asked for. */
		if (hdat->dbfiles[i].pieces > pieces)
			break;

		stat = parseindexfile(hdat, hdat->dbfiles + i, &allocated_bytes);

		/* Check for errors from parseindexfile. */
		if (stat)
			return(1);
	}

	/* End of reading index files, start of autoload. */
	t1 = GetTickCount();
	std::sprintf(msg, "Reading index files took %d secs\n", (t1 - t0) / 1000);
	(*hdat->log_msg_fn)(msg);

	/* Find the total size of all the files that will be used. */
	total_dbsize = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		if (hdat->dbfiles[i].is_present)
			total_dbsize += (int64)hdat->dbfiles[i].num_cacheblocks * CACHE_BLOCKSIZE;
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

	size = 0;

	for (i = 0; i < hdat->numdbfiles; ++i) {
		f = hdat->files_autoload_order[i];
		if (!f || !f->is_present || f->autoload)
			continue;

		size += f->num_cacheblocks;
		if (f->pieces <= MIN_AUTOLOAD_PIECES || (int)((int64)((int64)size * (int64)CACHE_BLOCKSIZE) / ONE_MB) <= max_autoload) {
			f->autoload = 1;
			std::sprintf(msg, "autoload %s\n", f->name);
			(*hdat->log_msg_fn)(msg);
		}
	}

	/* Open file handles for each db; leave them open for quick access. */
	for (i = 0; i < hdat->numdbfiles; ++i) {

		/* Dont do more pieces than he asked for. */
		if (hdat->dbfiles[i].pieces > pieces)
			break;

		if (!hdat->dbfiles[i].is_present)
			continue;

		std::sprintf(dbname, "%s%s.cpr1", hdat->db_filepath, hdat->dbfiles[i].name);
		hdat->dbfiles[i].fp = CreateFile((LPCSTR)dbname, GENERIC_READ, FILE_SHARE_READ,
						NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_NO_BUFFERING,
						NULL);
		if (hdat->dbfiles[i].fp == INVALID_HANDLE_VALUE) {
			std::sprintf(msg, "Cannot open %s\n", dbname);
			(*hdat->log_msg_fn)(msg);
			return(1);
		}

		/* Allocate buffers and read files for autoloaded dbs. */
		if (hdat->dbfiles[i].autoload) {
			size = hdat->dbfiles[i].num_cacheblocks * (size_t)CACHE_BLOCKSIZE;
			hdat->dbfiles[i].file_cache = (unsigned char *)aligned_large_alloc(size);
			allocated_bytes += size;
			autoload_bytes += size;
			if (hdat->dbfiles[i].file_cache == NULL) {
				(*hdat->log_msg_fn)("Cannot allocate memory for autoload array\n");
				return(1);
			}

			/* Seek to the beginning of the file. */
			if (set_file_pointer(hdat->dbfiles[i].fp, 0)) {
				(*hdat->log_msg_fn)("autoload seek failed\n");
				return(-1);
			}

			read_file(hdat->dbfiles[i].fp, hdat->dbfiles[i].file_cache, size, get_pagesize());

			/* Close the db file, we are done with it. */
			CloseHandle(hdat->dbfiles[i].fp);
			hdat->dbfiles[i].fp = INVALID_HANDLE_VALUE;

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

	t2 = GetTickCount();
	std::sprintf(msg, "Autoload took %d secs\n", (t2 - t1) / 1000);
	(*hdat->log_msg_fn)(msg);

	/* Figure out how much ram is left for lru cache buffers.
	 * If we autoloaded everything then no need for lru cache buffers.
	 */
	i = needed_cache_buffers<DBHANDLE>(hdat);
	if (i > 0) {
		if ((allocated_bytes + MIN_CACHE_BUF_BYTES) / ONE_MB >= cache_mb) {

			/* We need more memory than he gave us, allocate 10mb of cache
			 * buffers if we can use that many.
			 */
			hdat->cacheblocks = min(MIN_CACHE_BUF_BYTES / CACHE_BLOCKSIZE, i);
			std::sprintf(msg, "Allocating the minimum %d cache buffers\n",
							hdat->cacheblocks);
			(*hdat->log_msg_fn)(msg);
		}
		else {
			hdat->cacheblocks = (int)(((int64)cache_mb * (int64)ONE_MB - (int64)allocated_bytes) / 
					(int64)(CACHE_BLOCKSIZE + sizeof(CCB)));
			hdat->cacheblocks = min(hdat->cacheblocks, i);
		}

		/* Allocate the CCB array. */
		size = hdat->cacheblocks * sizeof(CCB);
		hdat->ccbs = (CCB *)std::malloc(size);
		if (!hdat->ccbs) {
			(*hdat->log_msg_fn)("Cannot allocate memory for ccbs\n");
			return(1);
		}

		std::memset(hdat->ccbs, 0, size);

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
			count = min(CACHE_ALLOC_COUNT, hdat->cacheblocks - i);
			size = count * CACHE_BLOCKSIZE * sizeof(unsigned char);
			blockp = (unsigned char *)aligned_large_alloc(size);
			if (blockp == NULL) {
				i = GetLastError();
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
					load_blocknum<DBHANDLE, CPRSUBDB, CCB>(hdat, subdb, j);
					++count;
				}
			}
			if (count >= hdat->cacheblocks)
				break;
		}
		t3 = GetTickCount();
		std::sprintf(msg, "Read %d buffers in %d sec, %.3f msec/buffer\n", 
					hdat->cacheblocks, (t3 - t2) / 1000, 
					(double)(t3 - t2) / (double)hdat->cacheblocks);
		(*hdat->log_msg_fn)(msg);
		std::sprintf(msg, "Egdb init took %d sec total\n", (t3 - t0) / 1000);
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
static int parseindexfile(DBHANDLE *hdat, DBFILE *f, int64 *allocated_bytes)
{
	int stat0, stat;
	char name[MAXFILENAME];
	char msg[MAXMSG];
	FILE *fp;
	char c, colorchar;
	int bm, bk, wm, wk, subslicenum, color;
	int singlevalue, startbyte, ncatalog, nvmap;
	INDEX indices[IDX_READBUFSIZE];				/* Use temp stack buffer to avoid fragmenting the heap. */
	unsigned char vmap[IDX_READBUFSIZE];
	unsigned char catalogidx[IDX_READBUFSIZE];
	int i, count;
	INDEX block_index_start;
	int first_idx_block;
	CPRSUBDB *dbpointer, *prev;
	int size;
	int64 filesize;
	HANDLE cprfp;
	DBP *dbp;

	/* Open the compressed data file. */
	std::sprintf(name, "%s%s.cpr1", hdat->db_filepath, f->name);
	cprfp = CreateFile((LPCSTR)name, GENERIC_READ, FILE_SHARE_READ,
					NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
	if (cprfp == INVALID_HANDLE_VALUE) {

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
	f->num_cacheblocks = (int)(filesize / IDX_BLOCKSIZE);
	if (filesize % IDX_BLOCKSIZE)
		++f->num_cacheblocks;

	/* Close the file. */
	if (!CloseHandle(cprfp)) {
		std::sprintf(msg, "Error from CloseHandle\n");
		(*hdat->log_msg_fn)(msg);
		return(1);
	}

	std::sprintf(name, "%s%s.idx1", hdat->db_filepath, f->name);
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

		/* Get the rest of the line.  It could be a n/n,n,n or it could just
		 * be a single character that is '+', '=', or '-'.
		 */
		stat = std::fgetc(fp);
		if (std::isdigit(stat)) {
			std::ungetc(stat, fp);
			stat0 = std::fscanf(fp, "%d/%d,%d,%d\n", &first_idx_block, &startbyte, &ncatalog, &nvmap);
			if (stat0 < 4) {
				stat = std::fscanf(fp, "%c\n", &c);
				if (stat < 1) {
					std::sprintf(msg, "Bad line in %s\n", name);
					(*hdat->log_msg_fn)(msg);
					return(1);
				}
			}

            dbpointer->first_idx_block = first_idx_block;       /* which block in db. */
			dbpointer->singlevalue = NOT_SINGLEVALUE;
			dbpointer->startbyte = startbyte;
			dbpointer->file = f;

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

				/* Check for special case of only 1 subidx block. */
				if (dbpointer->prev->num_idx_blocks == 1) {
					if (dbpointer->prev->first_subidx_block == dbpointer->prev->last_subidx_block)
						dbpointer->prev->single_subidx_block = 1;
				}
			}

			/* We got the first line, maybe there are more.
			 * Assign the array of first indices of each block.
			 */
			count = 0;
			i = 1;
			indices[0] = 0;		/* first block is index 0. */
			catalogidx[0] = ncatalog;
			vmap[0] = nvmap;
			while (std::fscanf(fp, "%d,%d,%d\n", &block_index_start, &ncatalog, &nvmap) == 3) {
				indices[i] = block_index_start;
				catalogidx[i] = ncatalog;
				vmap[i] = nvmap;
				i++;

				/* If the local buffers are full then allocate more space
				 * and copy.
				 */
				if (i == IDX_READBUFSIZE) {
					dbpointer->indices = (INDEX *)std::realloc(dbpointer->indices, (count + IDX_READBUFSIZE) * sizeof(dbpointer->indices[0]));
					if (dbpointer->indices == NULL) {
						(*hdat->log_msg_fn)("std::malloc error for indices array!\n");
						return(1);
					}
					std::memcpy(dbpointer->indices + count, indices, IDX_READBUFSIZE * sizeof(dbpointer->indices[0]));

					dbpointer->catalogidx = (char *)std::realloc(dbpointer->catalogidx, (count + IDX_READBUFSIZE) * sizeof(dbpointer->catalogidx[0]));
					if (dbpointer->catalogidx == NULL) {
						(*hdat->log_msg_fn)("std::malloc error for catalogidx array!\n");
						return(1);
					}
					std::memcpy(dbpointer->catalogidx + count, catalogidx, IDX_READBUFSIZE * sizeof(dbpointer->catalogidx[0]));

					dbpointer->vmap = (unsigned char *)std::realloc(dbpointer->vmap, (count + IDX_READBUFSIZE) * sizeof(dbpointer->vmap[0]));
					if (dbpointer->vmap == NULL) {
						(*hdat->log_msg_fn)("std::malloc error for vmap array!\n");
						return(1);
					}
					std::memcpy(dbpointer->vmap + count, vmap, IDX_READBUFSIZE * sizeof(dbpointer->vmap[0]));

					i = 0;
					count += IDX_READBUFSIZE;
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

				dbpointer->catalogidx = (char *)std::realloc(dbpointer->catalogidx, (count + i) * sizeof(dbpointer->catalogidx[0]));
				if (dbpointer->catalogidx == NULL) {
					(*hdat->log_msg_fn)("std::malloc error for catalogidx array!\n");
					return(1);
				}
				std::memcpy(dbpointer->catalogidx + count, catalogidx, i * sizeof(dbpointer->catalogidx[0]));

				dbpointer->vmap = (unsigned char *)std::realloc(dbpointer->vmap, (count + i) * sizeof(dbpointer->vmap[0]));
				if (dbpointer->vmap == NULL) {
					(*hdat->log_msg_fn)("std::malloc error for vmap array!\n");
					return(1);
				}
				std::memcpy(dbpointer->vmap + count, vmap, i * sizeof(dbpointer->vmap[0]));
			}
			count += i;
			dbpointer->num_idx_blocks = count;
			size = count * (sizeof(dbpointer->indices[0]) + sizeof(dbpointer->catalogidx[0]) + sizeof(dbpointer->vmap[0]));
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
			dbpointer->num_idx_blocks = 0;
			dbpointer->singlevalue = singlevalue;
			dbpointer->file = f;
		}
	}
	std::fclose(fp);

	if (prev) {
		prev->last_subidx_block = (unsigned char)(((LOWORD32(filesize) - 1) % IDX_BLOCKSIZE) / SUBINDEX_BLOCKSIZE);
	
		/* Check the total number of index blocks in this database. */
		if (f->num_cacheblocks != count + prev->first_idx_block) {
			std::sprintf(msg, "count of index blocks dont match: %d %d\n",
					f->num_cacheblocks, count + prev->first_idx_block);
			(*hdat->log_msg_fn)(msg);
		}
	}
	std::sprintf(msg, "%10d index blocks: %s\n", count + prev->first_idx_block, name);
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
			hdat->dbfiles[count].max_pieces_1side = min(npieces - 1, MAXPIECE);
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
	assert(hdat->numdbfiles <= MAXFILES);
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
					if (!dbp)
						continue;

					if (!dbp->subdb) {
						dbp = hdat->cprsubdatabase + DBOFFSET(nbm, nbk, nwm, nwk, EGDB_WHITE);
						if (!dbp || !dbp->subdb)
							continue;
					}

					f = dbp->subdb->file;
					if (!f->is_present)
						continue;

					hdat->files_autoload_order[count] = f;
					++count;
				}
			}
		}
	}
	assert(count <= hdat->numdbfiles);
}


static int egdb_close(EGDB_DRIVER *handle)
{
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;
	int i, k;
	DBP *p;

	/* Free the cache buffers in groups of CACHE_ALLOC_COUNT at a time. */
	if (hdat->ccbs) {
		for (i = 0; i < hdat->cacheblocks; i += CACHE_ALLOC_COUNT) {
			if (hdat->ccbs[i].data)
				VirtualFree(hdat->ccbs[i].data, 0, MEM_RELEASE);
		}

		/* Free the cache control blocks. */
		free(hdat->ccbs);
	}

	for (i = 0; i < sizeof(hdat->dbfiles) / sizeof(hdat->dbfiles[0]); ++i) {
		if (hdat->dbfiles[i].pieces > hdat->dbpieces)
			continue;

		if (!hdat->dbfiles[i].is_present)
			continue;

		if (hdat->dbfiles[i].file_cache) {
			VirtualFree(hdat->dbfiles[i].file_cache, 0, MEM_RELEASE);
			hdat->dbfiles[i].file_cache = 0;
		}
		else {
			if (hdat->dbfiles[i].cache_bufferi) {
				free(hdat->dbfiles[i].cache_bufferi);
				hdat->dbfiles[i].cache_bufferi = 0;
			}
		}

		if (hdat->dbfiles[i].fp != INVALID_HANDLE_VALUE)
			CloseHandle(hdat->dbfiles[i].fp);

		hdat->dbfiles[i].num_cacheblocks = 0;
		hdat->dbfiles[i].fp = INVALID_HANDLE_VALUE;
	}
	std::memset(hdat->dbfiles, 0, sizeof(hdat->dbfiles));

	for (i = 0; i < DBSIZE; ++i) {
		p = hdat->cprsubdatabase + i;
		if (p->subdb != NULL) {
			for (k = 0; k < p->num_subslices; ++k) {
				if (p->subdb[k].indices)
					free(p->subdb[k].indices);
				if (p->subdb[k].vmap)
					free(p->subdb[k].vmap);
				if (p->subdb[k].catalogidx)
					free(p->subdb[k].catalogidx);
				if (p->subdb[k].autoload_subindices)
					free(p->subdb[k].autoload_subindices);
			}
			free(p->subdb);
		}
	}
	free(hdat->cprsubdatabase);
	free(hdat);
	free(handle);
	return(0);
}


static int verify_crc(EGDB_DRIVER *handle, void (*msg_fn)(char *), int *abort, EGDB_VERIFY_MSGS *msgs)
{
	int i;
	int error_count;
	unsigned int crc;
	DBCRC *pcrc;
	FILE *fp;
	char filename[MAXFILENAME];
	char msg[MAXMSG];
	DBHANDLE *hdat = (DBHANDLE *)handle->internal_data;

	error_count = 0;
	for (i = 0; i < hdat->numdbfiles; ++i) {
		if (hdat->dbfiles[i].pieces > hdat->dbpieces)
			continue;

		/* Build the index filename. */
		std::sprintf(filename, "%s.idx1", hdat->dbfiles[i].name);
		pcrc = find_file_crc(filename, dbcrc, ARRAY_SIZE(dbcrc));
		if (!pcrc)
			continue;

		std::sprintf(filename, "%s%s.idx1", hdat->db_filepath, hdat->dbfiles[i].name);
		fp = std::fopen(filename, "rb");
		if (!fp)
			continue;

		std::sprintf(msg, "%s  ", filename);
		(*msg_fn)(msg);

		crc = file_crc_calc(fp, abort);
		if (crc != pcrc->crc) {
			std::sprintf(msg, "%s\n", msgs->crc_failed);
			(*msg_fn)(msg);
			++error_count;
		}
		else {
			std::sprintf(msg, "%s\n", msgs->ok);
			(*msg_fn)(msg);
		}
		std::fclose(fp);

		/* Build the data filename. */
		std::sprintf(filename, "%s.cpr1", hdat->dbfiles[i].name);
		pcrc = find_file_crc(filename, dbcrc, ARRAY_SIZE(dbcrc));
		if (!pcrc)
			continue;

		std::sprintf(filename, "%s%s.cpr1", hdat->db_filepath, hdat->dbfiles[i].name);
		fp = std::fopen(filename, "rb");
		if (!fp)
			continue;

		std::sprintf(msg, "%s  ", filename);
		(*msg_fn)(msg);

		crc = file_crc_calc(fp, abort);
		if (crc != pcrc->crc) {
			std::sprintf(msg, "%s\n", msgs->crc_failed);
			(*msg_fn)(msg);
			++error_count;
		}
		else {
			std::sprintf(msg, "%s\n", msgs->ok);
			(*msg_fn)(msg);
		}
		std::fclose(fp);
	}
	if (error_count) {
		std::sprintf(msg, "%d %s\n", error_count, msgs->errors);
		(*msg_fn)(msg);
	}
	else {
		std::sprintf(msg, "%s\n", msgs->no_errors);
		(*msg_fn)(msg);
	}
	return(error_count);
}


static int get_pieces(EGDB_DRIVER *handle, int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side)
{
	int i;
	DBFILE *f;
	DBP *p, *p1;
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
		/* If more than 1 king total, we have to test both black and white, because 
		 * we only have data for 1 side, and it could be either black or white.
		 */
		p = hdat->cprsubdatabase + DBOFFSET(0, 5, 3, 0, EGDB_BLACK);
		p1 = hdat->cprsubdatabase + DBOFFSET(0, 5, 3, 0, EGDB_WHITE);
		if ((p && p->subdb != NULL) || (p1 && p1->subdb != NULL))
			*max_8pc_kings_1side = 5;
		else {
			p = hdat->cprsubdatabase + DBOFFSET(0, 4, 4, 0, EGDB_BLACK);
			p1 = hdat->cprsubdatabase + DBOFFSET(0, 4, 4, 0, EGDB_WHITE);
			if ((p && p->subdb != NULL) || (p1 && p1->subdb != NULL))
				*max_8pc_kings_1side = 4;
			else {
				p = hdat->cprsubdatabase + DBOFFSET(1, 3, 4, 0, EGDB_BLACK);
				p1 = hdat->cprsubdatabase + DBOFFSET(1, 3, 4, 0, EGDB_WHITE);
				if ((p && p->subdb != NULL) || (p1 && p1->subdb != NULL))
					*max_8pc_kings_1side = 3;
				else {
					p = hdat->cprsubdatabase + DBOFFSET(2, 2, 4, 0, EGDB_BLACK);
					p1 = hdat->cprsubdatabase + DBOFFSET(2, 2, 4, 0, EGDB_WHITE);
					if ((p && p->subdb != NULL) || (p1 && p1->subdb != NULL))
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


EGDB_DRIVER *egdb_open_wld_tun_v2(int pieces, int kings_1side_8pcs,
				int cache_mb, char *directory, void (*msg_fn)(char *), EGDB_TYPE db_type)
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
		free(handle);
		(*msg_fn)("Cannot allocate memory for driver handle.\n");
		return(0);
	}
	((DBHANDLE *)(handle->internal_data))->db_type = db_type;
	status = initdblookup((DBHANDLE *)handle->internal_data, pieces, kings_1side_8pcs, cache_mb, directory, msg_fn);
	if (status) {
		egdb_close(handle);
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



