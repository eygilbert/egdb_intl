#include "../egdb_intl_dll.h"
#include "egdb/distance.h"
#include "egdb/egdb_intl.h"
#include "egdb/platform.h"
#include "egdb/slice.h"
#include "engine/bitcount.h"
#include "engine/fen.h"
#include "engine/move.h"
#include "engine/move_api.h"
#include "engine/project.h"
#include <stdint.h>
#include <algorithm>
#include <vector>

using namespace egdb_interface;

const char *wld_path = "c:/db_intl/wld_v2";
const char *dtw_path = "c:/db_intl/dtw";
const char *mtc_path = "c:/db_intl/mtc";


/*
 * Verify that the function names are not decorated.
 */
void test_names()
{
	HMODULE handle;
	void *fnptr;

#ifdef ENVIRONMENT64
	handle = LoadLibrary("egdb_intl64");
#else
	handle = LoadLibrary("egdb_intl32");
#endif
	fnptr = GetProcAddress(handle, "egdb_identify");
	if (fnptr == nullptr) {
		printf("GetProcAddress failed in test_names.\n");
		exit(1);
	}
	FreeLibrary(handle);
}


void print_msgs(char const *msg)
{
	std::printf("%s", msg);
}


int get_rand_int(int min, int max)
{
	int value;
	int range;
	
	range = max - min;
	value = rand() % range;
	value += min;
	return(value);
}


void get_rand_board(int nbm, int nbk, int nwm, int nwk, BOARD *board)
{
	int i, square;
	BITBOARD bb;

	board->black = 0;
	board->white = 0;
	board->king = 0;
	for (i = 0; i < nbm; ) {
		square = get_rand_int(1, 45);
		bb = square0_to_bitboard(square - 1);
		if ((bb & board->black) == 0) {
			board->black |= bb;
			++i;
		}
	}
	for (i = 0; i < nbk; ) {
		square = get_rand_int(1, 50);
		bb = square0_to_bitboard(square - 1);
		if ((bb & (board->black | board->white)) == 0) {
			board->black |= bb;
			board->king |= bb;
			++i;
		}
	}
	for (i = 0; i < nwm; ) {
		square = get_rand_int(6, 50);
		bb = square0_to_bitboard(square - 1);
		if ((bb & (board->black | board->white)) == 0) {
			board->white |= bb;
			++i;
		}
	}
	for (i = 0; i < nwk; ) {
		square = get_rand_int(1, 50);
		bb = square0_to_bitboard(square - 1);
		if ((bb & (board->black | board->white)) == 0) {
			board->white |= bb;
			board->king |= bb;
			++i;
		}
	}
}


void verify_pos(int dbhandle, BOARD *board, int color)
{
	int i, pval, val, bestval;
	MOVELIST movelist;
	char fen[100], pfen[100];

	build_movelist(board, color, &movelist);

	/* Lookup the parent value. */
	print_fen(board, color, pfen);
	pval = egdb_lookup_fen_with_search(dbhandle, pfen);

	/* Lookup each successor. */
	bestval = EGDB_LOSS;
	for (i = 0; i < movelist.count && bestval != EGDB_WIN; ++i) {
		print_fen(movelist.board + i, OTHER_COLOR(color), fen);
		val = egdb_lookup_fen_with_search(dbhandle, fen);
		switch (val) {
		case EGDB_WIN:
			break;

		case EGDB_DRAW:
			bestval = EGDB_DRAW;
			break;

		case EGDB_LOSS:
			bestval = EGDB_WIN;
			break;

		default:
			printf("%s: egdb_lookup_fen_with_search returned value %d\n", fen, val);
			break;
		}
	}

	if (pval != bestval) {
		printf("%s: value mismatch, parent %d, successors %d\n", pfen, pval, bestval);
	}

	/* Test dll function egdb_lookup() if possible. */
	if (!is_capture(board, color)) {
		int npieces = bitcount64(board->black | board->white);
		if (npieces < 7 || !is_capture(board, OTHER_COLOR(color))) {
			int value = egdb_lookup(dbhandle, board, color, 0);
			if (value > 0) {
				if (value != pval) {
					printf("%s: egdb_lookup returned value %d, expected %d\n", pfen, value, pval);
					exit(1);
				}
				value = egdb_lookup_fen(dbhandle, pfen, 0);
				if (value != pval) {
					printf("%s: egdb_lookup_fen returned value %d, expected %d\n", pfen, value, pval);
					exit(1);
				}
			}
		}
	}
}


