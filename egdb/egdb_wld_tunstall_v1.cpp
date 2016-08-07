#include "builddb/compression_tables.h"
#include "builddb/indexing.h"
#include "builddb/tunstall_decompress.h"
#include "egdb/crc.h"
#include "egdb/egdb_common.h"
#include "egdb/egdb_intl.h"
#include "egdb/platform.h"
#include "egdb/tunstall_decompress_v1.txt"
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
#include <utility>

#define MAXPIECES 8

#define LOG_HITS 0

#define SAME_PIECES_ONE_FILE 4
#define MIN_AUTOLOAD_PIECES 4

#define NUM_SUBINDICES 64
#define IDX_BLOCK_MULT 4
#define FILE_IDX_BLOCKSIZE 1024
#define IDX_BLOCKSIZE (FILE_IDX_BLOCKSIZE * IDX_BLOCK_MULT)
#define CACHE_BLOCKSIZE (IDX_BLOCKSIZE)
#define SUBINDEX_BLOCKSIZE (IDX_BLOCKSIZE / NUM_SUBINDICES)

#define MAXFILES 200		/* This is enough for an 8/9pc database. */

/* Having types with the same name as types in other files confuses the debugger. */
#define DBFILE DBFILE_TUN_V1
#define CPRSUBDB CPRSUBDB_TUN_V1
#define DBP DBP_TUN_V1
#define CCB CCB_TUN_V1
#define DBHANDLE DBHANDLE_TUN_V1

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
#if LOG_HITS
	int hits;
#endif
} DBFILE;


