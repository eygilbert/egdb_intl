#include "engine/project.h"
#include "engine/board.h"
#include "engine/move_api.h"
#include "engine/fen.h"
#include "engine/print_move.h"
#include "egdb/egdb_intl.h"
#include "egdb/egdb_search.h"
#include "builddb/indexing.h"
#include "../egdb_intl_dll.h"
#include <Windows.h>
#include <stdio.h>


typedef struct {
	char filename[260];
	void (*msg_fn)(char const *msg);
	egdb_interface::EGDB_INFO db;
} DRIVER_INFO;

DRIVER_INFO drivers[4];

void msg_fn(char *filename, char const *msg)
{
	FILE *fp;

	if (strlen(filename) == 0)
		return;
	fp = fopen(filename, "a");
	if (!fp)
		return;

	fprintf(fp, "%s", msg);
	fclose(fp);
}


void msg_fn0(char const *msg)
{
	msg_fn(drivers[0].filename, msg);
}


void msg_fn1(char const *msg)
{
	msg_fn(drivers[1].filename, msg);
}


void msg_fn2(char const *msg)
{
	msg_fn(drivers[2].filename, msg);
}


void msg_fn3(char const *msg)
{
	msg_fn(drivers[3].filename, msg);
}


int check_handle(int handle)
{
	if (handle < 0 || handle >= ARRAY_SIZE(drivers))
		return(EGDB_BAD_HANDLE);

	if (drivers[handle].db.handle == nullptr)
		return(EGDB_NULL_INTERNAL_HANDLE);

	return(0);
}


extern "C" int __stdcall egdb_open(char *options,
			int cache_mb,
			char *directory,
			char *filename)
{
	int i, max_pieces;
	egdb_interface::EGDB_TYPE egdb_type;
	FILE *fp;

	drivers[0].msg_fn = msg_fn0;
	drivers[1].msg_fn = msg_fn1;
	drivers[2].msg_fn = msg_fn2;
	drivers[3].msg_fn = msg_fn3;

	/* Find an open entry in handle[]. */
	for (i = 0; i < ARRAY_SIZE(drivers); ++i)
		if (drivers[i].db.handle == nullptr)
			break;

	if (i >= ARRAY_SIZE(drivers))
		return(EGDB_DRIVER_TABLE_FULL);

	strcpy(drivers[i].filename, filename);

	/* Clear the log file. */
	if (strlen(filename) > 0) {
		fp = fopen(filename, "w");
		if (!fp)
			return(EGDB_FOPEN_FAIL);
		fclose(fp);
	}

	drivers[i].db.handle = egdb_interface::egdb_open(options, cache_mb, directory, drivers[i].msg_fn);
	if (drivers[i].db.handle) {
		int result;

		result = egdb_interface::egdb_identify(directory, &egdb_type, &max_pieces);
		if (result)
			return(EGDB_IDENTIFY_FAIL);

		if (egdb_type == egdb_interface::EGDB_WLD_TUN_V2)
			drivers[i].db.egdb_excludes_some_nonside_caps = 1;
		else
			drivers[i].db.egdb_excludes_some_nonside_caps = 0;
		drivers[i].db.dbpieces = max_pieces;
		drivers[i].db.dbpieces_1side = 5;
		drivers[i].db.db9_kings = 0;
		drivers[i].db.db8_kings_1side = 5;
	}
	else
		return(EGDB_OPEN_FAIL);

	return(i);
}


extern "C" int __stdcall egdb_close(int handle)
{
	int result;

	result = check_handle(handle);
	if (result)
		return(result);

	result = egdb_interface::egdb_close(drivers[handle].db.handle);
	drivers[handle].db.handle = nullptr;
	drivers[handle].filename[0] = 0;

	return(result);
}


extern "C" int __stdcall egdb_identify(char *dir, int *egdb_type, int *max_pieces)
{
	egdb_interface::EGDB_TYPE db_type;
	int result;

	result = egdb_identify(dir, &db_type, max_pieces);
	*egdb_type = db_type;
	if (result)
		return(EGDB_IDENTIFY_FAIL);
	else
		return(0);
}


