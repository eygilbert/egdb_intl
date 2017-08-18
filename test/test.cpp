#include "../egdb_intl_dll.h"
#include "engine/fen.h"
#include "engine/move.h"
#include "engine/move_api.h"
#include "egdb/egdb_intl.h"
#include "egdb/slice.h"
#include <Windows.h>
#include "atlstr.h"
#include "comutil.h"

int get_rand_int(int min, int max)
{
	int value;
	int range;
	
	range = max - min;
	value = rand() % range;
	value += min;
	return(value);
}


void get_rand_board(int nbm, int nbk, int nwm, int nwk, egdb_interface::BOARD *board)
{
	int i, square;
	egdb_interface::BITBOARD bb;

	board->black = 0;
	board->white = 0;
	board->king = 0;
	for (i = 0; i < nbm; ) {
		square = get_rand_int(1, 45);
		bb = egdb_interface::square0_to_bitboard(square - 1);
		if ((bb & board->black) == 0) {
			board->black |= bb;
			++i;
		}
	}
	for (i = 0; i < nbk; ) {
		square = get_rand_int(1, 50);
		bb = egdb_interface::square0_to_bitboard(square - 1);
		if ((bb & (board->black | board->white)) == 0) {
			board->black |= bb;
			board->king |= bb;
			++i;
		}
	}
	for (i = 0; i < nwm; ) {
		square = get_rand_int(6, 50);
		bb = egdb_interface::square0_to_bitboard(square - 1);
		if ((bb & (board->black | board->white)) == 0) {
			board->white |= bb;
			++i;
		}
	}
	for (i = 0; i < nwk; ) {
		square = get_rand_int(1, 50);
		bb = egdb_interface::square0_to_bitboard(square - 1);
		if ((bb & (board->black | board->white)) == 0) {
			board->white |= bb;
			board->king |= bb;
			++i;
		}
	}
}


void verify_pos(int dbhandle, egdb_interface::BOARD *board, int color)
{
	int i, count, pval, val, bestval;
	egdb_interface::MOVELIST movelist;
	CComBSTR bstrfen;
	char fen[100], pfen[100];

	count = egdb_interface::build_jump_list<false>(board, &movelist, color);
	if (count == 0)
		count = egdb_interface::build_nonjump_list(board, &movelist, color);

	/* Lookup the parent value. */
	egdb_interface::print_fen(board, color, pfen);
	bstrfen = pfen;
	pval = egdb_lookup_fen_with_search(dbhandle, bstrfen);

	/* Lookup each successor. */
	bestval = egdb_interface::EGDB_LOSS;
	for (i = 0; i < count && bestval != egdb_interface::EGDB_WIN; ++i) {
		egdb_interface::print_fen(movelist.board + i, OTHER_COLOR(color), fen);
		bstrfen = fen;
		val = egdb_lookup_fen_with_search(dbhandle, bstrfen);
		switch (val) {
		case egdb_interface::EGDB_WIN:
			break;

		case egdb_interface::EGDB_DRAW:
			bestval = egdb_interface::EGDB_DRAW;
			break;

		case egdb_interface::EGDB_LOSS:
			bestval = egdb_interface::EGDB_WIN;
			break;

		default:
			printf("%s: egdb_lookup_fen_with_search returned value %d\n", fen, val);
			break;
		}
	}

	if (pval != bestval) {
		printf("%s: value mismatch, parent %d, successors %d\n", pfen, pval, bestval);
	}
}


void test_slice(int dbhandle, int nbm, int nbk, int nwm, int nwk)
{
	int i, color;
	egdb_interface::BOARD board;

	printf("testing db%d%d%d%d\n", nbm, nbk, nwm, nwk);
	color = WHITE;
	for (i = 0; i < 10000; ++i, color = OTHER_COLOR(color)) {
		get_rand_board(nbm, nbk, nwm, nwk, &board);
		verify_pos(dbhandle, &board, color);
	}
}


int main(int argc, char *argv[])
{
	int result, egdb_type, max_pieces, handle;

	/* Test egdb_identify(). */
	CComBSTR options("maxpieces=6");
	CComBSTR directory("c:/db_intl/wld_v2");
	CComBSTR filename("");
	result = egdb_identify((BSTR)directory, &egdb_type, &max_pieces);

	printf("result %d, type %d, max pieces %d\n", result, egdb_type, max_pieces);

	handle = egdb_open(options, 1000, (BSTR)directory, filename);
	if (handle < 0)
		printf("%d returned by egdb_open\n", handle);

	for (egdb_interface::Slice first(2), last(7); first != last; first.increment())
		test_slice(handle, first.nbm(), first.nbk(), first.nwm(), first.nwk());

	result = egdb_close(handle);
	if (result != 0)
		printf("%d returned by egdb_close\n", result);
    return 0;
}

