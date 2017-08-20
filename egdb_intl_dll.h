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
