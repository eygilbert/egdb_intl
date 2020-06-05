#include "egdb/wld_search.h"
#include "egdb/egdb_intl.h"
#include "engine/bitcount.h"
#include "engine/board.h"
#include "engine/bool.h"
#include "engine/fen.h"
#include "engine/move_api.h"
#include <cassert>
#include <climits>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace egdb_interface {

bool wld_search::requires_nonside_capture_test(BOARD *p)
{
	int npieces, nkings;

	if (egdb_excludes_some_nonside_caps == 0)
		return(false);

	npieces = bitcount64(p->black | p->white);
	if (npieces <= 6)
		return(false);
	nkings = bitcount64(p->king);
	if (npieces < 9 && nkings <= 1)
		return(false);
	return(true);
}


/*
 * Return true if a db lookup is possible based only on the number of men and kings present
 * (ignoring capture restrictions).
 */
bool wld_search::is_lookup_possible_pieces(MATERIAL *mat)
{
	if (mat->npieces <= dbpieces && mat->npieces_1_side <= dbpieces_1side)
		return(true);
	else
		return(false);
}


/*
 * Use this function when you already have qualified the positions with is_lookup_possible_pieces().
 */
bool wld_search::is_lookup_possible_with_nonside_caps(MATERIAL *mat)
{
	assert(wld_search::is_lookup_possible_pieces(mat));
	if (egdb_excludes_some_nonside_caps == 0 || mat->npieces < 7)
		return(true);

	if (mat->nbk + mat->nwk <= 1)
		return(true);

	return(false);
}


bool wld_search::is_lookup_possible(BOARD *board, int color, MATERIAL *mat)
{
	if (!is_lookup_possible_pieces(mat))
		return(false);

	if (is_lookup_possible_with_nonside_caps(mat))
		return(true);

	if (!canjump(board, OTHER_COLOR(color)))
		return(true);

	return(false);
}


int wld_search::negate(int value)
{
	if (value == EGDB_WIN)
		return(EGDB_LOSS);
	if (value == EGDB_DRAW)
		return(EGDB_DRAW);
	if (value == EGDB_LOSS)
		return(EGDB_WIN);
	if (value == EGDB_WIN_OR_DRAW)
		return(EGDB_DRAW_OR_LOSS);
	if (value == EGDB_DRAW_OR_LOSS)
		return(EGDB_WIN_OR_DRAW);

	assert(value == EGDB_UNKNOWN || value == EGDB_SUBDB_UNAVAILABLE);
	return(value);
}


bool wld_search::is_greater_or_equal(int left, int right)
{
	if (right == EGDB_WIN) {
		if (left == EGDB_WIN)
			return(true);
		else
			return(false);
	}
	if (right == EGDB_DRAW) {
		if (left == EGDB_DRAW || left == EGDB_WIN_OR_DRAW || left == EGDB_WIN)
			return(true);
		else
			return(false);
	}
	if (right == EGDB_LOSS)
		return(true);
	if (right == EGDB_WIN_OR_DRAW) {
		if (left == EGDB_WIN_OR_DRAW || left == EGDB_WIN)
			return(true);
		else
			return(false);
	}
	if (right == EGDB_DRAW_OR_LOSS) {
		if (left == EGDB_WIN || left == EGDB_WIN_OR_DRAW || left == EGDB_DRAW || left == EGDB_DRAW_OR_LOSS)
			return(true);
		else
			return(false);
	}
	if (right == EGDB_UNKNOWN || right == EGDB_SUBDB_UNAVAILABLE) {
		if (left == EGDB_LOSS)
			return(false);
		else
			return(true);
	}

	assert(false);
	return(true);
}


bool wld_search::is_greater(int left, int right)
{
	if (right == EGDB_WIN) {
		return(false);
	}
	if (right == EGDB_DRAW) {
		if (left == EGDB_WIN_OR_DRAW || left == EGDB_WIN)
			return(true);
		else
			return(false);
	}
	if (right == EGDB_LOSS) {
		if (left == EGDB_LOSS)
			return(false);
		else
			return(true);
	}
	if (right == EGDB_WIN_OR_DRAW) {
		if (left == EGDB_WIN)
			return(true);
		else
			return(false);
	}
	if (right == EGDB_DRAW_OR_LOSS) {
		if (left == EGDB_LOSS)
			return(false);
		else
			return(true);
	}

	assert(right == EGDB_UNKNOWN || right == EGDB_SUBDB_UNAVAILABLE);
	if (left == EGDB_LOSS || left == EGDB_UNKNOWN || left == EGDB_SUBDB_UNAVAILABLE)
		return(false);

	return(true);
}


int wld_search::bestvalue_improve(int value, int bestvalue)
{
	if (bestvalue == EGDB_WIN) {
		return(EGDB_WIN);
	}
	if (bestvalue == EGDB_DRAW) {
		if (value == EGDB_WIN_OR_DRAW || value == EGDB_WIN)
			return(value);
		if (value == EGDB_UNKNOWN || value == EGDB_SUBDB_UNAVAILABLE)
			return(EGDB_WIN_OR_DRAW);
		return(bestvalue);
	}
	if (bestvalue == EGDB_LOSS)
		return(value);
	if (bestvalue == EGDB_WIN_OR_DRAW) {
		if (value == EGDB_WIN)
			return(value);
		else
			return(bestvalue);
	}
	if (bestvalue == EGDB_DRAW_OR_LOSS) {
		if (value == EGDB_LOSS)
			return(bestvalue);
		else
			return(value);
	}
	if (bestvalue == EGDB_UNKNOWN || bestvalue == EGDB_SUBDB_UNAVAILABLE) {
		if (value == EGDB_WIN_OR_DRAW || value == EGDB_WIN)
			return(value);
		if (value == EGDB_DRAW)
			return(EGDB_WIN_OR_DRAW);
	}
	return(bestvalue);
}


