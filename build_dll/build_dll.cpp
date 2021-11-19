#include "engine/project.h"
#include "engine/board.h"
#include "engine/move_api.h"
#include "engine/fen.h"
#include "engine/print_move.h"
#include "egdb/dtw_search.h"
#include "egdb/egdb_intl.h"
#include "egdb/mtc_probe.h"
#include "egdb/wld_search.h"
#include "builddb/indexing.h"
#include "../egdb_intl_dll.h"
#include <stdio.h>

using namespace egdb_dll;
using namespace egdb_interface;

typedef struct {
	char filename[260];
	void (*msg_fn)(char const *msg);
	egdb_info db;
	wld_search wld;
} DRIVER_INFO;

DRIVER_INFO drivers[4];

int lookup_dtw(int handle_wld, int handle_dist, BOARD *board, int color, int *distances, char moves[maxmoves][maxmovelen]);
int lookup_mtc(int handle_wld, int handle_dist, BOARD *board, int color, int *distances, char moves[maxmoves][maxmovelen]);


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


extern "C" int __stdcall egdb_open(char *options, int cache_mb, const char *directory, const char *filename)
{
	int i, max_pieces, max_pieces_1side;
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

	drivers[i].db.handle = egdb_open(options, cache_mb, directory, drivers[i].msg_fn);
	if (drivers[i].db.handle) {
		int result;

		result = egdb_get_pieces(drivers[i].db.handle, &max_pieces, &max_pieces_1side);
		if (result)
			return(EGDB_FOPEN_FAIL);

		drivers[i].db.dbtype = egdb_get_type(drivers[i].db.handle);
		drivers[i].db.max_pieces = max_pieces;
		if (is_wld(drivers[i].db.handle)) {
			drivers[i].wld = wld_search(drivers[i].db.handle);
			drivers[i].wld.set_search_timeout(2000);
		}
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

	result = egdb_close(drivers[handle].db.handle);
	drivers[handle].db.handle = nullptr;
	drivers[handle].filename[0] = 0;
	drivers[handle].wld.clear();

	return(result);
}


extern "C" int __stdcall egdb_identify(const char *dir, int *egdb_type, int *max_pieces)
{
	egdb_interface::EGDB_TYPE db_type;
	int result;

	result = egdb_interface::egdb_identify(dir, &db_type, max_pieces);
	*egdb_type = db_type;
	if (result)
		return(EGDB_IDENTIFY_FAIL);
	else
		return(0);
}


extern "C" int __stdcall egdb_lookup(int handle, Position *board, int color, int cl)
{
	int result;

	result = check_handle(handle);
	if (result)
		return(result);

	result = egdb_lookup(drivers[handle].db.handle, (EGDB_POSITION *)board, color, cl);
	return(result);
}


extern "C" int __stdcall egdb_lookup_fen(int handle, char *fen, int cl)
{
	int color, result;
	BOARD board;

	result = check_handle(handle);
	if (result)
		return(result);

	result = parse_fen(fen, &board, &color);
	if (result)
		return(EGDB_FEN_ERROR);

	result = egdb_lookup(drivers[handle].db.handle, (EGDB_POSITION *)&board, color, cl);
	return(result);
}


extern "C" int __stdcall egdb_lookup_fen_with_search(int handle, char *fen)
{
	int color, result;
	BOARD board;

	result = check_handle(handle);
	if (result)
		return(result);

	if (!is_wld(drivers[handle].db.handle))
		return(EGDB_NOT_WLD_TYPE);

	result = parse_fen(fen, &board, &color);
	if (result)
		return(EGDB_FEN_ERROR);

	result = drivers[handle].wld.lookup_with_search(&board, color, false);
	return(result);
}


extern "C" int __stdcall egdb_lookup_with_search(int handle, Position *pos, int color)
{
	int result;

	result = check_handle(handle);
	if (result)
		return(result);

	if (!is_wld(drivers[handle].db.handle))
		return(EGDB_NOT_WLD_TYPE);

	result = drivers[handle].wld.lookup_with_search((BOARD *)pos, color, false);
	return(result);
}


extern "C" int __stdcall egdb_lookup_distance(int handle_wld, int handle_dist, const char *fen, 
											int distances[maxmoves], char moves[maxmoves][maxmovelen])
{
	int color, result, npieces;
	BOARD board;

	result = check_handle(handle_wld);
	if (result)
		return(result);

	if (!is_wld(drivers[handle_wld].db.handle))
		return(EGDB_NOT_WLD_TYPE);

	result = check_handle(handle_dist);
	if (result)
		return(result);

	result = parse_fen(fen, &board, &color);
	if (result)
		return(EGDB_FEN_ERROR);

	npieces = bitcount64(board.black | board.white);
	if (npieces > drivers[handle_wld].db.max_pieces || npieces > drivers[handle_dist].db.max_pieces)
		return(EGDB_LOOKUP_NOT_POSSIBLE);

	if (is_dtw(drivers[handle_dist].db.handle)) {
		return(lookup_dtw(handle_wld, handle_dist, &board, color, distances, moves));
		
	}
	else if (is_mtc(drivers[handle_dist].db.handle)) {
		return(lookup_mtc(handle_wld, handle_dist, &board, color, distances, moves));
	}
	else
		return(EGDB_NOT_DISTANCE_TYPE);

}


extern "C" int __stdcall get_movelist(Position *pos, int color, Position *ml)
{
	MOVELIST movelist;

	build_movelist((BOARD *)pos, color, &movelist);
	memcpy(ml, movelist.board, movelist.count * sizeof(Position));
	return(movelist.count);
}


extern "C" int16_t __stdcall is_capture(Position *pos, int color)
{
	int value;

	value = canjump((BOARD *)pos, color);
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


extern "C" void __stdcall indextoposition(int64_t index, Position *pos, int nbm, int nbk, int nwm, int nwk)
{
	indextoposition_slice(index, (EGDB_POSITION *)pos, nbm, nbk, nwm, nwk);
}


extern "C" int64_t __stdcall positiontoindex(Position *pos, int nbm, int nbk, int nwm, int nwk)
{
	return(position_to_index_slice((EGDB_POSITION *)pos, nbm, nbk, nwm, nwk));
}


extern "C" int32_t __stdcall sharp_status(int handle, Position *pos, int color, Position *sharp_move_pos)
{
	int i, len, result, wincount;
	MOVELIST movelist;

	result = check_handle(handle);
	if (result)
		return(result);

	if (!is_wld(drivers[handle].db.handle))
		return(EGDB_NOT_WLD_TYPE);

	result = drivers[handle].wld.lookup_with_search((BOARD *)pos, color, false);
	if (result == egdb_dll::EGDB_WIN) {
		len = build_movelist((BOARD *)pos, color, &movelist);
		for (i = 0, wincount = 0; i < len; ++i) {
			result = drivers[handle].wld.lookup_with_search(movelist.board + i, OTHER_COLOR(color), false);
			if (result == egdb_dll::EGDB_LOSS) {
				++wincount;
				if (wincount > 1)
					return(EGDB_SHARP_STAT_FALSE);

				memcpy(sharp_move_pos, &movelist.board[i], sizeof(Position));
			}
			else if (result == egdb_dll::EGDB_UNKNOWN)
				return(EGDB_SHARP_STAT_UNKNOWN);
		}
		if (wincount == 1)
			return(EGDB_SHARP_STAT_TRUE);
		else
			return(EGDB_SHARP_STAT_FALSE);
	}
	else if (result == egdb_dll::EGDB_UNKNOWN)
		return(EGDB_SHARP_STAT_UNKNOWN);
	else
		return(EGDB_SHARP_STAT_FALSE);
}


extern "C" int __stdcall move_string(Position *last_pos, Position *new_pos, int color, char *move)
{
	return(print_move((BOARD *)last_pos, (BOARD *)new_pos, color, move));
}


extern "C" int __stdcall capture_path(Position *last_board, Position *new_board, int color, char *landed, char *captured)
{
	int i, nmoves, nmatches, matchi;
	int fromsq, tosq;
	BITBOARD jumped;
	MOVELIST movelist;
	CAPTURE_INFO path[MAXMOVES];

	if (!canjump((BOARD *)last_board, color))
		return(0);

	nmatches = 0;
	nmoves = build_movelist_path((BOARD *)last_board, color, &movelist, path);
	for (i = 0; i < nmoves; ++i) {
		if (memcmp((BOARD *)new_board, movelist.board + i, sizeof(BOARD)) == 0) {
			++nmatches;
			matchi = i;
		}
	}

	if (nmatches != 1)
		return(0);

	get_fromto(&path[matchi].path[0], &path[matchi].path[1], color, &fromsq, &tosq);
	landed[0] = fromsq + 1;
	for (i = 0; i < path[matchi].capture_count; ++i) {
		get_fromto(&path[matchi].path[i], &path[matchi].path[i + 1], color, &fromsq, &tosq);
		landed[i + 1] = tosq + 1;
		get_jumped_square(i, matchi, color, path, &jumped);
		captured[i] = bitboard_to_square(jumped);
	}
	return(path[matchi].capture_count);
}


extern "C" int __stdcall positiontofen(Position *pos, int color, char fen[maxfenlen])
{
	return(print_fen((BOARD *)pos, color, fen));
}


extern "C" int __stdcall fentoposition(char *fen, Position *pos, int *color)
{
	int status;

	status = parse_fen(fen, (BOARD *)pos, color);
	if (status)
		return(EGDB_FEN_ERROR);

	return(0);
}


int lookup_dtw(int handle_wld, int handle_dist, BOARD *board, int color, int *distances, char moves[MAXMOVES][maxmovelen])
{
	int wld_value, status;
	dtw_search dtw;
	MOVELIST movelist;
	std::vector<move_distance> dists;

	wld_value = drivers[handle_wld].wld.lookup_with_search(board, color, false);
	switch (wld_value) {
	case egdb_dll::EGDB_WIN:
	case egdb_dll::EGDB_LOSS:
		dtw = dtw_search(drivers[handle_wld].wld, drivers[handle_dist].db.handle);
		dtw.set_search_timeout(2000);
		status = dtw.lookup_with_search(board, color, dists);
		if (status <= 0)
			return(EGDB_LOOKUP_NOT_POSSIBLE);
		if (dists.size() == 0)
			return(EGDB_LOOKUP_NOT_POSSIBLE);
		build_movelist(board, color, &movelist);
		for (size_t i = 0; i < dists.size(); ++i) {
			distances[i] = dists[i].distance;
			print_move(board, movelist.board + dists[i].move, color, moves[i]);
		}
		return((int)dists.size());

	default:
		return(EGDB_LOOKUP_NOT_POSSIBLE);
	}
	return(EGDB_LOOKUP_NOT_POSSIBLE);
}


int lookup_mtc(int handle_wld, int handle_dist, BOARD *board, int color, int *distances, char moves[MAXMOVES][maxmovelen])
{
	int wld_value, status;
	MOVELIST movelist;
	std::vector<move_distance> dists;

	wld_value = drivers[handle_wld].wld.lookup_with_search(board, color, false);
	switch (wld_value) {
	case egdb_dll::EGDB_WIN:
	case egdb_dll::EGDB_LOSS:
		build_movelist(board, color, &movelist);
		status = mtc_probe(drivers[handle_wld].wld, drivers[handle_dist].db.handle, board, color, &movelist, &wld_value, dists);
		if (status == 0)
			return(EGDB_LOOKUP_NOT_POSSIBLE);
		if (dists.size() == 0)
			return(EGDB_LOOKUP_NOT_POSSIBLE);
		for (size_t i = 0; i < dists.size(); ++i) {
			distances[i] = dists[i].distance;
			print_move(board, movelist.board + dists[i].move, color, moves[i]);
		}
		return((int)dists.size());
		return(0);

	default:
		return(EGDB_LOOKUP_NOT_POSSIBLE);
	}
	return(EGDB_LOOKUP_NOT_POSSIBLE);
}