// definition of a structure for compressed databases
typedef struct CPRSUBDB {
	char singlevalue;				/* WIN/LOSS/DRAW if single value, else NOT_SINGLEVALUE. */
	char catalog_entry;				/* index into decompress catalog. */
	char vmap[6];					/* virtual to real value map. */
	unsigned char first_subidx_block;	/* modulus NUM_SUBINDICES */
	unsigned char single_subidx_block;	/* true if only one subidx block in this subdb. */
	unsigned char last_subidx_block;	/* modulus NUM_SUBINDICES */
	short int startbyte;			/* offset into first index block of first data. */
	int num_idx_blocks;				/* number of index blocks in this subdb. */
	int first_idx_block;			/* index block containing the first data value. */
	INDEX *indices;					/* array of first index number in each index block */
									/* allocated dynamically, to size [num_idx_blocks] */
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

typedef struct cache_ht_node {
	unsigned short filenum;
	unsigned int blocknum;		/* The cache block number within the database file. */
	int cacheblock_index;		/* Index of the cache block, or UNDEFINED_BLOCK_ID if this block is unused. */
	struct cache_ht_node *previous;
	struct cache_ht_node *next;
} CACHE_HASHTABLE_NODE;

/* cache control block type. */
typedef struct {
	int next;				/* index of next node */
	int prev;				/* index of previous node */
	unsigned char *data;	/* data of this block. */
	CACHE_HASHTABLE_NODE *cache_ht_node;	/* node containing the data for this block, or NULL. */
	int cache_ht_index;		/* index into cache_ht of this block, or UNDEFINED_BLOCK_ID. */
	INDEX subindices[NUM_SUBINDICES];
} CCB;

typedef struct {
	EGDB_TYPE db_type;
	char db_filepath[MAXFILENAME];	/* Path to database files. */
	int dbpieces;
	int kings_1side_8pcs;			/* limit, >= 0 */
	int cacheblocks;				/* Total number of db cache blocks. */
	int cache_ht_size;				/* Number of cache hashtable nodes. */
	CACHE_HASHTABLE_NODE *cache_ht_nodes_base;
	CACHE_HASHTABLE_NODE **cache_ht;
	CACHE_HASHTABLE_NODE *cache_ht_free_list;
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

STATIC_DEFINE_LOCK(egdb_lock);

/* A table of crc values for each database file. */
static DBCRC dbcrc[] = {
	{"db2.cpr", 0xc8d8bd1b},
	{"db2.idx", 0x1b731f71},
	{"db3.cpr", 0xa851b439},
	{"db3.idx", 0x85aade3a},
	{"db4.cpr", 0x72a76eab},
	{"db4.idx", 0x66389130},
	{"db5-0302.cpr", 0x7c4320c3},
	{"db5-0302.idx", 0x1dc7d0dd},
	{"db5-0311.cpr", 0xcbc9771e},
	{"db5-0311.idx", 0x1cbecb4f},
	{"db5-0320.cpr", 0x459d077d},
	{"db5-0320.idx", 0xcd1c0ac4},
	{"db5-0401.cpr", 0xcbc5d458},
	{"db5-0401.idx", 0xcfb8c3d6},
	{"db5-0410.cpr", 0x78dccccb},
	{"db5-0410.idx", 0x3c24e626},
	{"db5-1202.cpr", 0x64634caa},
	{"db5-1202.idx", 0xadc595ab},
	{"db5-1211.cpr", 0x4986d7e0},
	{"db5-1211.idx", 0xc8366e17},
	{"db5-1220.cpr", 0x75a7805a},
	{"db5-1220.idx", 0x0233b28e},
	{"db5-1301.cpr", 0x30ece849},
	{"db5-1301.idx", 0x6922ee27},
	{"db5-1310.cpr", 0xd79080d3},
	{"db5-1310.idx", 0xfb1e3bd6},
	{"db5-2102.cpr", 0xb275501a},
	{"db5-2102.idx", 0x488da6a5},
	{"db5-2111.cpr", 0xb42a2adc},
	{"db5-2111.idx", 0xc2821c39},
	{"db5-2120.cpr", 0xebca7abf},
	{"db5-2120.idx", 0xae73b105},
	{"db5-2201.cpr", 0xad304e52},
	{"db5-2201.idx", 0x1f2cfc55},
	{"db5-2210.cpr", 0x83a01c75},
	{"db5-2210.idx", 0x738f8f3d},
	{"db5-3002.cpr", 0xd935e924},
	{"db5-3002.idx", 0xe1b95b9b},
	{"db5-3011.cpr", 0x5aa94fe0},
	{"db5-3011.idx", 0xa4230c3e},
	{"db5-3020.cpr", 0x67aadf7f},
	{"db5-3020.idx", 0xc008c727},
	{"db5-3101.cpr", 0x6fc7920c},
	{"db5-3101.idx", 0xa22adee0},
	{"db5-3110.cpr", 0x930c0b16},
	{"db5-3110.idx", 0x10b7e8c6},
	{"db5-4001.cpr", 0x8bbc15c2},
	{"db5-4001.idx", 0x8d65f3f8},
	{"db5-4010.cpr", 0xc80cd147},
	{"db5-4010.idx", 0xb64c312f},
	{"db6-0303.cpr", 0x0fc029a5},
	{"db6-0303.idx", 0xe85030ba},
	{"db6-0312.cpr", 0x1925ce3f},
	{"db6-0312.idx", 0xedbb960e},
	{"db6-0321.cpr", 0x5960f2db},
	{"db6-0321.idx", 0x559d4284},
	{"db6-0330.cpr", 0x9648fad3},
	{"db6-0330.idx", 0x949dcf5a},
	{"db6-0402.cpr", 0xc5f70eb5},
	{"db6-0402.idx", 0xd8048f13},
	{"db6-0411.cpr", 0x78e6ff59},
	{"db6-0411.idx", 0x19ad8466},
	{"db6-0420.cpr", 0x00575849},
	{"db6-0420.idx", 0x1d0b7454},
	{"db6-0501.cpr", 0xa0f3197b},
	{"db6-0501.idx", 0x2fe53972},
	{"db6-0510.cpr", 0xadb50394},
	{"db6-0510.idx", 0xf52844d9},
	{"db6-1212.cpr", 0x7abca427},
	{"db6-1212.idx", 0x090d06ca},
	{"db6-1221.cpr", 0x98415829},
	{"db6-1221.idx", 0x442bc0b6},
	{"db6-1230.cpr", 0x4d990da7},
	{"db6-1230.idx", 0xc97dc9d9},
	{"db6-1302.cpr", 0xc21f37b3},
	{"db6-1302.idx", 0xc019f41b},
	{"db6-1311.cpr", 0x07512ee2},
	{"db6-1311.idx", 0xe5e5469c},
	{"db6-1320.cpr", 0x33e3bc11},
	{"db6-1320.idx", 0x0c81de48},
	{"db6-1401.cpr", 0x5af4de47},
	{"db6-1401.idx", 0xa92e02fc},
	{"db6-1410.cpr", 0xbdf10497},
	{"db6-1410.idx", 0xb5be593f},
	{"db6-2121.cpr", 0xd79711a6},
	{"db6-2121.idx", 0xa9a02f1a},
	{"db6-2130.cpr", 0x0b9a9223},
	{"db6-2130.idx", 0xa20716e2},
	{"db6-2202.cpr", 0x248e0913},
	{"db6-2202.idx", 0x43d6bb49},
	{"db6-2211.cpr", 0x9c3cd253},
	{"db6-2211.idx", 0xbba49ed0},
	{"db6-2220.cpr", 0xf2db68a5},
	{"db6-2220.idx", 0x79bfbc36},
	{"db6-2301.cpr", 0x7aaa314b},
	{"db6-2301.idx", 0x96a1552d},
	{"db6-2310.cpr", 0x73ff4af9},
	{"db6-2310.idx", 0x277fde7f},
	{"db6-3030.cpr", 0x3ebcc072},
	{"db6-3030.idx", 0xf3693c6c},
	{"db6-3102.cpr", 0x5cc48ad3},
	{"db6-3102.idx", 0x6c22f7ca},
	{"db6-3111.cpr", 0x2da6c08d},
	{"db6-3111.idx", 0x1c120d26},
	{"db6-3120.cpr", 0x680317e9},
	{"db6-3120.idx", 0xe2b5019b},
	{"db6-3201.cpr", 0xb2e041ef},
	{"db6-3201.idx", 0xa78860b7},
	{"db6-3210.cpr", 0xffc350da},
	{"db6-3210.idx", 0x3968a835},
	{"db6-4002.cpr", 0x592c4f4b},
	{"db6-4002.idx", 0x5ae3db38},
	{"db6-4011.cpr", 0x17ccb465},
	{"db6-4011.idx", 0x861cb053},
	{"db6-4020.cpr", 0xe3940bd0},
	{"db6-4020.idx", 0x31425e9a},
	{"db6-4101.cpr", 0x9eb2ed6d},
	{"db6-4101.idx", 0xe0a80352},
	{"db6-4110.cpr", 0xcb5ec8bf},
	{"db6-4110.idx", 0xf8b7b92d},
	{"db6-5001.cpr", 0x56bca471},
	{"db6-5001.idx", 0x58a3a6fc},
	{"db6-5010.cpr", 0xd4b6e583},
	{"db6-5010.idx", 0x5dad3611},
	{"db7-0403.cpr", 0x92a25c62},
	{"db7-0403.idx", 0x5f74e464},
	{"db7-0412.cpr", 0x63ae302a},
	{"db7-0412.idx", 0x8ee7bdc9},
	{"db7-0421.cpr", 0x09589967},
	{"db7-0421.idx", 0x0443086e},
	{"db7-0430.cpr", 0x0821231c},
	{"db7-0430.idx", 0x2c23f98e},
	{"db7-0502.cpr", 0xbdd0fdd1},
	{"db7-0502.idx", 0x81fdad5e},
	{"db7-0511.cpr", 0x3d8ddf90},
	{"db7-0511.idx", 0xacbbcf6d},
	{"db7-0520.cpr", 0x2637b6cf},
	{"db7-0520.idx", 0x3f293059},
	{"db7-1303.cpr", 0x771aa89f},
	{"db7-1303.idx", 0x673b752c},
	{"db7-1312.cpr", 0x3854317c},
	{"db7-1312.idx", 0x82497758},
	{"db7-1321.cpr", 0x3e8ff400},
	{"db7-1321.idx", 0x947cc425},
	{"db7-1330.cpr", 0xd15234c3},
	{"db7-1330.idx", 0xe125b333},
	{"db7-1402.cpr", 0x562c4e92},
	{"db7-1402.idx", 0x390aacd8},
	{"db7-1411.cpr", 0xce3dccc9},
	{"db7-1411.idx", 0xb2cd991d},
	{"db7-1420.cpr", 0xbc011fbf},
	{"db7-1420.idx", 0xf22194ed},
	{"db7-2203.cpr", 0x2551bf8d},
	{"db7-2203.idx", 0x07e17564},
	{"db7-2212.cpr", 0x8da5fbe3},
	{"db7-2212.idx", 0x5d21276f},
	{"db7-2221.cpr", 0xfecce429},
	{"db7-2221.idx", 0x32f2d346},
	{"db7-2230.cpr", 0xaf21d3de},
	{"db7-2230.idx", 0xd17768f5},
	{"db7-2302.cpr", 0x8eca672d},
	{"db7-2302.idx", 0xa9ab4f12},
	{"db7-2311.cpr", 0xc4e5433d},
	{"db7-2311.idx", 0x1fbbc07b},
	{"db7-2320.cpr", 0xec705da7},
	{"db7-2320.idx", 0x8fb3ffc1},
	{"db7-3103.cpr", 0xf77e4501},
	{"db7-3103.idx", 0xfeb8b916},
	{"db7-3112.cpr", 0x5ffd449f},
	{"db7-3112.idx", 0x570fd556},
	{"db7-3121.cpr", 0x7267bc3d},
	{"db7-3121.idx", 0x85de93e1},
	{"db7-3130.cpr", 0x1ab83cfc},
	{"db7-3130.idx", 0xe4654b53},
	{"db7-3202.cpr", 0x023074ba},
	{"db7-3202.idx", 0x78508ba2},
	{"db7-3211.cpr", 0xd57829a8},
	{"db7-3211.idx", 0xb94274d2},
	{"db7-3220.cpr", 0x8f839806},
	{"db7-3220.idx", 0xe8b6e1b5},
	{"db7-4003.cpr", 0x308238b4},
	{"db7-4003.idx", 0x9a8ef625},
	{"db7-4012.cpr", 0x0d175c17},
	{"db7-4012.idx", 0xf74309f2},
	{"db7-4021.cpr", 0x9ac2d50e},
	{"db7-4021.idx", 0x492d2dcf},
	{"db7-4030.cpr", 0x105a6319},
	{"db7-4030.idx", 0xa1067e2b},
	{"db7-4102.cpr", 0xf807a930},
	{"db7-4102.idx", 0x88c8b374},
	{"db7-4111.cpr", 0x53c6329a},
	{"db7-4111.idx", 0x165843bb},
	{"db7-4120.cpr", 0xccd2d80e},
	{"db7-4120.idx", 0x5b953ce9},
	{"db7-5002.cpr", 0x903ffadc},
	{"db7-5002.idx", 0x4cd3765d},
	{"db7-5011.cpr", 0x399df272},
	{"db7-5011.idx", 0xadf8a46a},
	{"db7-5020.cpr", 0x966012b2},
	{"db7-5020.idx", 0xb44dfaa1},
	{"db8-0404.cpr", 0x698e6219},
	{"db8-0404.idx", 0xee912f6d},
	{"db8-0413.cpr", 0x69ef81ea},
	{"db8-0413.idx", 0x337ce842},
	{"db8-0422.cpr", 0x0f5878cb},
	{"db8-0422.idx", 0x3ea766c3},
	{"db8-0431.cpr", 0x22b2e738},
	{"db8-0431.idx", 0xb389e93c},
	{"db8-0440.cpr", 0x9929a112},
	{"db8-0440.idx", 0xc8b2bcd4},
	{"db8-0503.cpr", 0x8a7ce1f2},
	{"db8-0503.idx", 0xef4569c0},
	{"db8-0512.cpr", 0x37482753},
	{"db8-0512.idx", 0x40d15c45},
	{"db8-0521.cpr", 0xc8032452},
	{"db8-0521.idx", 0x312e8132},
	{"db8-0530.cpr", 0x0c235d68},
	{"db8-0530.idx", 0xb581be01},
	{"db8-1313.cpr", 0x68447136},
	{"db8-1313.idx", 0xd628f3c0},
	{"db8-1322.cpr", 0x667e523d},
	{"db8-1322.idx", 0x9bf431ea},
	{"db8-1331.cpr", 0xf1636bca},
	{"db8-1331.idx", 0xc4437677},
	{"db8-1340.cpr", 0x75705942},
	{"db8-1340.idx", 0x52ad65f2},
	{"db8-1403.cpr", 0x5dbbbbb8},
	{"db8-1403.idx", 0x39a54c32},
	{"db8-1412.cpr", 0x32548e52},
	{"db8-1412.idx", 0x4935b7a8},
	{"db8-1421.cpr", 0x58055415},
	{"db8-1421.idx", 0xd7655b45},
	{"db8-1430.cpr", 0x5f0fc264},
	{"db8-1430.idx", 0xbac8caa4},
	{"db8-2222.cpr", 0x8b394801},
	{"db8-2222.idx", 0xcbe902be},
	{"db8-2231.cpr", 0x3b9ebc53},
	{"db8-2231.idx", 0x0eff1736},
	{"db8-2240.cpr", 0x63a4ba8d},
	{"db8-2240.idx", 0xa6bc6d37},
	{"db8-2303.cpr", 0x10cb0da9},
	{"db8-2303.idx", 0x48769c7b},
	{"db8-2312.cpr", 0x96250ffa},
	{"db8-2312.idx", 0x0d043f50},
	{"db8-2321.cpr", 0x47020b72},
	{"db8-2321.idx", 0x361a2564},
	{"db8-2330.cpr", 0x0bcf003b},
	{"db8-2330.idx", 0xc5b3bd6d},
	{"db8-3131.cpr", 0xe7138a48},
	{"db8-3131.idx", 0x058acdf8},
	{"db8-3140.cpr", 0x5dc183cc},
	{"db8-3140.idx", 0x65d694ab},
	{"db8-3203.cpr", 0x36b584ac},
	{"db8-3203.idx", 0xca7bf831},
	{"db8-3212.cpr", 0x4627d957},
	{"db8-3212.idx", 0x6ce0aa59},
	{"db8-3221.cpr", 0x987cae5e},
	{"db8-3221.idx", 0xa74df158},
	{"db8-3230.cpr", 0x893a564b},
	{"db8-3230.idx", 0x26db9f1c},
	{"db8-4040.cpr", 0x649c774d},
	{"db8-4040.idx", 0x97ed951e},
	{"db8-4103.cpr", 0x7520f6f3},
	{"db8-4103.idx", 0x87392f19},
	{"db8-4112.cpr", 0x7e7d694b},
	{"db8-4112.idx", 0xfb81f08c},
	{"db8-4121.cpr", 0x9728e3c8},
	{"db8-4121.idx", 0x5c57add2},
	{"db8-4130.cpr", 0xd524592c},
	{"db8-4130.idx", 0x1c29a53c},
	{"db8-5003.cpr", 0x9245713e},
	{"db8-5003.idx", 0x33ae99f6},
	{"db8-5012.cpr", 0x9e1d2560},
	{"db8-5012.idx", 0x00554862},
	{"db8-5021.cpr", 0x4ba2ec32},
	{"db8-5021.idx", 0x4c7312c4},
	{"db8-5030.cpr", 0x7f415d6a},
	{"db8-5030.idx", 0x10dad04e},
	{"db9-5040.cpr", 0x83e06934},
	{"db9-5040.idx", 0x1f866ec6},
};


/* Function prototypes. */
static int parseindexfile(DBHANDLE *, DBFILE *, int64_t *allocated_bytes);
static void build_file_table(DBHANDLE *hdat);
static void build_autoload_list(DBHANDLE *hdat);
static void assign_subindices(DBHANDLE *hdat, CPRSUBDB *subdb, CCB *ccbp);

unsigned int calc_cache_hashindex(int tablesize, unsigned int filenum, unsigned int blocknum)
{
	unsigned int hashcode;

	hashcode = blocknum | (filenum << 23);
	hashcode = ~hashcode + (hashcode << 15);
	hashcode = hashcode ^ (hashcode >> 12);
	hashcode = hashcode + (hashcode << 2);
	hashcode = hashcode ^ (hashcode >> 4);
	hashcode = hashcode * 2057;
	hashcode = hashcode ^ (hashcode >> 16);
	return(hashcode % tablesize);
}


/*
 * Return a pointer to the hashtable node matching the filenum and blocknum,
 * or else NULL.
 */
CACHE_HASHTABLE_NODE *cache_ht_lookup(DBHANDLE *hdat, unsigned int hashindex, unsigned int filenum, unsigned int blocknum)
{
	CACHE_HASHTABLE_NODE *node;

	for (node = hdat->cache_ht[hashindex]; node; node = node->next) {
		if (node->blocknum == blocknum && node->filenum == filenum)
			return(node);
	}
	return(NULL);
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


float get_avg_ht_list_length(DBHANDLE *hdat)
{
	int i, list_count;
	int length_sum;
	CACHE_HASHTABLE_NODE *node;

	list_count = 0;
	length_sum = 0;
	for (i = 0; i < hdat->cache_ht_size; ++i) {
		node = hdat->cache_ht[i];
		if (node) {
			++list_count;
			while (node) {
				++length_sum;
				node = node->next;
			}
		}
	}
	return((float)length_sum / (float)list_count);
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
	hdat->lookup_stats.avg_ht_list_length = get_avg_ht_list_length(hdat);
	return(&hdat->lookup_stats);
}


static void read_blocknum_from_file(DBHANDLE *hdat, CCB *ccb)
{
	FILE_HANDLE hfile;
	int64_t filepos;
	int stat;

	filepos = (int64_t)ccb->cache_ht_node->blocknum * CACHE_BLOCKSIZE;
	hfile = hdat->dbfiles[ccb->cache_ht_node->filenum].fp;

	/* Seek the start position. */
	if (set_file_pointer(hfile, filepos)) {
		(*hdat->log_msg_fn)("seek failed\n");
		return;
	}
	stat = read_file(hfile, ccb->data, CACHE_BLOCKSIZE, CACHE_BLOCKSIZE);
	if (!stat)
		(*hdat->log_msg_fn)("Error reading file\n");
}


/*
 * Return a pointer to a cache block.
 * Get the least recently used cache
 * block and load it into that.
 * Update the LRU list to make this block the most recently used.
 */
static CCB *load_blocknum_tun_v1(DBHANDLE *hdat, CPRSUBDB *subdb, int blocknum, unsigned int hashindex)
{
	CCB *ccbp;
	CACHE_HASHTABLE_NODE *free_node, *prev, *next;

	++hdat->lookup_stats.lru_cache_loads;

	/* Not cached, need to load this block from disk. */
	ccbp = hdat->ccbs + hdat->ccbs_top;
	if (ccbp->cache_ht_node) {

		/* The LRU block is in use.
		 * Mark its cache hashtable node as unused.
		 */
		free_node = ccbp->cache_ht_node;
		next = free_node->next;
		if (hdat->cache_ht[ccbp->cache_ht_index] == free_node) {
			hdat->cache_ht[ccbp->cache_ht_index] = free_node->next;
		}
		else {
			prev = free_node->previous;
			if (prev)
				prev->next = free_node->next;
		}
		if (next)
			next->previous = free_node->previous;
	}
	else {
		free_node = hdat->cache_ht_free_list;
		hdat->cache_ht_free_list = free_node->next;
	}

	/* The ccbs_top block is now free for use. */
	free_node->filenum = (int)(subdb->file - hdat->dbfiles);
	free_node->blocknum = blocknum;
	free_node->cacheblock_index = hdat->ccbs_top;

	free_node->next = hdat->cache_ht[hashindex];
	if (free_node->next)
		free_node->next->previous = free_node;
	hdat->cache_ht[hashindex] = free_node;
	free_node->previous = NULL;
	
	ccbp->cache_ht_node = free_node;
	ccbp->cache_ht_index = hashindex;

	/* Read this block from disk into ccbs_top's ccb. */
	read_blocknum_from_file(hdat, hdat->ccbs + hdat->ccbs_top);

	assign_subindices(hdat, subdb, ccbp);

	/* Fix linked list to point to the next oldest entry */
	hdat->ccbs_top = hdat->ccbs[hdat->ccbs_top].next;
	return(ccbp);
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
	uint32_t index;
	unsigned short *runlength;
	unsigned short value_runs_offset;
	int64_t index64;
	int bm, bk, wm, wk;
	int i, subslicenum;
	int idx_blocknum, subidx_blocknum;	
	int blocknum;
	int returnvalue;
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

	subslicenum = (int)(index64 / (int64_t)MAX_SUBSLICE_INDICES);
	index = (uint32_t)(index64 - (int64_t)subslicenum * (int64_t)MAX_SUBSLICE_INDICES);

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

		diskblock = dbpointer->file->file_cache + (subidx_blocknum * (size_t)SUBINDEX_BLOCKSIZE) +
					dbpointer->first_idx_block * (size_t)IDX_BLOCKSIZE;
		n_idx = indices[subidx_blocknum];
		i = 0;
		if (subidx_blocknum == dbpointer->first_subidx_block)
			i = dbpointer->startbyte - subidx_blocknum * SUBINDEX_BLOCKSIZE;
	}
	else {		/* Not an autoloaded block. */
		int filenum;
		unsigned int hashindex;
		CCB *ccbp;
		CACHE_HASHTABLE_NODE *node;

		/* We know the index and the database, so look in 
		 * the indices array to find the right index block.
		 */
		indices = dbpointer->indices;
		idx_blocknum = find_block(0, dbpointer->num_idx_blocks, indices, index);

		/* See if blocknumber is already in cache. */
		blocknum = dbpointer->first_idx_block + idx_blocknum;

		/* Is this block already cached? Look it up in the cache hashtable. */
		take_lock(egdb_lock);
		filenum = (int)(dbpointer->file - hdat->dbfiles);
		hashindex = calc_cache_hashindex(hdat->cache_ht_size, filenum, blocknum);
		node = cache_ht_lookup(hdat, hashindex, filenum, blocknum);
		if (node) {

			/* Already cached.  Update the lru list. */
			ccbp = update_lru<CCB>(hdat, dbpointer->file, node->cacheblock_index);
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
			ccbp = load_blocknum_tun_v1(hdat, dbpointer, blocknum, hashindex);
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

	/* and move through the bytes until we overshoot index
	 * set the start byte in this block: if it's block 0 of a db,
	 * this can be !=0.
	 */
	runlength = decompress_catalog[dbpointer->catalog_entry].runlength_table;
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
	value_runs_offset = decompress_catalog[dbpointer->catalog_entry].value_runs[diskblock[i]];
	while (1) {
		n_idx += value_runs[value_runs_offset + 1] + (value_runs[value_runs_offset + 2] << 8);
		if (n_idx > index)
			break;
		value_runs_offset += 3;
	}
	returnvalue = dbpointer->vmap[value_runs[value_runs_offset]];
	++hdat->lookup_stats.db_returns;

	return(returnvalue);
}

static int init_autoload_subindices(DBHANDLE *hdat, DBFILE *file, size_t *allocated_bytes)
{
	int i, k, m, size;
	int first_subi, num_subi, subi;
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
					runlen_table = decompress_catalog[p->subdb[k].catalog_entry].runlength_table;

					index = 0;
					subi = first_subi;
					for (m = p->subdb[k].startbyte; subi < num_subi; ++m) {
						if ((m % SUBINDEX_BLOCKSIZE) == 0) {
							subi = m / SUBINDEX_BLOCKSIZE;
							if (subi >= num_subi)
								break;
							p->subdb[k].autoload_subindices[subi] = index;
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
	while (subdb->first_idx_block == ccbp->cache_ht_node->blocknum && subdb->startbyte > 0)
		subdb = subdb->prev;

	/* For each subdb that has data in this block. */
	do {
		first_blocknum = subdb->first_idx_block;
		if (first_blocknum == ccbp->cache_ht_node->blocknum) {
			subi = subdb->first_subidx_block;
			m = subdb->startbyte;
			index = 0;
		}
		else {
			subi = 0;
			m = 0;
			idx_blocknum = ccbp->cache_ht_node->blocknum - subdb->first_idx_block;
			index = subdb->indices[idx_blocknum];
		}

		if (subdb->first_idx_block + subdb->num_idx_blocks - 1 > (int)ccbp->cache_ht_node->blocknum)
			end_subi = NUM_SUBINDICES - 1;
		else
			end_subi = subdb->last_subidx_block;

		runlen_table = decompress_catalog[subdb->catalog_entry].runlength_table;
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
	while (subdb && (subdb->first_idx_block == ccbp->cache_ht_node->blocknum));
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
 * 
 * pieces is the maximum number of pieces to do lookups for.
 * cache_mb is the amount of ram to use for the driver, in bytes * 1E6.
 * filepath is the path to the database files.
 * msg_fn is a pointer to a function which will log status and error messages.
 * A non-zero return value means some kind of error occurred.  The nature of
 * any errors are communicated through the msg_fn.
 */
static int initdblookup(DBHANDLE *hdat, int pieces, int kings_1side_8pcs, int cache_mb, char const *filepath, void (*msg_fn)(char const*))
{
	int i, j, blocknumnum, stat;
	int t0, t1, t2, t3;
	char dbname[MAXFILENAME];
	char msg[MAXMSG];
	int64_t allocated_bytes;		/* keep track of heap allocations in bytes. */
	int64_t autoload_bytes;			/* keep track of autoload allocations in bytes. */
	int cache_mb_avail;
	int max_autoload;
	int64_t total_dbsize;
	size_t size;
	int count;
	DBFILE *f;
	CPRSUBDB *subdb;
	unsigned char *blockp;		/* Base address of an allocate group of cache buffers. */

	t0 = std::clock();

	/* Save off some global data. */
	strcpy(hdat->db_filepath, filepath);

	/* Add trailing backslash if needed. */
	size = (int)strlen(hdat->db_filepath);
	if (size && (hdat->db_filepath[size - 1] != '/'))
		strcat(hdat->db_filepath, "/");

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

		/* Calculate the number of cache blocks of data in this dbfile. */
		if (hdat->dbfiles[i].is_present) {
			hdat->dbfiles[i].num_cacheblocks = hdat->dbfiles[i].num_idx_blocks;
			if (hdat->dbfiles[i].num_idx_blocks > hdat->dbfiles[i].num_cacheblocks)
				++hdat->dbfiles[i].num_cacheblocks;
		}
	}

	/* End of reading index files, start of autoload. */
	t1 = std::clock();
	std::sprintf(msg, "Reading index files took %.0f secs\n", tdiff_secs(t1, t0));
	(*hdat->log_msg_fn)(msg);

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

	/* Open file handles for each db; leave them open for quick access. */
	for (i = 0; i < hdat->numdbfiles; ++i) {

		/* Dont do more pieces than he asked for. */
		if (hdat->dbfiles[i].pieces > pieces)
			break;

		if (!hdat->dbfiles[i].is_present)
			continue;

		std::sprintf(dbname, "%s%s.cpr", hdat->db_filepath, hdat->dbfiles[i].name);
		hdat->dbfiles[i].fp = open_file(dbname);
		if (hdat->dbfiles[i].fp == nullptr) {
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

			read_file(hdat->dbfiles[i].fp, hdat->dbfiles[i].file_cache, size, get_page_size());

			/* Close the db file, we are done with it. */
			close_file(hdat->dbfiles[i].fp);
			hdat->dbfiles[i].fp = nullptr;

			/* Allocate the subindices. */
			stat = init_autoload_subindices(hdat, hdat->dbfiles + i, &size);
			if (stat)
				return(1);

			allocated_bytes += size;
			autoload_bytes += size;
		}
	}
	std::sprintf(msg, "Allocated %dkb for indexing\n", (int)((allocated_bytes - autoload_bytes) / 1024));
	(*hdat->log_msg_fn)(msg);
	std::sprintf(msg, "Allocated %dkb for permanent slice caches\n", (int)(autoload_bytes / 1024));
	(*hdat->log_msg_fn)(msg);

	t2 = std::clock();
	std::sprintf(msg, "Autoload took %.0f secs\n", tdiff_secs(t2, t1));
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
			hdat->cache_ht_size = (3 * hdat->cacheblocks) / 2;
			std::sprintf(msg, "Allocating the minimum %d cache buffers\n",
							hdat->cacheblocks);
			(*hdat->log_msg_fn)(msg);
		}
		else {
			int64_t bytes_left = (int64_t)cache_mb * (int64_t)ONE_MB - (int64_t)allocated_bytes;
			int64_t bytes_per_cache_block = CACHE_BLOCKSIZE + sizeof(CCB) + 3 * sizeof(hdat->cache_ht[0]) / 2 + sizeof(CACHE_HASHTABLE_NODE);

			/* Calculate the number of cache blocks and hashtable nodes that can be allocated
			 * from the bytes remaining.  Use 50% more hashtable nodes than cache blocks.
			 */
			hdat->cacheblocks = (int)(bytes_left / bytes_per_cache_block);
			hdat->cache_ht_size = (3 * hdat->cacheblocks) / 2;

			hdat->cacheblocks = (std::min)(hdat->cacheblocks, i);
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
			hdat->ccbs[i].cache_ht_index = UNDEFINED_BLOCK_ID;
			hdat->ccbs[i].cache_ht_node = NULL;
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

		/* Allocate the cache hashtable. */
		if (hdat->cache_ht_size > 0) {
			hdat->cache_ht = (CACHE_HASHTABLE_NODE **)std::malloc(hdat->cache_ht_size * sizeof(hdat->cache_ht[0]));

			hdat->cache_ht_nodes_base = (CACHE_HASHTABLE_NODE *)std::malloc(hdat->cacheblocks * sizeof(hdat->cache_ht_nodes_base[0]));
			if (!hdat->cache_ht || !hdat->cache_ht_nodes_base) {
				(*hdat->log_msg_fn)("Cannot allocate memory for cache hashtable\n");
				return(1);
			}

			/* Init each hashtable bucket linked list to empty. */
			for (i = 0; i < hdat->cache_ht_size; ++i)
				hdat->cache_ht[i] = NULL;

			/* Init the free list of hashtable nodes. */
			for (i = 0; i < hdat->cacheblocks; ++i) {
				hdat->cache_ht_nodes_base[i].cacheblock_index = UNDEFINED_BLOCK_ID;
				hdat->cache_ht_nodes_base[i].filenum = 0;
				hdat->cache_ht_nodes_base[i].blocknum = 0;
				if (i != hdat->cacheblocks - 1)
					hdat->cache_ht_nodes_base[i].next = hdat->cache_ht_nodes_base + i + 1;
				else
					hdat->cache_ht_nodes_base[i].next = NULL;
				if (i != 0)
					hdat->cache_ht_nodes_base[i].previous = hdat->cache_ht_nodes_base + i - 1;
				else
					hdat->cache_ht_nodes_base[i].previous = NULL;
			}

			/* Init the head of the free list. */
			hdat->cache_ht_free_list = hdat->cache_ht_nodes_base;
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

			for (blocknumnum = 0; blocknumnum < f->num_cacheblocks && count < hdat->cacheblocks; ++blocknumnum) {
				unsigned int hashindex;
				unsigned int filenum;
				CACHE_HASHTABLE_NODE *node;

				filenum = (int)(f - hdat->dbfiles);
				hashindex = calc_cache_hashindex(hdat->cache_ht_size, filenum, blocknumnum);
				node = cache_ht_lookup(hdat, hashindex, filenum, blocknumnum);
				if (node == NULL) {
					subdb = find_first_subdb(hdat, f, blocknumnum);
					load_blocknum_tun_v1(hdat, subdb, blocknumnum, hashindex);
					++count;
				}
			}
			if (count >= hdat->cacheblocks)
				break;
		}
		t3 = std::clock();
		std::sprintf(msg, "Read %d buffers in %.0f sec, %.3f msec/buffer\n", 
					hdat->cacheblocks, tdiff_secs(t3, t2), 
					(double)(t3 - t2) / (double)hdat->cacheblocks);
		(*hdat->log_msg_fn)(msg);
		std::sprintf(msg, "Egdb init took %.0f sec total\n", tdiff_secs(t3, t0));
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
	int singlevalue, startbyte;
	INDEX indices[IDX_READBUFSIZE];				/* Use temp stack buffer to avoid fragmenting the heap. */
	int i, count, linecount;
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
	if (cprfp == nullptr) {

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
			dbpointer->singlevalue = NOT_SINGLEVALUE;
			dbpointer->startbyte = startbyte + (first_idx_block % IDX_BLOCK_MULT) * FILE_IDX_BLOCKSIZE;
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
			}

			/* Check for one or more info lines beginning with a '#'. */
			while (1) {
				stat = std::fgetc(fp);
				if (std::isspace(stat))
					continue;
				if (stat == '#') {
					char line[120];

					std::fgets(line, sizeof(line), fp);
					if (std::strstr(line, "vmap")) {
						int cat_entry, v0, v1, v2, v3, v4, v5;
						stat = std::sscanf(line, " vmap %d: %d,%d,%d,%d,%d,%d",
							&cat_entry, &v0, &v1, &v2, &v3, &v4, &v5);
						if (stat == 7) {
							dbpointer->catalog_entry = cat_entry;
							dbpointer->vmap[0] = v0;
							dbpointer->vmap[1] = v1;
							dbpointer->vmap[2] = v2;
							dbpointer->vmap[3] = v3;
							dbpointer->vmap[4] = v4;
							dbpointer->vmap[5] = v5;
						}
						else {
							if (stat == 4) {
								dbpointer->catalog_entry = cat_entry;
								dbpointer->vmap[0] = v0;
								dbpointer->vmap[1] = v1;
								dbpointer->vmap[2] = v2;
							}
						}
					}
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
		if (f->num_idx_blocks != count + prev->first_idx_block) {
			std::sprintf(msg, "count of index blocks dont match: %d %d\n",
					f->num_idx_blocks, count + prev->first_idx_block);
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
				virtual_free(hdat->ccbs[i].data);
		}

		/* Free the cache control blocks. */
		std::free(hdat->ccbs);
	}

	for (i = 0; i < sizeof(hdat->dbfiles) / sizeof(hdat->dbfiles[0]); ++i) {
		if (hdat->dbfiles[i].pieces > hdat->dbpieces)
			continue;

		if (!hdat->dbfiles[i].is_present)
			continue;

		if (hdat->dbfiles[i].file_cache) {
			virtual_free(hdat->dbfiles[i].file_cache);
			hdat->dbfiles[i].file_cache = 0;
		}

		if (hdat->dbfiles[i].fp != nullptr)
			close_file(hdat->dbfiles[i].fp);

		hdat->dbfiles[i].num_idx_blocks = 0;
		hdat->dbfiles[i].num_cacheblocks = 0;
		hdat->dbfiles[i].fp = nullptr;
	}
	std::memset(hdat->dbfiles, 0, sizeof(hdat->dbfiles));
	if (hdat->cache_ht)
		std::free(hdat->cache_ht);

	if (hdat->cache_ht_nodes_base)
		std::free(hdat->cache_ht_nodes_base);

	for (i = 0; i < DBSIZE; ++i) {
		p = hdat->cprsubdatabase + i;
		if (p->subdb != NULL) {
			for (k = 0; k < p->num_subslices; ++k) {
				if (p->subdb[k].indices)
					std::free(p->subdb[k].indices);
				if (p->subdb[k].autoload_subindices)
					std::free(p->subdb[k].autoload_subindices);
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
		std::sprintf(filename, "%s.idx", hdat->dbfiles[i].name);
		pcrc = find_file_crc(filename, dbcrc, ARRAY_SIZE(dbcrc));
		if (!pcrc)
			continue;

		std::sprintf(filename, "%s%s.idx", hdat->db_filepath, hdat->dbfiles[i].name);
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
		std::sprintf(filename, "%s.cpr", hdat->dbfiles[i].name);
		pcrc = find_file_crc(filename, dbcrc, ARRAY_SIZE(dbcrc));
		if (!pcrc)
			continue;

		std::sprintf(filename, "%s%s.cpr", hdat->db_filepath, hdat->dbfiles[i].name);
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


EGDB_DRIVER *egdb_open_wld_tun_v1(int pieces, int kings_1side_8pcs,
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
		std::free(handle);
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


/* Used to test mutual exclusion locking. */
LOCKT *get_tun_v1_lock(void)
{
	return(&egdb_lock);
}



