#pragma once

#include <cstdint>

/*
 * To use the library, include the header file egdb_intl.h in your application source
 * code and link with the lib file egdb_intl.lib.
 *
 * To lookup values in a database, the database must first be opened using
 * egdb_open().  This returns a handle to an open database.  The handle is
 * a pointer to a structure of db access function pointers.  These
 * access functions are then used for all subsequent communication with 
 * the driver.  The handle also holds all state data that is used by each 
 * instance of an open driver, thus multiple handles can be opened from a
 * a single process without conflicting with one another.  Critical regions
 * within the driver are protected by mutual exclusion mechanisms so multiple
 * threads can use a single handle.
 * 
 * The open function should be tested for a NULL return pointer, which means
 * that some kind of error occurred.  The likely reasons for errors are either
 * the wrong directory path was given for the database files, or the driver
 * was unable to allocate all the memory that it needed from the heap.  If a
 * NULL pointer is returned by the open function, an error message will be passed
 * back to the application through the callback message function (the fourth
 * argument passed to egdb_open).
 *
 * The first argument to the open function is an a character string of options.
 * The options are of the form name=value.  Multiple options are separated by
 * a semicolon.  These options are defined:
 *
 *	maxpieces			sets the maximum number of pieces that the driver will
 *						attempt to lookup values for.  Normally the driver will
 *						use all the databases files that it finds during
 *						initialization.
 *
 *	maxkings_1side_8pcs	
 *						sets the maximum number of kings that the driver will
 *						attempt to lookup values for of 8-piece positions.
 *
 * The second argument to the open function is the number of megabytes (2^20 bytes)
 * of heap memory that the driver will use.  The driver will use some minimum
 * amount of memory even if you give a value which is lower than this minimum.
 * Any memory that you give
 * above this mimimum is used exclusively to create more cache buffers.  Cache
 * buffers are allocated in 4k blocks using VirtualAlloc, and are managed using
 * a 'least recently used' replacement strategy.
 * If the memory argument is large enough the driver will allocate permanent
 * buffers for some of the smaller subsets of the database.  In the statistics
 * counters these are called 'autoload' buffers.
 *
 * The third argument to the open function is the path to the location of the 
 * database files.
 *
 * The last argument to the open function is a pointer to a function that will 
 * receive status and error messages from the driver.  The driver normally
 * emits status messages when it starts up, with 
 * information about files opened and memory used.  If any errors occur during
 * the open call, the details will be reported through the message function.
 * A message function that writes messages to stdout can be provided
 * like this:
 *
 *        void msg_fn(char *msg)
 *        {
 *             printf(msg);
 *        }
 *
 * The API provides a couple of functions for gathering statistics on db lookups.
 * (*driver->get_stats)() returns a pointer to a structure of various counts
 * that have accumulated since the counts were last reset.
 * (*driver->reset_stats)() resets all the counts to 0.
 *
 * The (*driver->lookup)() function returns the value of a position in an open
 * database.  It takes 4 arguments -- a driver handle, position, color, and a
 * conditional lookup boolean.  If the conditional lookup argument is true,
 * then the driver will only get the value of the
 * position if the position is already cached in ram somewhere, otherwise
 * EGDB_NOT_IN_CACHE will be returned.  If the conditional lookup
 * argument if false, the driver will always attempt to get a value for the
 * position even if it has to read a disk file to get it.
 *
 * For the WLD databases, the values returned by lookup are defined by the
 * macros EGDB_UNKNOWN, EGDB_WIN, EGDB_LOSS, EGDB_DRAW, and EGDB_NOT_IN_CACHE.
 * The lookup function will return the correct EGDB_WIN or EGDB_LOSS if it is
 * given a position where one side has no pieces.
 *
 * In the 9-piece database some positions have values EGDB_DRAW_OR_LOSS and 
 * EGDB_WIN_OR_DRAW.
 *
 * For the MTC databases, the values returned by lookup are either the number
 * of moves to a conversion move, or the value MTC_LESS_THAN_THRESHOLD for 
 * positions which are close to a conversion move.  The Kingsrow mtc databases
 * do not store positions which are closer than MTC_THRESHOLD moves to a 
 * conversion move.  The Kingsrow mtc db will only return even mtc values,
 * because the database represents the mtc value internally by distance/2.  The
 * true value is either the value returned, or value + 1.  An application
 * program can infer the true even or odd value of a position by looking up
 * the mtc value of the position's successors.  If the largest successor mtc
 * value is the same as the position's, then the position's mtc true value is
 * 1 greater than the returned value.
 *
 * The WLD databases will not return valid values for positions where
 * the side to move has a capture. There is no way to tell this from the
 * return value, which will normally be one of the win, loss, or draw values
 * from a lookup on a capture position.  Your application must test for these
 * captures and avoid calling lookup for any positions where they are present.
 * The MTC databases do not have this restriction.
 *
 * The driver has tables of good crc values for all db files.
 * The (*driver->verify)() function performs a crc check on each index and
 * data file in the open database.  It returns 0 if all crcs compared ok. 
 * If any file fails the check, the verify call returns a non-zero value, and error
 * messages are sent through the callback msg_fn that was registered when the
 * driver was opened.
 *
 * The (*driver->close)() function frees all heap memory used by the driver.
 * If the application needs to change anything about an egdb driver after it
 * has been opened, such as number of pieces or megabytes of ram to use,
 * it must be closed and then opened again with the new parameters.  Close
 * returns 0 on success, non-zero if there are any errors.
 *
 */

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

#define EGDB_HAS_NONSIDE_CAPTURES 1

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

typedef int64_t EGDB_BITBOARD;

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
	int (*verify)(struct egdb_driver *handle, void (*msg_fn)(char *), int *abort, EGDB_VERIFY_MSGS *msgs);
	int (*close)(struct egdb_driver *handle);
	int (*get_pieces)(struct egdb_driver *handle, int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side);
	void *internal_data;
} EGDB_DRIVER;

/* Open an endgame database driver. */
EGDB_DRIVER *egdb_open(char *options, 
						int cache_mb,
						char *directory,
						void (*msg_fn)(char *));

/*
 * Identify which type of database is present, and the maximum number of pieces
 * for which it has data.
 * The return value of the function is 0 if a database is found, and its
 * database type and piece info are written using the pointer arguments.
 * The return value is non-zero if no database is found.  In this case the values
 * for egdb_type and max_pieces are undefined on return.
 */
int egdb_identify(char *directory, EGDB_TYPE *egdb_type, int *max_pieces);