extern "C" int __stdcall egdb_lookup_fen(int handle, char *fen, int cl)
{
	int color, result;
	egdb_interface::BOARD board;

	result = check_handle(handle);
	if (result)
		return(result);

	result = parse_fen(fen, &board, &color);
	if (result)
		return(EGDB_FEN_ERROR);

	result = egdb_interface::egdb_lookup(drivers[handle].db.handle, (egdb_interface::EGDB_POSITION *)&board, color, cl);
	return(result);
}


extern "C" int __stdcall egdb_lookup_fen_with_search(int handle, char *fen)
{
	int color, result;
	egdb_interface::BOARD board;

	result = check_handle(handle);
	if (result)
		return(result);

	result = parse_fen(fen, &board, &color);
	if (result)
		return(EGDB_FEN_ERROR);

	result = drivers[handle].db.lookup_with_search(&board, color, 64, false);
	return(result);
}


extern "C" int __stdcall egdb_lookup_with_search(int handle, egdb_interface::BOARD *board, int color)
{
	int result;

	result = check_handle(handle);
	if (result)
		return(result);

	result = drivers[handle].db.lookup_with_search(board, color, 64, false);
	return(result);
}


extern "C" int __stdcall get_movelist(egdb_interface::BOARD *board, int color, egdb_interface::BOARD *ml)
{
	int len;
	egdb_interface::MOVELIST movelist;

	len = build_movelist(board, color, &movelist);
	memcpy(ml, movelist.board, len * sizeof(egdb_interface::BOARD));
	return(len);
}


extern "C" int16_t __stdcall is_capture(egdb_interface::BOARD *board, int color)
{
	int value;

	value = egdb_interface::canjump(board, color);
	if (value)
		return(-1);
	else
		return(0);
}


extern "C" int64_t __stdcall getdatabasesize_slice(int nbm, int nbk, int nwm, int nwk)
{
	int64_t size;

	size = egdb_interface::getdatabasesize_slice(nbm, nbk, nwm, nwk);
	return(size);
}


extern "C" void __stdcall indextoposition(int64_t index, egdb_interface::BOARD *pos, int nbm, int nbk, int nwm, int nwk)
{
	indextoposition_slice(index, (egdb_interface::EGDB_POSITION *)pos, nbm, nbk, nwm, nwk);
}


extern "C" int64_t __stdcall positiontoindex(egdb_interface::BOARD *pos, int nbm, int nbk, int nwm, int nwk)
{
	return(position_to_index_slice((egdb_interface::EGDB_POSITION *)pos, nbm, nbk, nwm, nwk));
}


extern "C" int16_t __stdcall is_sharp_win(int handle, egdb_interface::BOARD *board, int color, egdb_interface::BOARD *sharp_move_pos)
{
	int i, len, result, wincount;
	egdb_interface::MOVELIST movelist;

	result = check_handle(handle);
	if (result)
		return(result);

	result = drivers[handle].db.lookup_with_search(board, color, 64, false);
	if (result == egdb_interface::EGDB_WIN) {
		len = build_movelist(board, color, &movelist);
		for (i = 0, wincount = 0; i < len; ++i) {
			result = drivers[handle].db.lookup_with_search(movelist.board + i, OTHER_COLOR(color), 64, false);
			if (result == egdb_interface::EGDB_LOSS) {
				++wincount;
				if (wincount > 1)
					return(0);

				*sharp_move_pos = movelist.board[i];
			}
		}
		if (wincount == 1)
			return(-1);
	}
	return(0);
}


extern "C" int __stdcall move_string(egdb_interface::BOARD *last_board, egdb_interface::BOARD *new_board, int color, char *move)
{
	return(print_move(last_board, new_board, color, move));
}


extern "C" int __stdcall positiontofen(egdb_interface::BOARD *board, int color, char *fen)
{
	return(egdb_interface::print_fen(board, color, fen));
}


extern "C" int __stdcall fentoposition(char *fen, egdb_interface::BOARD *pos, int *color)
{
	int status;

	status = egdb_interface::parse_fen(fen, pos, color);
	if (status)
		return(EGDB_FEN_ERROR);

	return(0);
}
