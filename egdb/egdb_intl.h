#pragma once
#include <cstdint>  // uint64_t

namespace egdb_interface {

typedef uint64_t EGDB_BITBOARD;

/* This is the driver's definition of a draughts position.
 * The bitboards have a 1 bit gap after each group of 10 squares.
 * bit0 for square 1, bit1 for square 2, ..., bit9 for square 10, and
 * then bit 10 is skipped, bit11 for square 11, ...
 * The 4 skipped bits are 10, 21, 32, and 43.
 */
struct EGDB_POSITION {
	EGDB_BITBOARD black;
	EGDB_BITBOARD white;
	EGDB_BITBOARD king;
};

/* Color definitions. */
enum EGDB_COLOR {
	EGDB_BLACK = 0,
	EGDB_WHITE = 1
};

/* Values returned by handle->lookup(). */
enum WLD_VALUE {
	EGDB_UNKNOWN = 0,			/* value not in the database. */
	EGDB_WIN = 1,
	EGDB_LOSS = 2,
	EGDB_DRAW = 3,
	EGDB_DRAW_OR_LOSS = 4,
	EGDB_WIN_OR_DRAW = 5,
	EGDB_NOT_IN_CACHE = -1,		/* conditional lookup and position not in cache. */
	EGDB_SUBDB_UNAVAILABLE = -2	/* this slice is not being used */
};

#define NOT_SINGLEVALUE 127

enum MTC_VALUE {
	MTC_THRESHOLD = 10,
	MTC_SKIPS = 94,
	MTC_LESS_THAN_THRESHOLD = 1,
	MTC_UNKNOWN = 0
};

/* MTC macros. */

/* Option strings to egdb_open(). */
#define MAXPIECES_OPTSTR "maxpieces"
#define MAXKINGS_1SIDE_8PCS_OPTSTR "maxkings_1side_8pcs"

/* Decode a moves-to-conv number that is >= the threshold for saving. */
#define MTC_DECODE(val) (2 * ((val) - MTC_SKIPS))

enum EGDB_TYPE {
	EGDB_WLD_RUNLEN = 0,
	EGDB_MTC_RUNLEN,
	EGDB_WLD_HUFFMAN,
	EGDB_WLD_TUN_V1,
	EGDB_WLD_TUN_V2,
};

/* for database lookup stats. */
struct EGDB_STATS {
	unsigned int lru_cache_hits;
	unsigned int lru_cache_loads;
	unsigned int autoload_hits;
	unsigned int db_requests;				/* total egdb requests. */
	unsigned int db_returns;				/* total egdb w/l/d returns. */
	unsigned int db_not_present_requests;	/* requests for positions not in the db */
	float avg_ht_list_length;
};

struct EGDB_VERIFY_MSGS {
	char crc_failed[80];
	char ok[80];
	char errors[80];
	char no_errors[80];
};

/* The driver handle type */
struct EGDB_DRIVER {
	int (*lookup)(EGDB_DRIVER *handle, EGDB_POSITION *position, int color, int cl);
	void (*reset_stats)(EGDB_DRIVER *handle);
	EGDB_STATS *(*get_stats)(EGDB_DRIVER *handle);
	int (*verify)(EGDB_DRIVER *handle, void (*msg_fn)(char const *msg), int *abort, EGDB_VERIFY_MSGS *msgs);
	int (*close)(EGDB_DRIVER *handle);
	int (*get_pieces)(EGDB_DRIVER *handle, int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side);
	void *internal_data;
};

/* Open an endgame database driver. */
EGDB_DRIVER *egdb_open(char const *options, 
						int cache_mb,
						char const *directory,
						void (*msg_fn)(char const *msg));

/* Identify a db, get its size and type. */
int egdb_identify(char const *directory, EGDB_TYPE *egdb_type, int *max_pieces);

inline
int egdb_lookup(EGDB_DRIVER *handle, EGDB_POSITION *position, int color, int cl)
{
	return handle->lookup(handle, position, color, cl);
}

inline
void egdb_reset_stats(EGDB_DRIVER *handle)
{
	handle->reset_stats(handle);
}

inline
EGDB_STATS *egdb_get_stats(EGDB_DRIVER *handle)
{
	return handle->get_stats(handle);
}

inline
int egdb_verify(EGDB_DRIVER *handle, void (*msg_fn)(char const *msg), int *abort, EGDB_VERIFY_MSGS *msgs)
{
	return handle->verify(handle, msg_fn, abort, msgs);
}

inline
int egdb_close(EGDB_DRIVER *handle)
{
	return handle->close(handle);
}

inline
int egdb_get_pieces(EGDB_DRIVER *handle, int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side)
{
	return handle->get_pieces(handle, max_pieces, max_pieces_1side, max_9pc_kings, max_8pc_kings_1side);
}

}	// namespace egdb_interface