void test_slice(int dbhandle, int nbm, int nbk, int nwm, int nwk)
{
	int i, color;
	BOARD board;

	printf("testing db%d%d%d%d\n", nbm, nbk, nwm, nwk);
	color = WHITE;
	for (i = 0; i < 10000; ++i, color = OTHER_COLOR(color)) {
		get_rand_board(nbm, nbk, nwm, nwk, &board);
		verify_pos(dbhandle, &board, color);
	}
}


BOARD get_next_pos(BOARD &pos, int color, MOVELIST &movelist, const char *movestr)
{
	int from, to, mlfrom, mlto;
	BOARD retboard;
	sscanf(movestr, "%d%*c%d", &from, &to);

	for (int i = 0; i < movelist.count; ++i) {
		get_fromto(&pos, movelist.board + i, color, &mlfrom, &mlto);
		if (from == 1 + mlfrom && to == 1 + mlto)
			return(movelist.board[i]);
	}
	memset(&retboard, 0, sizeof(retboard));
	return(retboard);
}


void dtw_test(Slice &slice, int wld_handle, int dtw_handle)
{
	int64_t size, index, incr, max_lookups;
	int color, status, return_size;
	BOARD pos;
	std::string fenstr;
	char moves[MAXMOVES][20];
	int distances[MAXMOVES];

	max_lookups = 30;
	size = getdatabasesize_slice(slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
	if (max_lookups < 1)
		incr = 1;
	else
		incr = (std::max)(size / max_lookups, (int64_t)1);

	printf("Testing slice db%d-%d%d%d%d\n", slice.npieces(), slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
	color = BLACK;
	for (index = 0; index < size; index += incr) {
		int plies;
		int value;
		int64_t sidx;

		for (sidx = index; sidx < size; ++sidx) {
			indextoposition(sidx, &pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
			value = egdb_lookup_with_search(wld_handle, &pos, color);
			if (value == EGDB_WIN || value == EGDB_LOSS)
				break;
		}
		if (sidx >= size)
			break;

		print_fen(&pos, color, fenstr);
		status = egdb_lookup_distance(wld_handle, dtw_handle, fenstr.c_str(), &return_size, distances, moves);
		if (status < 0 || return_size == 0) {
			printf("dtw lookup timed out, status %d (not a bug)\n", status);
			continue;
		}

		if (distances[0] < 2)
			continue;

		plies = distances[0];
		while (plies > 1) {
			MOVELIST movelist;
			BOARD next;

			build_movelist(&pos, color, &movelist);

			/* Lookup dtw of the first best move. */
			next = get_next_pos(pos, color, movelist, moves[0]);
			color = OTHER_COLOR(color);
			print_fen(&next, color, fenstr);
			status = egdb_lookup_distance(wld_handle, dtw_handle, fenstr.c_str(), &return_size, distances, moves);
			if (status < 0 || return_size == 0) {
				printf("dtw lookup timed out, status %d (not a bug)\n", status);
				break;
			}

			if (distances[0] != plies - 1) {
				printf("dtw depth error, expected %d, got %d\n", plies - 1, distances[0]);
				exit(1);
			}
			--plies;
			pos = next;
		}
	}
}


void dtw_test(int wld_handle)
{
	int status, max_pieces;
	int type;
	Slice slice(2);
	int dtw_handle;
	const int most_pieces = 7;

	std::printf("\nDTW test.\n");
	status = egdb_identify(dtw_path, &type, &max_pieces);
	if (status) {
		std::printf("DTW db not found at %s\n", dtw_path);
		std::exit(1);
	}
	if (type != EGDB_DTW) {
		std::printf("Wrong db type, not DTW.\n");
		std::exit(1);
	}
	if (max_pieces < most_pieces) {
		std::printf("Need %d pieces DTW for test, only found %d.\n", most_pieces, max_pieces);
		std::exit(1);
	}

	dtw_handle = egdb_open("maxpieces=7", 10, dtw_path, "");
	if (!dtw_handle) {
		std::printf("Cannot open DTW db at %s\n", dtw_path);
		std::exit(1);
	}
	for (slice = Slice(2); slice.npieces() <= most_pieces; slice.increment()) {
		dtw_test(slice, wld_handle, dtw_handle);
	}

	egdb_close(dtw_handle);
}


void test_best_mtc_successor(int wld_handle, int mtc_handle, BOARD &pos, int color)
{
	int status, return_size;
	int plies;
	MOVELIST movelist;
	int distances[MAXMOVES];
	char moves[MAXMOVES][20];
	std::string fenstr;

	build_movelist(&pos, color, &movelist);
	print_fen(&pos, color, fenstr);
	status = egdb_lookup_distance(wld_handle, mtc_handle, fenstr.c_str(), &return_size, distances, moves);

	if (status < 0 || !return_size) {
		printf("mtc_probe timed out (not a bug)\n");
		return;
	}

	plies = distances[0];
	while (plies > 10) {
		MOVELIST movelist;
		BOARD next;

		build_movelist(&pos, color, &movelist);

		/* Lookup dtc of the first best move. */
		next = get_next_pos(pos, color, movelist, moves[0]);
		color = OTHER_COLOR(color);
		print_fen(&next, color, fenstr);
		status = egdb_lookup_distance(wld_handle, mtc_handle, fenstr.c_str(), &return_size, distances, moves);
		if (status < 0 || return_size == 0) {
			printf("mtc lookup timed out, status %d (not a bug)\n", status);
			break;
		}
		if (distances[0] != plies - 1) {
			printf("mtc depth error, expected %d, got %d\n", plies - 1, distances[0]);
			exit(1);
		}
		--plies;
		pos = next;
	}
}


/*
* Read a file of the highest MTC positions in each db subdivision.
* Lookup the MTC value of that position, and then descend into one of the
* best successor positions. Keep going until there are no successors with
* MTC values >= 12. Make sure that each position has a successor with either
* the same MTC value or a value that is smaller by 2. MTC values are always even
* because the db stores the real MTC value divided by 2.
*/
void mtc_test()
{
	int color, status, max_pieces, value;
	FILE *fp;
	int type;
	char const *mtc_stats_filename = "test/10x10 HighMtc.txt";
	char linebuf[120];
	BOARD pos;

	std::printf("\nMTC test.\n");
	status = egdb_identify(mtc_path, &type, &max_pieces);
	if (status) {
		std::printf("MTC db not found at %s\n", mtc_path);
		std::exit(1);
	}
	if (type != EGDB_MTC_RUNLEN) {
		std::printf("Wrong db type, not MTC.\n");
		std::exit(1);
	}
	if (max_pieces != 8) {
		std::printf("Need 8 pieces MTC for test, only found %d.\n", max_pieces);
		std::exit(1);
	}

	/* Open the endgame db drivers. */
	int wld_handle = egdb_open("maxpieces=8", 1500, wld_path, "");
	if (wld_handle < 0) {
		std::printf("Cannot open tun v2 db\n");
		std::exit(1);
	}

	int mtc_handle = egdb_open("maxpieces=8", 0, mtc_path, "");
	if (mtc_handle < 0) {
		std::printf("Cannot open MTC db\n");
		std::exit(1);
	}
	fp = std::fopen(mtc_stats_filename, "r");
	if (!fp) {
		std::printf("Cannot open %s for reading\n", mtc_stats_filename);
		std::exit(1);
	}

	for (int count = 0; ; ) {
		if (!std::fgets(linebuf, ARRAY_SIZE(linebuf), fp))
			break;
		if (parse_fen(linebuf, &pos, &color))
			continue;

		/* For 7 and 8 pieces, only test every 128th position, to speed up the test. */
		++count;
		if (((count % 32) != 0) && (bitcount64(pos.black | pos.white) >= 7))
			continue;

		print_fen(&pos, color, linebuf);
		std::printf("%s ", linebuf);
		value = egdb_lookup_fen(mtc_handle, linebuf, 0);
		if (value >= 12)
			test_best_mtc_successor(wld_handle, mtc_handle, pos, color);

		std::printf("mtc %d\n", value);
	}
	egdb_close(mtc_handle);
	egdb_close(wld_handle);
}


void test_fen()
{
	const char *fentbl[] = {
		"W:W31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50:B1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20",
		"W:WK36,K37,K48,49,K50:BK1,K2,K3,9,10,11,12"
	};
	BOARD board;
	int color;
	std::string fenstr;

	for (int i = 0; i < ARRAY_SIZE(fentbl); ++i) {
		parse_fen(fentbl[i], &board, &color);
		print_fen(&board, color, fenstr);
		printf("%s\n", fenstr.c_str());
	}
}


int main(int argc, char *argv[])
{
	int result, egdb_type, max_pieces, handle;

	init_bitcount();
	test_names();
	test_fen();
	mtc_test();

	/* Test egdb_identify(). */
	result = egdb_identify(wld_path, &egdb_type, &max_pieces);

	printf("result %d, type %d, max pieces %d\n", result, egdb_type, max_pieces);

	handle = egdb_open("maxpieces=8", 1000, wld_path, "");
	if (handle < 0)
		printf("%d returned by egdb_open\n", handle);

	dtw_test(handle);

	for (Slice first(2), last(9); first != last; first.increment())
		test_slice(handle, first.nbm(), first.nbk(), first.nwm(), first.nwk());

	result = egdb_close(handle);
	if (result != 0)
		printf("%d returned by egdb_close\n", result);
    return 0;
}

