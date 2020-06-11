#pragma once
#include <stdint.h>

namespace egdb_dll {

typedef uint64_t Bitboard;

/* This is the driver's definition of a draughts position.
 * The bitboards have a 1 bit gap after each group of 10 squares.
 * bit0 for square 1, bit1 for square 2, ..., bit9 for square 10, and
 * then bit 10 is skipped, bit11 for square 11, ...
 * The 4 skipped bits are 10, 21, 32, and 43.
 */
struct Position {
	Bitboard black;
	Bitboard white;
	Bitboard king;
};

/* Color definitions. */
enum EGDB_COLOR {
	EGDB_BLACK = 0,
	EGDB_WHITE = 1
};

enum RETURN_VALUE {
	EGDB_DRIVER_TABLE_FULL = -100,
	EGDB_FOPEN_FAIL = -101,
	EGDB_OPEN_FAIL = -102,
	EGDB_IDENTIFY_FAIL = -103,
	EGDB_BAD_HANDLE = -104,
	EGDB_NULL_INTERNAL_HANDLE = -105,
	EGDB_FEN_ERROR = -106,
	EGDB_NOT_WLD_TYPE = -107,
	EGDB_NOT_DISTANCE_TYPE = -108,
	EGDB_LOOKUP_NOT_POSSIBLE = -109,
};

enum {
	maxmoves = 128,
};

extern "C" int __stdcall egdb_open(char *options, int cache_mb, const char *directory, const char *filename);
extern "C" int __stdcall egdb_close(int handle);
extern "C" int __stdcall egdb_identify(const char *directory, int *egdb_type, int *max_pieces);
extern "C" int __stdcall egdb_lookup(int handle, Position *board, int color, int cl);
extern "C" int __stdcall egdb_lookup_fen(int handle, char *fen, int cl);
extern "C" int __stdcall egdb_lookup_fen_with_search(int handle, char *fen);
extern "C" int __stdcall egdb_lookup_with_search(int handle, Position *board, int color);
extern "C" int __stdcall egdb_lookup_distance(int handle_wld, int handle_dist, const char *fen, 
									int distances[maxmoves], char moves[maxmoves][20]);
extern "C" int __stdcall get_movelist(Position *board, int color, Position movelist[maxmoves]);
extern "C" int16_t __stdcall is_capture(Position *board, int color);
extern "C" int64_t __stdcall getdatabasesize_slice(int nbm, int nbk, int nwm, int nwk);
extern "C" void __stdcall indextoposition(int64_t index, Position *pos, int nbm, int nbk, int nwm, int nwk);
extern "C" int64_t __stdcall positiontoindex(Position *pos, int nbm, int nbk, int nwm, int nwk);
extern "C" int16_t __stdcall is_sharp_win(int handle, Position *board, int color, Position *sharp_move_pos);
extern "C" int __stdcall move_string(Position *last_board, Position *new_board, int color, char *move);
extern "C" int __stdcall positiontofen(Position *board, int color, char *fen);
extern "C" int __stdcall fentoposition(char *fen, Position *pos, int *color);

}	// namespace