bool is_repetition(BOARD *history, BOARD *p, int depth)
{
	int i;
	BITBOARD men, earlier_men;

	men = (p->black | p->white) & ~p->king;
	for (i = depth - 4; i >= 0; i -= 2) {

		/* Stop searching if a man (not king) was moved of either color. */
		earlier_men = (history[i].black | history[i].white) & ~history[i].king;
		if (men ^ earlier_men)
			break;

		if (!std::memcmp(p, history + i, sizeof(*p)))
			return(true);
	}

	return(false);
}

/*
 * Caution: this function is not thread-safe.
 */
int wld_search::lookup_with_rep_check(BOARD *p, int color, int depth, int maxdepth, int alpha, int beta, bool force_root_search)
{
	int i;
	int movecount;
	int side_capture;
	int value, bestvalue;
	MOVELIST movelist;

	rep_check_positions[depth] = *p;
	++nodes;
	if (maxnodes && (nodes >= maxnodes))
		longjmp(env, 1);

	if (timeout && (nodes & 63) == 63) {
		clock_t t1 = clock();
		if ((t1 - t0) > timeout)
			longjmp(env, 1);
	}

	/* Check for one side with no pieces. */
	if (p->black == 0) {
		if (color == BLACK)
			return(EGDB_LOSS);
		else
			return(EGDB_WIN);
	}
	if (p->white == 0) {
		if (color == BLACK)
			return(EGDB_WIN);
		else
			return(EGDB_LOSS);
	}

	/* Check for draw by repetition. */
	if (is_repetition(rep_check_positions, p, depth))
		return(EGDB_DRAW);

	side_capture = 0;
	if (canjump(p, color) == 0) {
		if (depth != 0 || !force_root_search) {
			if (!requires_nonside_capture_test(p))
				return(egdb_lookup(handle, (EGDB_POSITION *)p, color, 0));
			if (canjump(p, OTHER_COLOR(color)) == 0) {
				value = egdb_lookup(handle, (EGDB_POSITION *)p, color, 0);
				if (value != EGDB_UNKNOWN)
					return(value);
			}
		}
	}
	else
		side_capture = 1;

	if (depth > maxdepth_reached)
		maxdepth_reached = depth;

	if (depth >= maxdepth) {
		return(EGDB_UNKNOWN);
	}

	/* We have a capture.  Have to do a search. */
	if (side_capture)
		movecount = build_jump_list<false>(p, &movelist, color);
	else
		movecount = build_nonjump_list(p, &movelist, color);

	/* If no moves then its a loss. */
	if (movecount == 0) {
		return(EGDB_LOSS);
	}

	/* look up all successors. */
	bestvalue = EGDB_LOSS;
	for (i = 0; i < movecount; ++i) {
		value = negate(lookup_with_rep_check(movelist.board + i, OTHER_COLOR(color), depth + 1, maxdepth, negate(beta), negate(alpha), force_root_search));
		log_tree(value, depth + 1, color);
		if (is_greater_or_equal(value, beta))
			return(bestvalue_improve(value, beta));

		bestvalue = bestvalue_improve(value, bestvalue);
		if (is_greater(bestvalue, alpha))
			alpha = bestvalue;
	}
	return(bestvalue);
}


/*
 * Lookup the value of a position in the db.
 * It may require a search if the position is a capture,
 * or if the position has a non-side capture and this slice of the db doesn't store those positions,
 * or if the function is called with force_root_search set true. This is used for self-verification. It
 * forces the value to be computed from the successor positions even if a direct lookup of the 
 * root position is possible.
 * Because some slices have neither positions with captures nor non-side captures, it is possible that the search has to go quite 
 * deep to resolve the root value, and can take a long time. There are 2 limits to control this. The maxdepth argument
 * limits the depth of the lookup search, and there is also a maxnodes limit. If either of these limits are hit,
 * the search returns the value EGDB_UNKNOWN.
 */
int wld_search::lookup_with_search(BOARD *p, int color, bool force_root_search)
{
	int status, i;
	int value = EGDB_UNKNOWN;	// prevent using unitialized memory

	nodes = 0;
	t0 = clock();
	reset_maxdepth();

	status = setjmp(env);
	if (status) {
		char buf[150];

		print_fen(p, color, buf);
		std::printf("wld lookup max node count or time limit exceeded, nodes %d, time %.3f sec, (not an error): %s\n", 
					nodes, (clock() - t0) / 1000.00, buf);
		return(EGDB_UNKNOWN);
	}

	for (i = 1; i < maxrepdepth; ++i) {
		value = lookup_with_rep_check(p, color, 0, i, EGDB_LOSS, EGDB_WIN, force_root_search);
		switch (value) {
		case EGDB_WIN:
		case EGDB_DRAW:
		case EGDB_LOSS:
			return(value);
		}
	}

	return(value);
}


void wld_search::log_tree(int value, int depth, int color)
{
#if 0
	int i;
	bool white_is_even;
	char buf[50];

	if (color == WHITE)
		if (depth & 1)
			white_is_even = false;
		else
			white_is_even = true;
	else
		if (depth & 1)
			white_is_even = true;
		else
			white_is_even = false;

	for (i = 1; i <= depth; ++i) {
		if (white_is_even) {
			if (i & 1)
				color = BLACK;
			else
				color = WHITE;
		}
		else {
			if (i & 1)
				color = WHITE;
			else
				color = BLACK;
		}

		print_move(rep_check_positions + i - 1, rep_check_positions + i, color, buf);
		std::printf("%s, ", buf);
	}

	std::printf("value %d, depth %d\n", value, depth);
#endif
}

}	// namespace
