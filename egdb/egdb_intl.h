#pragma once
#include <cstdint>

/* Color definitions. */
#define EGDB_BLACK 0
#define EGDB_WHITE 1

/* Values returned by handle->lookup(). */
#define EGDB_UNKNOWN 0			/* value not in the database. */
#define EGDB_WIN 1
#define EGDB_LOSS 2
#define EGDB_DRAW 3
#define EGDB_DRAW_OR_LOSS 4
#define EGDB_WIN_OR_DRAW 5
#define EGDB_NOT_IN_CACHE -1			/* conditional lookup and position not in cache. */
#define EGDB_SUBDB_UNAVAILABLE -2		/* this slice is not being used */

#define NOT_SINGLEVALUE 127

/* MTC macros. */
#define MTC_THRESHOLD 10
#define MTC_SKIPS 94
#define MTC_LESS_THAN_THRESHOLD 1
#define MTC_UNKNOWN 0

/* Option strings to egdb_open(). */
#define MAXPIECES_OPTSTR "maxpieces"
#define MAXKINGS_1SIDE_8PCS_OPTSTR "maxkings_1side_8pcs"

/* Decode a moves-to-conv number that is >= the threshold for saving. */
#define MTC_DECODE(val) (2 * ((val) - MTC_SKIPS))

typedef enum {
	EGDB_WLD_RUNLEN = 0,
	EGDB_MTC_RUNLEN,
	EGDB_WLD_HUFFMAN,
	EGDB_WLD_TUN_V1,
	EGDB_WLD_TUN_V2,
} EGDB_TYPE;

/* for database lookup stats. */
typedef struct {
	unsigned int lru_cache_hits;
	unsigned int lru_cache_loads;
	unsigned int autoload_hits;
	unsigned int db_requests;				/* total egdb requests. */
	unsigned int db_returns;				/* total egdb w/l/d returns. */
	unsigned int db_not_present_requests;	/* requests for positions not in the db */
	float avg_ht_list_length;
} EGDB_STATS;

typedef struct {
	char crc_failed[80];
	char ok[80];
	char errors[80];
	char no_errors[80];
} EGDB_VERIFY_MSGS;

typedef uint64_t EGDB_BITBOARD;

/* This is the driver's definition of a draughts position.
 * The bitboards have a 1 bit gap after each group of 10 squares.
 * bit0 for square 1, bit1 for square 2, ..., bit9 for square 10, and
 * then bit 10 is skipped, bit11 for square 11, ...
 * The 4 skipped bits are 10, 21, 32, and 43.
 */
typedef struct {
	EGDB_BITBOARD black;
	EGDB_BITBOARD white;
	EGDB_BITBOARD king;
} EGDB_POSITION;

/* The driver handle type */
typedef struct egdb_driver {
	int (*lookup)(struct egdb_driver *handle, EGDB_POSITION *position, int color, int cl);
	void (*reset_stats)(struct egdb_driver *handle);
	EGDB_STATS *(*get_stats)(struct egdb_driver *handle);
	int (*verify)(struct egdb_driver *handle, void (*msg_fn)(char const *msg), int *abort, EGDB_VERIFY_MSGS *msgs);
	int (*close)(struct egdb_driver *handle);
	int (*get_pieces)(struct egdb_driver *handle, int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side);
	void *internal_data;
} EGDB_DRIVER;

/* Open an endgame database driver. */
EGDB_DRIVER *egdb_open(char const *options, 
						int cache_mb,
						char const *directory,
						void (*msg_fn)(char const *msg));

/* Identify a db, get its size and type. */
int egdb_identify(char const *directory, EGDB_TYPE *egdb_type, int *max_pieces);

