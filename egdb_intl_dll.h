#pragma once

#include <Windows.h>
#include <comutil.h>

enum RETURN_VALUE {
	EGDB_DRIVER_TABLE_FULL = -100,
	EGDB_FOPEN_FAIL = -101,
	EGDB_OPEN_FAIL = -102,
	EGDB_IDENTIFY_FAIL = -103,
	EGDB_BAD_HANDLE = -104,
	EGDB_NULL_INTERNAL_HANDLE = -105,
	EGDB_FEN_ERROR = -106,
};

extern "C" int __stdcall egdb_open(char *options,
			int cache_mb,
			char *directory,
			char *filename);
extern "C" int __stdcall egdb_close(int handle);
extern "C" int __stdcall egdb_identify(char *directory, int *egdb_type, int *max_pieces);
extern "C" int __stdcall egdb_lookup_fen(int handle, char *fen, int cl);
extern "C" int __stdcall egdb_lookup_fen_with_search(int handle, char *fen);
extern "C" int __stdcall egdb_lookup_with_search(int handle, egdb_interface::BOARD *board, int color);
extern "C" int __stdcall get_movelist(egdb_interface::BOARD *board, int color, egdb_interface::BOARD *ml);
extern "C" int16_t __stdcall is_capture(egdb_interface::BOARD *board, int color);
extern "C" int64_t __stdcall getdatabasesize_slice(int nbm, int nbk, int nwm, int nwk);
extern "C" void __stdcall indextoposition(int64_t index, egdb_interface::BOARD *p, int nbm, int nbk, int nwm, int nwk);
extern "C" int64_t __stdcall positiontoindex(egdb_interface::BOARD *pos, int nbm, int nbk, int nwm, int nwk);
extern "C" int16_t __stdcall is_sharp_win(int handle, egdb_interface::BOARD *board, int color, egdb_interface::BOARD *sharp_move_pos);
extern "C" int __stdcall move_string(egdb_interface::BOARD *last_board, egdb_interface::BOARD *new_board, int color, char *move);
extern "C" int __stdcall positiontofen(egdb_interface::BOARD *last_board, int color, char *move);
extern "C" int __stdcall fentoposition(char *fen, egdb_interface::BOARD *pos, int *color);
