#include "move.h"

// engine
#include "board.h"
#include "bool.h"
#include "move_api.h"
#include "project.h"

#include <Windows.h>

#include <cstdio>

template <bool save_capture_info>
static void black_man_jump(BOARD *board, BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from, int num_jumps, int *largest_num_jumps, CAPTURE_INFO cap[]);
template <bool save_capture_info>
static void white_man_jump(BOARD *board, BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from, int num_jumps, int *largest_num_jumps, CAPTURE_INFO cap[]);
template <bool save_capture_info>
static void black_king_jump(BOARD *board, BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from, DIR dir,
					 int num_jumps, int *largest_num_jumps, CAPTURE_INFO cap[]);
template <bool save_capture_info>
static void white_king_jump(BOARD *board, BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from, DIR dir,
					 int num_jumps, int *largest_num_jumps, CAPTURE_INFO cap[]);


/*
 *     Bit positions           Square numbers
 *
 *          white                   white
 *
 *    53  52  51  50  49      50  49  48  47  46
 *  48  47  46  45  44      45  44  43  42  41
 *    42  41  40  39  38      40  39  38  37  36
 *  37  36  35  34  33      35  34  33  32  31
 *    31  30  29  28  27      30  29  28  27  26
 *  26  25  24  23  22      25  24  23  22  21
 *    20  19  18  17  16      20  19  18  17  16
 *  15  14  13  12  11      15  14  13  12  11
 *    09  08  07  06  05      10  09  08  07  06
 *  04  03  02  01  00      05  04  03  02  01
 *
 *         black                    black
 */


extern int fwd_shift_counts[2];
extern int back_shift_counts[2];
extern DIAGS diag_tbl[NUM_BITBOARD_BITS];


static inline void assign_black_jump_path(BOARD *board, BITBOARD jumps, CAPTURE_INFO *cap,
							 int capture_count, int movecount)
{
	cap[movecount].path[capture_count] = *board;
	cap[movecount].path[capture_count].white &= ~jumps;
	cap[movecount].path[capture_count].king &= ~jumps;
}


static inline void assign_white_jump_path(BOARD *board, BITBOARD jumps, CAPTURE_INFO *cap,
							 int capture_count, int movecount)
{
	cap[movecount].path[capture_count] = *board;
	cap[movecount].path[capture_count].black &= ~jumps;
	cap[movecount].path[capture_count].king &= ~jumps;
}


static inline void copy_path(CAPTURE_INFO *cap, MOVELIST *movelist)
{
	/* Copy the path to the next potential jump move. */
	memcpy(cap[movelist->count + 1].path, cap[movelist->count].path, sizeof(cap[0].path));
}


int fwd_shift_counts[] = {
	LEFT_FWD_SHIFT, RIGHT_FWD_SHIFT
};

int back_shift_counts[] = {
	LEFT_BACK_SHIFT, RIGHT_BACK_SHIFT
};


/* A table of DIAGS indexed by bitnum. */
DIAGS diag_tbl[NUM_BITBOARD_BITS];

void add_diags(int *len, BITBOARD *table, int x, int y, int xincr, int yincr)
{
	int square0;

	for (*len = 0; ; ++*len) {
		x += xincr;
		y += yincr;
		if ((x >= 0 && x < 10) && (y >= 0 && y < 10)) {

			/* Convert from x,y back to a square number. */
			square0 = 5 * y + x / 2;
			table[*len] = square0_to_bitboard(square0);
		}
		else
			break;
	}
}


void init_move_tables()
{
	int square0;		/* zero based square number. */
	int bitnum;
	int x0, y0;

	for (square0 = 0; square0 < NUMSQUARES; ++square0) {

		/* Decompose square into x, y coordinates of 10x10 board. */
		y0 = square0 / 5;
		x0 = 2 * (square0 % 5);
		if (!(y0 & 1))
			++x0;

		/* Do the LF direction. */
		bitnum = square0_to_bitnum(square0);
		add_diags(&diag_tbl[bitnum].lflen, diag_tbl[bitnum].lf, x0, y0, 1, 1);
		add_diags(&diag_tbl[bitnum].rflen, diag_tbl[bitnum].rf, x0, y0, -1, 1);
		add_diags(&diag_tbl[bitnum].lblen, diag_tbl[bitnum].lb, x0, y0, 1, -1);
		add_diags(&diag_tbl[bitnum].rblen, diag_tbl[bitnum].rb, x0, y0, -1, -1);
	}
}

#ifdef _WIN64

BITBOARD has_king_jumps_fwd(BITBOARD gen, BITBOARD free, BITBOARD opp, int shift)
{
	BITBOARD flood;

	flood = 0;
	while (gen) {
		flood |= gen;
		gen = (gen << shift) & free;
	}
	flood = (flood << shift) & opp;
	flood = (flood << shift) & free;
	return(flood);
}


BITBOARD has_king_jumps_back(BITBOARD gen, BITBOARD free, BITBOARD opp, int shift)
{
	BITBOARD flood;

	flood = 0;
	while (gen) {
		flood |= gen;
		gen = (gen >> shift) & free;
	}
	flood = (flood >> shift) & opp;
	flood = (flood >> shift) & free;
	return(flood);
}


/*
 * Flood fill version, is faster in 64-bit mode.
 */
int canjump(BOARD *board, int color)
{
	BITBOARD free;
	BITBOARD mask;
	BITBOARD bk, wk, bm, wm;

	free = ~(board->black | board->white) & ALL_SQUARES;
	if (color == BLACK) {
		bm = board->black & ~board->king;

		/* Jumps forward to the left. */
		mask = (((bm << LEFT_FWD_SHIFT) & board->white) << LEFT_FWD_SHIFT);

		/* Jumps forward to the right. */
		mask |= (((bm << RIGHT_FWD_SHIFT) & board->white) << RIGHT_FWD_SHIFT);

		/* Jumps back to the left. */
		mask |= (((bm >> LEFT_BACK_SHIFT) & board->white) >> LEFT_BACK_SHIFT);

		/* Jumps back to the right. */
		mask |= (((bm >> RIGHT_BACK_SHIFT) & board->white) >> RIGHT_BACK_SHIFT);

		if (mask & free)
			return(1);

		bk = board->black & board->king;
		if (bk) {
			if (has_king_jumps_fwd(bk, free, board->white, LEFT_FWD_SHIFT))
				return(1);
			if (has_king_jumps_fwd(bk, free, board->white, RIGHT_FWD_SHIFT))
				return(1);
			if (has_king_jumps_back(bk, free, board->white, LEFT_BACK_SHIFT))
				return(1);
			if (has_king_jumps_back(bk, free, board->white, RIGHT_BACK_SHIFT))
				return(1);
		}
		return(0);
	}
	else {
		wm = board->white & ~board->king;

		/* Jumps forward to the left. */
		mask = (((wm << LEFT_FWD_SHIFT) & board->black) << LEFT_FWD_SHIFT);

		/* Jumps forward to the right. */
		mask |= (((wm << RIGHT_FWD_SHIFT) & board->black) << RIGHT_FWD_SHIFT);

		/* Jumps back to the left. */
		mask |= (((wm >> LEFT_BACK_SHIFT) & board->black) >> LEFT_BACK_SHIFT);

		/* Jumps back to the right. */
		mask |= (((wm >> RIGHT_BACK_SHIFT) & board->black) >> RIGHT_BACK_SHIFT);

		if (mask & free)
			return(1);

		wk = board->white & board->king;
		if (wk) {
			if (has_king_jumps_fwd(wk, free, board->black, LEFT_FWD_SHIFT))
				return(1);
			if (has_king_jumps_fwd(wk, free, board->black, RIGHT_FWD_SHIFT))
				return(1);
			if (has_king_jumps_back(wk, free, board->black, LEFT_BACK_SHIFT))
				return(1);
			if (has_king_jumps_back(wk, free, board->black, RIGHT_BACK_SHIFT))
				return(1);
		}
		return(0);
	}
	return(0);
}

#else

int has_king_jumps(BITBOARD *pdiag, BITBOARD free, BITBOARD opponent_mask, int len)
{
	int i;

	if (len < 2)
		return(0);

	i = 0;
	while (i < len - 2 && (free & pdiag[i]))
		++i;
	if ((pdiag[i] & opponent_mask) && (pdiag[i + 1] & free))
		return(1);
	return(0);
}


/*
 * Iterative version, is faster in 32-bit mode.
 */
int canjump(BOARD *board, int color)
{
	int bitnum;
	BITBOARD free;
	BITBOARD mask;
	BITBOARD bk, wk, bm, wm;

	free = ~(board->black | board->white) & ALL_SQUARES;
	if (color == BLACK) {
		bm = board->black & ~board->king;

		/* Jumps forward to the left. */
		mask = (((bm << LEFT_FWD_SHIFT) & board->white) << LEFT_FWD_SHIFT);

		/* Jumps forward to the right. */
		mask |= (((bm << RIGHT_FWD_SHIFT) & board->white) << RIGHT_FWD_SHIFT);

		/* Jumps back to the left. */
		mask |= (((bm >> LEFT_BACK_SHIFT) & board->white) >> LEFT_BACK_SHIFT);

		/* Jumps back to the right. */
		mask |= (((bm >> RIGHT_BACK_SHIFT) & board->white) >> RIGHT_BACK_SHIFT);

		if (mask & free)
			return(1);

		bk = board->black & board->king;
		while (bk) {

			/* Strip off the next black king. */
			mask = bk & -(SIGNED_BITBOARD)bk;
			bk &= ~mask;
			bitnum = LSB64(mask);

			if (has_king_jumps(diag_tbl[bitnum].lf, free, board->white, diag_tbl[bitnum].lflen))
				return(1);
			if (has_king_jumps(diag_tbl[bitnum].rf, free, board->white, diag_tbl[bitnum].rflen))
				return(1);
			if (has_king_jumps(diag_tbl[bitnum].lb, free, board->white, diag_tbl[bitnum].lblen))
				return(1);
			if (has_king_jumps(diag_tbl[bitnum].rb, free, board->white, diag_tbl[bitnum].rblen))
				return(1);
		}
		return(0);
	}
	else {
		wm = board->white & ~board->king;

		/* Jumps forward to the left. */
		mask = (((wm << LEFT_FWD_SHIFT) & board->black) << LEFT_FWD_SHIFT);

		/* Jumps forward to the right. */
		mask |= (((wm << RIGHT_FWD_SHIFT) & board->black) << RIGHT_FWD_SHIFT);

		/* Jumps back to the left. */
		mask |= (((wm >> LEFT_BACK_SHIFT) & board->black) >> LEFT_BACK_SHIFT);

		/* Jumps back to the right. */
		mask |= (((wm >> RIGHT_BACK_SHIFT) & board->black) >> RIGHT_BACK_SHIFT);

		if (mask & free)
			return(1);

		wk = board->white & board->king;
		while (wk) {

			/* Strip off the next white king. */
			mask = wk & -(SIGNED_BITBOARD)wk;
			wk &= ~mask;
			bitnum = LSB64(mask);

			if (has_king_jumps(diag_tbl[bitnum].lf, free, board->black, diag_tbl[bitnum].lflen))
				return(1);
			if (has_king_jumps(diag_tbl[bitnum].rf, free, board->black, diag_tbl[bitnum].rflen))
				return(1);
			if (has_king_jumps(diag_tbl[bitnum].lb, free, board->black, diag_tbl[bitnum].lblen))
				return(1);
			if (has_king_jumps(diag_tbl[bitnum].rb, free, board->black, diag_tbl[bitnum].rblen))
				return(1);
		}
		return(0);
	}
	return(0);
}
#endif

/*
 * Build a movelist of non-jump moves.
 * Return the number of moves in the list.
 */
int build_nonjump_list(BOARD *board, MOVELIST *movelist, int color)
{
	int bitnum, i, len;
	BITBOARD free;
	BITBOARD mask;
	BITBOARD bk, wk;
	BITBOARD from, to;
	BITBOARD *pdiag;
	BOARD *mlp;

	movelist->count = 0;
	free = ~(board->black | board->white) & ALL_SQUARES;
	if (color == BLACK) {

		/* Left forward moves. */
		mask = ((board->black & ~board->king) << LEFT_FWD_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to >> RIGHT_BACK_SHIFT;

			/* Add it to the movelist. First clear the from square. */
			mlp = movelist->board + movelist->count++;
			mlp->black = board->black & ~from;
			mlp->white = board->white;
			mlp->king = board->king & ~from;

			/* Put him on the square where he lands. */
			mlp->black |= to;

			/* See if this made him a king. */
			if (to & BLACK_KING_RANK_MASK)
				mlp->king |= to;
		}

		/* Right forward moves. */
		mask = ((board->black & ~board->king) << RIGHT_FWD_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to >> LEFT_BACK_SHIFT;

			/* Add it to the movelist. First clear the from square. */
			mlp = movelist->board + movelist->count++;
			mlp->black = board->black & ~from;
			mlp->white = board->white;
			mlp->king = board->king & ~from;

			/* Put him on the square where he lands. */
			mlp->black |= to;

			/* See if this made him a king. */
			if (to & BLACK_KING_RANK_MASK)
				mlp->king |= to;
		}

		/* Black kings. */
		bk = board->black & board->king;
		while (bk) {

			/* Strip off the next black king. */
			from = bk & -(SIGNED_BITBOARD)bk;
			bk &= ~from;
			bitnum = LSB64(from);

			pdiag = diag_tbl[bitnum].lf;
			len = diag_tbl[bitnum].lflen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->white = board->white;
					mlp->black = board->black & ~from;
					mlp->black |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rf;
			len = diag_tbl[bitnum].rflen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->white = board->white;
					mlp->black = board->black & ~from;
					mlp->black |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].lb;
			len = diag_tbl[bitnum].lblen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->white = board->white;
					mlp->black = board->black & ~from;
					mlp->black |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rb;
			len = diag_tbl[bitnum].rblen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->white = board->white;
					mlp->black = board->black & ~from;
					mlp->black |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}
		}
	}
	else {
		/* White men. */
		/* Left back moves. */
		mask = ((board->white & ~board->king) >> LEFT_BACK_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to << RIGHT_FWD_SHIFT;

			/* Add it to the movelist. First clear the from square. */
			mlp = movelist->board + movelist->count++;
			mlp->white = board->white & ~from;
			mlp->black = board->black;
			mlp->king = board->king & ~from;

			/* Put him on the square where he lands. */
			mlp->white |= to;

			/* See if this made him a king. */
			if (to & WHITE_KING_RANK_MASK)
				mlp->king |= to;
		}

		/* Right back moves. */
		mask = ((board->white & ~board->king) >> RIGHT_BACK_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to << LEFT_FWD_SHIFT;

			/* Add it to the movelist. First clear the from square. */
			mlp = movelist->board + movelist->count++;
			mlp->white = board->white & ~from;
			mlp->black = board->black;
			mlp->king = board->king & ~from;

			/* Put him on the square where he lands. */
			mlp->white |= to;

			/* See if this made him a king. */
			if (to & WHITE_KING_RANK_MASK)
				mlp->king |= to;
		}

		/* White kings. */
		wk = board->white & board->king;
		while (wk) {

			/* Strip off the next black king. */
			from = wk & -(SIGNED_BITBOARD)wk;
			wk &= ~from;
			bitnum = LSB64(from);

			pdiag = diag_tbl[bitnum].lf;
			len = diag_tbl[bitnum].lflen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->black = board->black;
					mlp->white = board->white & ~from;
					mlp->white |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rf;
			len = diag_tbl[bitnum].rflen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->black = board->black;
					mlp->white = board->white & ~from;
					mlp->white |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].lb;
			len = diag_tbl[bitnum].lblen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->black = board->black;
					mlp->white = board->white & ~from;
					mlp->white |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rb;
			len = diag_tbl[bitnum].rblen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->black = board->black;
					mlp->white = board->white & ~from;
					mlp->white |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}
		}
	}
	return(movelist->count);
}


int build_man_nonjump_list(BOARD *board, MOVELIST *movelist, int color)
{
	BITBOARD free;
	BITBOARD mask;
	BITBOARD from, to;
	BOARD *mlp;

	movelist->count = 0;
	free = ~(board->black | board->white) & ALL_SQUARES;
	if (color == BLACK) {
		/* Left forward moves. */
		mask = ((board->black & ~board->king) << LEFT_FWD_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to >> RIGHT_BACK_SHIFT;

			/* Add it to the movelist. First clear the from square. */
			mlp = movelist->board + movelist->count++;
			mlp->black = board->black & ~from;
			mlp->white = board->white;
			mlp->king = board->king & ~from;

			/* Put him on the square where he lands. */
			mlp->black |= to;

			/* See if this made him a king. */
			if (to & BLACK_KING_RANK_MASK)
				mlp->king |= to;
		}

		/* Right forward moves. */
		mask = ((board->black & ~board->king) << RIGHT_FWD_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to >> LEFT_BACK_SHIFT;

			/* Add it to the movelist. First clear the from square. */
			mlp = movelist->board + movelist->count++;
			mlp->black = board->black & ~from;
			mlp->white = board->white;
			mlp->king = board->king & ~from;

			/* Put him on the square where he lands. */
			mlp->black |= to;

			/* See if this made him a king. */
			if (to & BLACK_KING_RANK_MASK)
				mlp->king |= to;
		}
	}
	else {
		/* White men. */
		/* Left back moves. */
		mask = ((board->white & ~board->king) >> LEFT_BACK_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to << RIGHT_FWD_SHIFT;

			/* Add it to the movelist. First clear the from square. */
			mlp = movelist->board + movelist->count++;
			mlp->white = board->white & ~from;
			mlp->black = board->black;
			mlp->king = board->king & ~from;

			/* Put him on the square where he lands. */
			mlp->white |= to;

			/* See if this made him a king. */
			if (to & WHITE_KING_RANK_MASK)
				mlp->king |= to;
		}

		/* Right back moves. */
		mask = ((board->white & ~board->king) >> RIGHT_BACK_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to << LEFT_FWD_SHIFT;

			/* Add it to the movelist. First clear the from square. */
			mlp = movelist->board + movelist->count++;
			mlp->white = board->white & ~from;
			mlp->black = board->black;
			mlp->king = board->king & ~from;

			/* Put him on the square where he lands. */
			mlp->white |= to;

			/* See if this made him a king. */
			if (to & WHITE_KING_RANK_MASK)
				mlp->king |= to;
		}
	}
	return(movelist->count);
}


/*
 * Build a movelist of non-jump moves.
 * Return the number of moves in the list.
 */
int build_king_nonjump_list(BOARD *board, MOVELIST *movelist, int color)
{
	int bitnum, i, len;
	BITBOARD free;
	BITBOARD bk, wk;
	BITBOARD from;
	BITBOARD *pdiag;
	BOARD *mlp;

	movelist->count = 0;
	free = ~(board->black | board->white);
	if (color == BLACK) {

		bk = board->black & board->king;
		while (bk) {

			/* Strip off the next black king. */
			from = bk & -(SIGNED_BITBOARD)bk;
			bk &= ~from;
			bitnum = LSB64(from);

			pdiag = diag_tbl[bitnum].lf;
			len = diag_tbl[bitnum].lflen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->white = board->white;
					mlp->black = board->black & ~from;
					mlp->black |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rf;
			len = diag_tbl[bitnum].rflen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->white = board->white;
					mlp->black = board->black & ~from;
					mlp->black |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].lb;
			len = diag_tbl[bitnum].lblen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->white = board->white;
					mlp->black = board->black & ~from;
					mlp->black |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rb;
			len = diag_tbl[bitnum].rblen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->white = board->white;
					mlp->black = board->black & ~from;
					mlp->black |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}
		}
	}
	else {

		wk = board->white & board->king;
		while (wk) {

			/* Strip off the next black king. */
			from = wk & -(SIGNED_BITBOARD)wk;
			wk &= ~from;
			bitnum = LSB64(from);

			pdiag = diag_tbl[bitnum].lf;
			len = diag_tbl[bitnum].lflen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->black = board->black;
					mlp->white = board->white & ~from;
					mlp->white |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rf;
			len = diag_tbl[bitnum].rflen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->black = board->black;
					mlp->white = board->white & ~from;
					mlp->white |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].lb;
			len = diag_tbl[bitnum].lblen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->black = board->black;
					mlp->white = board->white & ~from;
					mlp->white |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rb;
			len = diag_tbl[bitnum].rblen;
			for (i = 0; i < len; ++i) {
				if (pdiag[i] & free) {
					mlp = movelist->board + movelist->count++;
					mlp->black = board->black;
					mlp->white = board->white & ~from;
					mlp->white |= pdiag[i];
					mlp->king = board->king & ~from;
					mlp->king |= pdiag[i];
				}
				else
					break;
			}

		}
	}
	return(movelist->count);
}


/*
 * Build a movelist of jump moves.
 * Return the number of moves in the list.
 */
template <bool save_capture_info>
int build_jump_list(BOARD *board, MOVELIST *movelist, int color, CAPTURE_INFO cap[MAXMOVES])
{
	int i, lasti, bitnum, len, num_jumps, largest_num_jumps;
	BITBOARD free;
	BITBOARD mask;
	BITBOARD bm, wm;
	BITBOARD bk, wk;
	BITBOARD from, jumps, lands;
	BOARD jboard, immediately_behind;
	BITBOARD *pdiag;

	if (save_capture_info) {
		assert(cap != NULL);
	}
	movelist->count = 0;
	largest_num_jumps = 0;
	free = ~(board->black | board->white) & ALL_SQUARES;
	if (color == BLACK) {

		/* First do man jumps. */
		bm = board->black & ~board->king;
		for (i = 0; i < ARRAY_SIZE(fwd_shift_counts); ++i) {
			mask = (((bm << fwd_shift_counts[i]) & board->white) << fwd_shift_counts[i]) & free;
			for ( ; mask; mask = mask & (mask - 1)) {

				/* Peel off the lowest set bit in mask. */
				lands = mask & -(SIGNED_BITBOARD)mask;
				jumps = lands >> fwd_shift_counts[i];
				from = jumps >> fwd_shift_counts[i];

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				jboard.black = board->black & ~from;
				jboard.white = board->white;
				jboard.king = board->king;

				/* Put him on the square where he lands. */
				jboard.black |= lands;
				if (save_capture_info) {
					cap[movelist->count].path[0] = *board;
					cap[movelist->count].path[1] = jboard;
					cap[movelist->count].path[1].white &= ~jumps;
					cap[movelist->count].path[1].king &= ~jumps;
				}
				black_man_jump<save_capture_info>(&jboard, jumps, movelist, lands, 1, &largest_num_jumps, cap);
			}
		}

		for (i = 0; i < ARRAY_SIZE(back_shift_counts); ++i) {
			mask = (((bm >> back_shift_counts[i]) & board->white) >> back_shift_counts[i]) & free;
			for ( ; mask; mask = mask & (mask - 1)) {

				/* Peel off the lowest set bit in mask. */
				lands = mask & -(SIGNED_BITBOARD)mask;
				jumps = lands << back_shift_counts[i];
				from = jumps << back_shift_counts[i];

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				jboard.black = board->black & ~from;
				jboard.white = board->white;
				jboard.king = board->king;

				/* Put him on the square where he lands. */
				jboard.black |= lands;
				if (save_capture_info) {
					cap[movelist->count].path[0] = *board;
					cap[movelist->count].path[1] = jboard;
					cap[movelist->count].path[1].white &= ~jumps;
					cap[movelist->count].path[1].king &= ~jumps;
				}
				black_man_jump<save_capture_info>(&jboard, jumps, movelist, lands, 1, &largest_num_jumps, cap);
			}
		}

		bk = board->black & board->king;
		while (bk) {

			/* Strip off the next black king. */
			from = bk & -(SIGNED_BITBOARD)bk;
			bk &= ~from;
			bitnum = LSB64(from);

			pdiag = diag_tbl[bitnum].lf;
			len = diag_tbl[bitnum].lflen;

			/* Move past free squares along the diagonal. */
			for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
				;

			jumps = 0;
			num_jumps = 0;
			while (i < len - 1) {
				if (pdiag[i] & board->white && (pdiag[i + 1] & free)) {

					/* Found a jump.  Log this part of the move,
					 * and make the move on a copy of the board to be
					 * passed along to the recursion function.
					 * First clear the from square.
					 */
					jumps |= pdiag[i];
					++num_jumps;
					++i;
					if (save_capture_info) {
						lasti = i;
						cap[movelist->count].path[0] = *board;
					}
					for ( ; i < len && (pdiag[i] & free); ++i) {
						jboard.black = (board->black & ~from) | pdiag[i];
						jboard.king = (board->king & ~from) | pdiag[i];
						jboard.white = board->white;
						if (save_capture_info) {
							cap[movelist->count].path[num_jumps] = jboard;
							cap[movelist->count].path[num_jumps].white &= ~jumps;
							cap[movelist->count].path[num_jumps].king &= ~jumps;

							if (num_jumps > 1) {
								if (i == lasti)
									cap[movelist->count].path[num_jumps - 1] = immediately_behind;
							}
							if (i == lasti)
								immediately_behind = cap[movelist->count].path[num_jumps];
						}
						black_king_jump<save_capture_info>(&jboard, jumps, movelist, pdiag[i], DIR_RFLB, num_jumps,
								&largest_num_jumps, cap);
					}
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rf;
			len = diag_tbl[bitnum].rflen;

			/* Move past free squares along the diagonal. */
			for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
				;

			jumps = 0;
			num_jumps = 0;
			while (i < len - 1) {
				if (pdiag[i] & board->white && (pdiag[i + 1] & free)) {

					/* Found a jump.  Log this part of the move,
					 * and make the move on a copy of the board to be
					 * passed along to the recursion function.
					 * First clear the from square.
					 */
					jumps |= pdiag[i];
					++num_jumps;
					++i;
					if (save_capture_info) {
						lasti = i;
						cap[movelist->count].path[0] = *board;
					}
					for ( ; i < len && (pdiag[i] & free); ++i) {
						jboard.black = (board->black & ~from) | pdiag[i];
						jboard.king = (board->king & ~from) | pdiag[i];
						jboard.white = board->white;
						if (save_capture_info) {
							cap[movelist->count].path[num_jumps] = jboard;
							cap[movelist->count].path[num_jumps].white &= ~jumps;
							cap[movelist->count].path[num_jumps].king &= ~jumps;

							if (num_jumps > 1) {
								if (i == lasti)
									cap[movelist->count].path[num_jumps - 1] = immediately_behind;
							}
							if (i == lasti)
								immediately_behind = cap[movelist->count].path[num_jumps];
						}
						black_king_jump<save_capture_info>(&jboard, jumps, movelist, pdiag[i], DIR_LFRB, num_jumps,
								&largest_num_jumps, cap);
					}
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].lb;
			len = diag_tbl[bitnum].lblen;

			/* Move past free squares along the diagonal. */
			for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
				;

			jumps = 0;
			num_jumps = 0;
			while (i < len - 1) {
				if (pdiag[i] & board->white && (pdiag[i + 1] & free)) {

					/* Found a jump.  Log this part of the move,
					 * and make the move on a copy of the board to be
					 * passed along to the recursion function.
					 * First clear the from square.
					 */
					jumps |= pdiag[i];
					++num_jumps;
					++i;
					if (save_capture_info) {
						lasti = i;
						cap[movelist->count].path[0] = *board;
					}
					for ( ; i < len && (pdiag[i] & free); ++i) {
						jboard.black = (board->black & ~from) | pdiag[i];
						jboard.king = (board->king & ~from) | pdiag[i];
						jboard.white = board->white;
						if (save_capture_info) {
							cap[movelist->count].path[num_jumps] = jboard;
							cap[movelist->count].path[num_jumps].white &= ~jumps;
							cap[movelist->count].path[num_jumps].king &= ~jumps;

							if (num_jumps > 1) {
								if (i == lasti)
									cap[movelist->count].path[num_jumps - 1] = immediately_behind;
							}
							if (i == lasti)
								immediately_behind = cap[movelist->count].path[num_jumps];
						}
						black_king_jump<save_capture_info>(&jboard, jumps, movelist, pdiag[i], DIR_LFRB, num_jumps,
								&largest_num_jumps, cap);
					}
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rb;
			len = diag_tbl[bitnum].rblen;

			/* Move past free squares along the diagonal. */
			for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
				;

			jumps = 0;
			num_jumps = 0;
			while (i < len - 1) {
				if (pdiag[i] & board->white && (pdiag[i + 1] & free)) {

					/* Found a jump.  Log this part of the move,
					 * and make the move on a copy of the board to be
					 * passed along to the recursion function.
					 * First clear the from square.
					 */
					jumps |= pdiag[i];
					++num_jumps;
					++i;
					if (save_capture_info) {
						lasti = i;
						cap[movelist->count].path[0] = *board;
					}
					for ( ; i < len && (pdiag[i] & free); ++i) {
						jboard.black = (board->black & ~from) | pdiag[i];
						jboard.king = (board->king & ~from) | pdiag[i];
						jboard.white = board->white;
						if (save_capture_info) {
							cap[movelist->count].path[num_jumps] = jboard;
							cap[movelist->count].path[num_jumps].white &= ~jumps;
							cap[movelist->count].path[num_jumps].king &= ~jumps;

							if (num_jumps > 1) {
								if (i == lasti)
									cap[movelist->count].path[num_jumps - 1] = immediately_behind;
							}
							if (i == lasti)
								immediately_behind = cap[movelist->count].path[num_jumps];
						}
						black_king_jump<save_capture_info>(&jboard, jumps, movelist, pdiag[i], DIR_RFLB, num_jumps,
								&largest_num_jumps, cap);
					}
				}
				else
					break;
			}
		}	
	}
	else {
		wm = board->white & ~board->king;
		for (i = 0; i < ARRAY_SIZE(back_shift_counts); ++i) {
			mask = (((wm >> back_shift_counts[i]) & board->black) >> back_shift_counts[i]) & free;

			for ( ; mask; mask = mask & (mask - 1)) {

				/* Peel off the lowest set bit in mask. */
				lands = mask & -(SIGNED_BITBOARD)mask;
				jumps = lands << back_shift_counts[i];
				from = jumps << back_shift_counts[i];

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				jboard.white = board->white & ~from;
				jboard.black = board->black;
				jboard.king = board->king;

				/* Put him on the square where he lands. */
				jboard.white |= lands;
				if (save_capture_info) {
					cap[movelist->count].path[0] = *board;
					cap[movelist->count].path[1] = jboard;
					cap[movelist->count].path[1].black &= ~jumps;
					cap[movelist->count].path[1].king &= ~jumps;
				}
				white_man_jump<save_capture_info>(&jboard, jumps, movelist, lands, 1, &largest_num_jumps, cap);
			}
		}

		for (i = 0; i < ARRAY_SIZE(fwd_shift_counts); ++i) {
			mask = (((wm << fwd_shift_counts[i]) & board->black) << fwd_shift_counts[i]) & free;

			for ( ; mask; mask = mask & (mask - 1)) {

				/* Peel off the lowest set bit in mask. */
				lands = mask & -(SIGNED_BITBOARD)mask;
				jumps = lands >> fwd_shift_counts[i];
				from = jumps >> fwd_shift_counts[i];

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				jboard.white = board->white & ~from;
				jboard.black = board->black;
				jboard.king = board->king;

				/* Put him on the square where he lands. */
				jboard.white |= lands;
				if (save_capture_info) {
					cap[movelist->count].path[0] = *board;
					cap[movelist->count].path[1] = jboard;
					cap[movelist->count].path[1].black &= ~jumps;
					cap[movelist->count].path[1].king &= ~jumps;
				}
				white_man_jump<save_capture_info>(&jboard, jumps, movelist, lands, 1, &largest_num_jumps, cap);
			}
		}

		wk = board->white & board->king;
		while (wk) {

			/* Strip off the next white king. */
			from = wk & -(SIGNED_BITBOARD)wk;
			wk &= ~from;
			bitnum = LSB64(from);

			pdiag = diag_tbl[bitnum].lf;
			len = diag_tbl[bitnum].lflen;

			/* Move past free squares along the diagonal. */
			for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
				;

			jumps = 0;
			num_jumps = 0;
			while (i < len - 1) {
				if (pdiag[i] & board->black && (pdiag[i + 1] & free)) {

					/* Found a jump.  Log this part of the move,
					 * and make the move on a copy of the board to be
					 * passed along to the recursion function.
					 * First clear the from square.
					 */
					jumps |= pdiag[i];
					++num_jumps;
					++i;
					if (save_capture_info) {
						lasti = i;
						cap[movelist->count].path[0] = *board;
					}
					for ( ; i < len && (pdiag[i] & free); ++i) {
						jboard.white = (board->white & ~from) | pdiag[i];
						jboard.king = (board->king & ~from) | pdiag[i];
						jboard.black = board->black;
						if (save_capture_info) {
							cap[movelist->count].path[num_jumps] = jboard;
							cap[movelist->count].path[num_jumps].black &= ~jumps;
							cap[movelist->count].path[num_jumps].king &= ~jumps;

							if (num_jumps > 1) {
								if (i == lasti)
									cap[movelist->count].path[num_jumps - 1] = immediately_behind;
							}
							if (i == lasti)
								immediately_behind = cap[movelist->count].path[num_jumps];
						}
						white_king_jump<save_capture_info>(&jboard, jumps, movelist, pdiag[i], DIR_RFLB, num_jumps,
								&largest_num_jumps, cap);
					}
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rf;
			len = diag_tbl[bitnum].rflen;

			/* Move past free squares along the diagonal. */
			for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
				;

			jumps = 0;
			num_jumps = 0;
			while (i < len - 1) {
				if (pdiag[i] & board->black && (pdiag[i + 1] & free)) {

					/* Found a jump.  Log this part of the move,
					 * and make the move on a copy of the board to be
					 * passed along to the recursion function.
					 * First clear the from square.
					 */
					jumps |= pdiag[i];
					++num_jumps;
					++i;
					if (save_capture_info) {
						lasti = i;
						cap[movelist->count].path[0] = *board;
					}
					for ( ; i < len && (pdiag[i] & free); ++i) {
						jboard.white = (board->white & ~from) | pdiag[i];
						jboard.king = (board->king & ~from) | pdiag[i];
						jboard.black = board->black;
						if (save_capture_info) {
							cap[movelist->count].path[num_jumps] = jboard;
							cap[movelist->count].path[num_jumps].black &= ~jumps;
							cap[movelist->count].path[num_jumps].king &= ~jumps;

							if (num_jumps > 1) {
								if (i == lasti)
									cap[movelist->count].path[num_jumps - 1] = immediately_behind;
							}
							if (i == lasti)
								immediately_behind = cap[movelist->count].path[num_jumps];
						}
						white_king_jump<save_capture_info>(&jboard, jumps, movelist, pdiag[i], DIR_LFRB, num_jumps,
								&largest_num_jumps, cap);
					}
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].lb;
			len = diag_tbl[bitnum].lblen;

			/* Move past free squares along the diagonal. */
			for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
				;

			jumps = 0;
			num_jumps = 0;
			while (i < len - 1) {
				if (pdiag[i] & board->black && (pdiag[i + 1] & free)) {

					/* Found a jump.  Log this part of the move,
					 * and make the move on a copy of the board to be
					 * passed along to the recursion function.
					 * First clear the from square.
					 */
					jumps |= pdiag[i];
					++num_jumps;
					++i;
					if (save_capture_info) {
						lasti = i;
						cap[movelist->count].path[0] = *board;
					}
					for ( ; i < len && (pdiag[i] & free); ++i) {
						jboard.white = (board->white & ~from) | pdiag[i];
						jboard.king = (board->king & ~from) | pdiag[i];
						jboard.black = board->black;
						if (save_capture_info) {
							cap[movelist->count].path[num_jumps] = jboard;
							cap[movelist->count].path[num_jumps].black &= ~jumps;
							cap[movelist->count].path[num_jumps].king &= ~jumps;

							if (num_jumps > 1) {
								if (i == lasti)
									cap[movelist->count].path[num_jumps - 1] = immediately_behind;
							}
							if (i == lasti)
								immediately_behind = cap[movelist->count].path[num_jumps];
						}
						white_king_jump<save_capture_info>(&jboard, jumps, movelist, pdiag[i], DIR_LFRB, num_jumps,
								&largest_num_jumps, cap);
					}
				}
				else
					break;
			}

			pdiag = diag_tbl[bitnum].rb;
			len = diag_tbl[bitnum].rblen;

			/* Move past free squares along the diagonal. */
			for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
				;

			jumps = 0;
			num_jumps = 0;
			while (i < len - 1) {
				if (pdiag[i] & board->black && (pdiag[i + 1] & free)) {

					/* Found a jump.  Log this part of the move,
					 * and make the move on a copy of the board to be
					 * passed along to the recursion function.
					 * First clear the from square.
					 */
					jumps |= pdiag[i];
					++num_jumps;
					++i;
					if (save_capture_info) {
						lasti = i;
						cap[movelist->count].path[0] = *board;
					}
					for ( ; i < len && (pdiag[i] & free); ++i) {
						jboard.white = (board->white & ~from) | pdiag[i];
						jboard.king = (board->king & ~from) | pdiag[i];
						jboard.black = board->black;
						if (save_capture_info) {
							cap[movelist->count].path[num_jumps] = jboard;
							cap[movelist->count].path[num_jumps].black &= ~jumps;
							cap[movelist->count].path[num_jumps].king &= ~jumps;

							if (num_jumps > 1) {
								if (i == lasti)
									cap[movelist->count].path[num_jumps - 1] = immediately_behind;
							}
							if (i == lasti)
								immediately_behind = cap[movelist->count].path[num_jumps];
						}
						white_king_jump<save_capture_info>(&jboard, jumps, movelist, pdiag[i], DIR_RFLB, num_jumps,
								&largest_num_jumps, cap);
					}
				}
				else
					break;
			}
		}	
	}
	return(movelist->count);
}


template <bool save_capture_info>
static void black_king_jump(BOARD *board, BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from,
					 DIR dir, int num_jumps, int *largest_num_jumps,
					 CAPTURE_INFO cap[])
{
	BITBOARD jumps;
	BITBOARD free;
	BITBOARD *pdiag;
	int i, lasti, bitnum, len, local_num_jumps, found_jump;
	BOARD jboard, immediately_behind;

	found_jump = 0;
	free = ~(board->black | board->white);
	bitnum = LSB64(from);

	if (dir == DIR_LFRB) {
		pdiag = diag_tbl[bitnum].lf;
		len = diag_tbl[bitnum].lflen;

		/* Move past free squares along the diagonal. */
		for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
			;

		jumps = 0;
		local_num_jumps = 0;
		while (i < len - 1) {
			if ((pdiag[i] & board->white & ~all_jumped) && (pdiag[i + 1] & free)) {

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				found_jump = 1;
				jumps |= pdiag[i];
				++local_num_jumps;
				++i;
				if (save_capture_info)
					lasti = i;

				for ( ; i < len && (pdiag[i] & free); ++i) {
					jboard.black = (board->black & ~from) | pdiag[i];
					jboard.king = (board->king & ~from) | pdiag[i];
					jboard.white = board->white;
					if (save_capture_info) {
						assign_black_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + local_num_jumps, movelist->count);
						if (local_num_jumps > 1) {
							if (i == lasti)
								cap[movelist->count].path[num_jumps + local_num_jumps - 1] = immediately_behind;
						}
						if (i == lasti)
							immediately_behind = cap[movelist->count].path[num_jumps + local_num_jumps];
					}

					black_king_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, pdiag[i], DIR_RFLB,
									num_jumps + local_num_jumps, largest_num_jumps, cap);
				}
			}
			else
				break;
		}

		pdiag = diag_tbl[bitnum].rb;
		len = diag_tbl[bitnum].rblen;

		/* Move past free squares along the diagonal. */
		for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
			;

		jumps = 0;
		local_num_jumps = 0;
		while (i < len - 1) {
			if ((pdiag[i] & board->white & ~all_jumped) && (pdiag[i + 1] & free)) {

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				found_jump = 1;
				jumps |= pdiag[i];
				++local_num_jumps;
				++i;
				if (save_capture_info)
					lasti = i;

				for ( ; i < len && (pdiag[i] & free); ++i) {
					jboard.black = (board->black & ~from) | pdiag[i];
					jboard.king = (board->king & ~from) | pdiag[i];
					jboard.white = board->white;
					if (save_capture_info) {
						assign_black_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + local_num_jumps, movelist->count);
						if (local_num_jumps > 1) {
							if (i == lasti)
								cap[movelist->count].path[num_jumps + local_num_jumps - 1] = immediately_behind;
						}
						if (i == lasti)
							immediately_behind = cap[movelist->count].path[num_jumps + local_num_jumps];
					}

					black_king_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, pdiag[i], DIR_RFLB,
									num_jumps + local_num_jumps, largest_num_jumps, cap);
				}
			}
			else
				break;
		}
	}
	else {
		pdiag = diag_tbl[bitnum].rf;
		len = diag_tbl[bitnum].rflen;

		/* Move past free squares along the diagonal. */
		for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
			;

		jumps = 0;
		local_num_jumps = 0;
		while (i < len - 1) {
			if ((pdiag[i] & board->white & ~all_jumped) && (pdiag[i + 1] & free)) {

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				found_jump = 1;
				jumps |= pdiag[i];
				++local_num_jumps;
				++i;
				if (save_capture_info)
					lasti = i;

				for ( ; i < len && (pdiag[i] & free); ++i) {
					jboard.black = (board->black & ~from) | pdiag[i];
					jboard.king = (board->king & ~from) | pdiag[i];
					jboard.white = board->white;
					if (save_capture_info) {
						assign_black_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + local_num_jumps, movelist->count);
						if (local_num_jumps > 1) {
							if (i == lasti)
								cap[movelist->count].path[num_jumps + local_num_jumps - 1] = immediately_behind;
						}
						if (i == lasti)
							immediately_behind = cap[movelist->count].path[num_jumps + local_num_jumps];
					}

					black_king_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, pdiag[i], DIR_LFRB,
									num_jumps + local_num_jumps, largest_num_jumps, cap);
				}
			}
			else
				break;
		}

		pdiag = diag_tbl[bitnum].lb;
		len = diag_tbl[bitnum].lblen;

		/* Move past free squares along the diagonal. */
		for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
			;

		jumps = 0;
		local_num_jumps = 0;
		while (i < len - 1) {
			if ((pdiag[i] & board->white & ~all_jumped) && (pdiag[i + 1] & free)) {

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				found_jump = 1;
				jumps |= pdiag[i];
				++local_num_jumps;
				++i;
				if (save_capture_info)
					lasti = i;

				for ( ; i < len && (pdiag[i] & free); ++i) {
					jboard.black = (board->black & ~from) | pdiag[i];
					jboard.king = (board->king & ~from) | pdiag[i];
					jboard.white = board->white;
					if (save_capture_info) {
						assign_black_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + local_num_jumps, movelist->count);
						if (local_num_jumps > 1) {
							if (i == lasti)
								cap[movelist->count].path[num_jumps + local_num_jumps - 1] = immediately_behind;
						}
						if (i == lasti)
							immediately_behind = cap[movelist->count].path[num_jumps + local_num_jumps];
					}

					black_king_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, pdiag[i], DIR_LFRB,
									num_jumps + local_num_jumps, largest_num_jumps, cap);
				}
			}
			else
				break;
		}
	}

	/* If we didn't find any move jumps, then add the board that was
	 * passed in to the movelist.
	 */
	if (!found_jump) {
		if (num_jumps >= *largest_num_jumps) {
			if (num_jumps > *largest_num_jumps) {
				if (save_capture_info)
					memcpy(cap, cap + movelist->count, sizeof(cap[0]));
				movelist->count = 0;
				*largest_num_jumps = num_jumps;
			}

			movelist->board[movelist->count] = *board;
			if (save_capture_info) {
				cap[movelist->count].capture_count = num_jumps;

				/* Copy the path to the next potential jump move. */
				copy_path(cap, movelist);
			}

			/* Clear the board of white pieces that were jumped. */
			movelist->board[movelist->count].white &= ~all_jumped;
			movelist->board[movelist->count].king &= ~all_jumped;
			if (num_jumps >= 4) {
				for (i = 0; i < movelist->count; ++i)
					if (memcmp(movelist->board + i, movelist->board + movelist->count, sizeof(BOARD)) == 0)
						return;
			}
			movelist->count++;
		}
	}
}


template <bool save_capture_info>
void white_king_jump(BOARD *board, BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from,
					 DIR dir, int num_jumps, int *largest_num_jumps,
					 CAPTURE_INFO cap[])
{
	BITBOARD jumps;
	BITBOARD free;
	BITBOARD *pdiag;
	int i, lasti, bitnum, len, local_num_jumps, found_jump;
	BOARD jboard, immediately_behind;

	found_jump = 0;
	free = ~(board->black | board->white);
	bitnum = LSB64(from);

	if (dir == DIR_LFRB) {
		pdiag = diag_tbl[bitnum].lf;
		len = diag_tbl[bitnum].lflen;

		/* Move past free squares along the diagonal. */
		for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
			;

		jumps = 0;
		local_num_jumps = 0;
		while (i < len - 1) {
			if ((pdiag[i] & board->black & ~all_jumped) && (pdiag[i + 1] & free)) {

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				found_jump = 1;
				jumps |= pdiag[i];
				++local_num_jumps;
				++i;
				if (save_capture_info)
					lasti = i;

				for ( ; i < len && (pdiag[i] & free); ++i) {
					jboard.white = (board->white & ~from) | pdiag[i];
					jboard.king = (board->king & ~from) | pdiag[i];
					jboard.black = board->black;
					if (save_capture_info) {
						assign_white_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + local_num_jumps, movelist->count);
						if (local_num_jumps > 1) {
							if (i == lasti)
								cap[movelist->count].path[num_jumps + local_num_jumps - 1] = immediately_behind;
						}
						if (i == lasti)
							immediately_behind = cap[movelist->count].path[num_jumps + local_num_jumps];
					}

					white_king_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, pdiag[i], DIR_RFLB,
									num_jumps + local_num_jumps, largest_num_jumps, cap);
				}
			}
			else
				break;
		}

		pdiag = diag_tbl[bitnum].rb;
		len = diag_tbl[bitnum].rblen;

		/* Move past free squares along the diagonal. */
		for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
			;

		jumps = 0;
		local_num_jumps = 0;
		while (i < len - 1) {
			if ((pdiag[i] & board->black & ~all_jumped) && (pdiag[i + 1] & free)) {

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				found_jump = 1;
				jumps |= pdiag[i];
				++local_num_jumps;
				++i;
				if (save_capture_info)
					lasti = i;

				for ( ; i < len && (pdiag[i] & free); ++i) {
					jboard.white = (board->white & ~from) | pdiag[i];
					jboard.king = (board->king & ~from) | pdiag[i];
					jboard.black = board->black;
					if (save_capture_info) {
						assign_white_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + local_num_jumps, movelist->count);
						if (local_num_jumps > 1) {
							if (i == lasti)
								cap[movelist->count].path[num_jumps + local_num_jumps - 1] = immediately_behind;
						}
						if (i == lasti)
							immediately_behind = cap[movelist->count].path[num_jumps + local_num_jumps];
					}

					white_king_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, pdiag[i], DIR_RFLB,
									num_jumps + local_num_jumps, largest_num_jumps, cap);
				}
			}
			else
				break;
		}
	}
	else {
		pdiag = diag_tbl[bitnum].rf;
		len = diag_tbl[bitnum].rflen;

		/* Move past free squares along the diagonal. */
		for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
			;

		jumps = 0;
		local_num_jumps = 0;
		while (i < len - 1) {
			if ((pdiag[i] & board->black & ~all_jumped) && (pdiag[i + 1] & free)) {

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				found_jump = 1;
				jumps |= pdiag[i];
				++local_num_jumps;
				++i;
				if (save_capture_info)
					lasti = i;

				for ( ; i < len && (pdiag[i] & free); ++i) {
					jboard.white = (board->white & ~from) | pdiag[i];
					jboard.king = (board->king & ~from) | pdiag[i];
					jboard.black = board->black;
					if (save_capture_info) {
						assign_white_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + local_num_jumps, movelist->count);
						if (local_num_jumps > 1) {
							if (i == lasti)
								cap[movelist->count].path[num_jumps + local_num_jumps - 1] = immediately_behind;
						}
						if (i == lasti)
							immediately_behind = cap[movelist->count].path[num_jumps + local_num_jumps];
					}

					white_king_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, pdiag[i], DIR_LFRB,
									num_jumps + local_num_jumps, largest_num_jumps, cap);
				}
			}
			else
				break;
		}

		pdiag = diag_tbl[bitnum].lb;
		len = diag_tbl[bitnum].lblen;

		/* Move past free squares along the diagonal. */
		for (i = 0; i < len - 2 && (pdiag[i] & free); ++i)
			;

		jumps = 0;
		local_num_jumps = 0;
		while (i < len - 1) {
			if ((pdiag[i] & board->black & ~all_jumped) && (pdiag[i + 1] & free)) {

				/* Found a jump.  Log this part of the move,
				 * and make the move on a copy of the board to be
				 * passed along to the recursion function.
				 * First clear the from square.
				 */
				found_jump = 1;
				jumps |= pdiag[i];
				++local_num_jumps;
				++i;
				if (save_capture_info)
					lasti = i;

				for ( ; i < len && (pdiag[i] & free); ++i) {
					jboard.white = (board->white & ~from) | pdiag[i];
					jboard.king = (board->king & ~from) | pdiag[i];
					jboard.black = board->black;
					if (save_capture_info) {
						assign_white_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + local_num_jumps, movelist->count);
						if (local_num_jumps > 1) {
							if (i == lasti)
								cap[movelist->count].path[num_jumps + local_num_jumps - 1] = immediately_behind;
						}
						if (i == lasti)
							immediately_behind = cap[movelist->count].path[num_jumps + local_num_jumps];
					}

					white_king_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, pdiag[i], DIR_LFRB,
									num_jumps + local_num_jumps, largest_num_jumps, cap);
				}
			}
			else
				break;
		}
	}

	/* If we didn't find any move jumps, then add the board that was
	 * passed in to the movelist.
	 */
	if (!found_jump) {
		if (num_jumps >= *largest_num_jumps) {
			if (num_jumps > *largest_num_jumps) {
				if (save_capture_info)
					memcpy(cap, cap + movelist->count, sizeof(cap[0]));
				movelist->count = 0;
				*largest_num_jumps = num_jumps;
			}

			movelist->board[movelist->count] = *board;
			if (save_capture_info) {
				cap[movelist->count].capture_count = num_jumps;

				/* Copy the path to the next potential jump move. */
				copy_path(cap, movelist);
			}

			/* Clear the board of black pieces that were jumped. */
			movelist->board[movelist->count].black &= ~all_jumped;
			movelist->board[movelist->count].king &= ~all_jumped;
			if (num_jumps >= 4) {
				for (i = 0; i < movelist->count; ++i)
					if (memcmp(movelist->board + i, movelist->board + movelist->count, sizeof(BOARD)) == 0)
						return;
			}
			movelist->count++;
		}
	}
}


#define BLACK_MAN_JUMP(shift, shift_oper) \
	jumps = (from shift_oper shift) & board->white & ~all_jumped; \
	lands = (jumps shift_oper shift) & free; \
	if (lands) { \
\
		/* Found a jump. \
		 * Make the move on a copy of the board to be \
		 * passed along to the recursion function. \
		 * First clear the from square. \
		 */ \
		jboard.black = board->black & ~from; \
		jboard.white = board->white; \
		jboard.king = board->king; \
\
		/* Put him on the square where he lands. */ \
		jboard.black |= lands; \
\
		if (save_capture_info) \
			assign_black_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + 1, movelist->count); \
		black_man_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, lands, num_jumps + 1, largest_num_jumps, cap); \
		found_jump = 1; \
	}

	
	
/*
 * We're in the middle of a jump.
 * If we've reached the end of the jump, then add this board to the movelist.
 * If not, call again recursively.
 */
template <bool save_capture_info>
void black_man_jump(BOARD *board, BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from,
						 int num_jumps, int *largest_num_jumps, CAPTURE_INFO cap[])
{
	BITBOARD jumps;
	BITBOARD lands;
	BITBOARD free;
	int found_jump;
	BOARD jboard;

	found_jump = 0;
	free = ~(board->black | board->white) & ALL_SQUARES;

	BLACK_MAN_JUMP(LEFT_FWD_SHIFT, <<);
	BLACK_MAN_JUMP(RIGHT_FWD_SHIFT, <<);
	BLACK_MAN_JUMP(LEFT_BACK_SHIFT, >>);
	BLACK_MAN_JUMP(RIGHT_BACK_SHIFT, >>);

	/* If we didn't find any move jumps, then add the board that was
	 * passed in to the movelist.
	 */
	if (!found_jump) {
		if (num_jumps >= *largest_num_jumps) {
			if (num_jumps > *largest_num_jumps) {
				if (save_capture_info)
					memcpy(cap, cap + movelist->count, sizeof(cap[0]));
				movelist->count = 0;
				*largest_num_jumps = num_jumps;
			}

			movelist->board[movelist->count] = *board;
			if (save_capture_info)
				cap[movelist->count].capture_count = num_jumps;

			if (from & BLACK_KING_RANK_MASK)
				movelist->board[movelist->count].king |= from;

			/* Copy the path to the next potential jump move. */
			if (save_capture_info)
				copy_path(cap, movelist);

			/* Clear the board of white pieces that were jumped. */
			movelist->board[movelist->count].white &= ~all_jumped;
			movelist->board[movelist->count].king &= ~all_jumped;

			/* Check for duplicates. */
			if (num_jumps >= 4) {
				int i;

				for (i = 0; i < movelist->count; ++i)
					if (memcmp(movelist->board + i, movelist->board + movelist->count, sizeof(BOARD)) == 0)
						return;
			}
			movelist->count++;
		}
	}
}


#define WHITE_MAN_JUMP(shift, shift_oper) \
	jumps = (from shift_oper shift) & board->black & ~all_jumped; \
	lands = (jumps shift_oper shift) & free; \
	if (lands) { \
\
		/* Found a jump. \
		 * Make the move on a copy of the board to be \
		 * passed along to the recursion function. \
		 * First clear the from square. \
		 */ \
		jboard.white = board->white & ~from; \
		jboard.black = board->black; \
		jboard.king = board->king; \
\
		/* Put him on the square where he lands. */ \
		jboard.white |= lands; \
\
		if (save_capture_info) \
			assign_white_jump_path(&jboard, all_jumped | jumps, cap, num_jumps + 1, movelist->count); \
		white_man_jump<save_capture_info>(&jboard, all_jumped | jumps, movelist, lands, num_jumps + 1, largest_num_jumps, cap); \
		found_jump = 1; \
	}


/*
 * We're in the middle of a jump.
 * If we've reached the end of the jump, then add this board to the movelist.
 * If not, call again recursively.
 */
template <bool save_capture_info>
void white_man_jump(BOARD *board, BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from,
						 int num_jumps, int *largest_num_jumps, CAPTURE_INFO cap[])
{
	BITBOARD jumps;
	BITBOARD lands;
	BITBOARD free;
	int found_jump;
	BOARD jboard;

	found_jump = 0;
	free = ~(board->black | board->white) & ALL_SQUARES;

	WHITE_MAN_JUMP(LEFT_FWD_SHIFT, <<);
	WHITE_MAN_JUMP(RIGHT_FWD_SHIFT, <<);
	WHITE_MAN_JUMP(LEFT_BACK_SHIFT, >>);
	WHITE_MAN_JUMP(RIGHT_BACK_SHIFT, >>);

	/* If we didn't find any move jumps, then add the board that was
	 * passed in to the movelist.
	 */
	if (!found_jump) {
		if (num_jumps >= *largest_num_jumps) {
			if (num_jumps > *largest_num_jumps) {
				if (save_capture_info)
					memcpy(cap, cap + movelist->count, sizeof(cap[0]));
				movelist->count = 0;
				*largest_num_jumps = num_jumps;
			}

			movelist->board[movelist->count] = *board;

			/* See if this makes him a king. */
			if (from & WHITE_KING_RANK_MASK)
				movelist->board[movelist->count].king |= from;

			if (save_capture_info) {
				cap[movelist->count].capture_count = num_jumps;

				/* Copy the path to the next potential jump move. */
				copy_path(cap, movelist);
			}

			/* Clear the board of black pieces that were jumped. */
			movelist->board[movelist->count].black &= ~all_jumped;
			movelist->board[movelist->count].king &= ~all_jumped;

			/* Check for duplicates. */
			if (num_jumps >= 4) {
				int i;

				for (i = 0; i < movelist->count; ++i)
					if (memcmp(movelist->board + i, movelist->board + movelist->count, sizeof(BOARD)) == 0)
						return;
			}
			movelist->count++;
		}
	}
}


void get_jumped_square(int jumpi, int movei, int color, CAPTURE_INFO cap[], BITBOARD *jumped)
{
	if (color == BLACK)
		*jumped = cap[movei].path[jumpi].white & ~cap[movei].path[jumpi + 1].white;
	else
		*jumped = cap[movei].path[jumpi].black & ~cap[movei].path[jumpi + 1].black;
}


void get_landed_square(int jumpi, int movei, int color, CAPTURE_INFO cap[], BITBOARD *landed)
{
	if (color == BLACK)
		*landed = ~cap[movei].path[jumpi].black & cap[movei].path[jumpi + 1].black;
	else
		*landed = ~cap[movei].path[jumpi].white & cap[movei].path[jumpi + 1].white;
}


int has_jumped_square(int jumpcount, int movei, int color, CAPTURE_INFO cap[], BITBOARD jumped)
{
	int i;
	BITBOARD thisjumped;

	for (i = 0; i < jumpcount; ++i) {
		get_jumped_square(i, movei, color, cap, &thisjumped);
		if (thisjumped == jumped)
			return(1);
	}
	return(0);
}


int has_landed_square(int jumpcount, int movei, int color, CAPTURE_INFO *cap, int jumpto_square)
{
	int i;

	for (i = 0; i < jumpcount - 1; ++i) {
		if (match_jumpto_square(cap + movei, i, color, jumpto_square, jumpcount))
			return(1);
	}
	return(0);
}


void get_from_to(int jumpcount, int movei, int color, BOARD *board, MOVELIST *movelist,
				 CAPTURE_INFO cap[], BITBOARD *from, BITBOARD *to)
{
	if (jumpcount) {
		if (color == BLACK) {
			*from = cap[movei].path[0].black & ~cap[movei].path[1].black;
			*to = ~cap[movei].path[jumpcount - 1].black & cap[movei].path[jumpcount].black;
		}
		else {
			*from = cap[movei].path[0].white & ~cap[movei].path[1].white;
			*to = ~cap[movei].path[jumpcount - 1].white & cap[movei].path[jumpcount].white;
		}
	}
	else {
		if (color == BLACK) {
			*from = board->black & ~movelist->board[movei].black;
			*to = ~board->black & movelist->board[movei].black;
		}
		else {
			*from = board->white & ~movelist->board[movei].white;
			*to = ~board->white & movelist->board[movei].white;
		}
	}
}


DIR4 get_direction(int src_sq, int dest_sq)
{
	BITBOARD srcbb, destbb;

	srcbb = square0_to_bitboard(src_sq - 1);
	destbb = square0_to_bitboard(dest_sq - 1);

	if (dest_sq > src_sq) {
		/* Try left fwd. */
		for (srcbb <<= LEFT_FWD_SHIFT; srcbb & ALL_SQUARES; srcbb <<= LEFT_FWD_SHIFT)
			if (srcbb == destbb)
				return(DIR_LF);

		return(DIR_RF);
	}
	else {
		/* Try left back. */
		for (srcbb >>= LEFT_BACK_SHIFT; srcbb & ALL_SQUARES; srcbb >>= LEFT_BACK_SHIFT)
			if (srcbb == destbb)
				return(DIR_LB);

		return(DIR_RB);
	}
}


int match_jumpto_square(CAPTURE_INFO *cap, int jumpindex, int color, int jumpto_square, int jumpcount)
{
	int fromsq0, tosq0, next_fromsq0, next_tosq0, shift;
	DIR4 direction_this, direction_next;
	BITBOARD frombb, free;

	/* If the jumper is a king, and the next jump is in the same direction as this one, then
	 * there may be other jumpto squares possible besides the one in the cap array from the movelist.
	 */
	get_fromto(cap->path + jumpindex, cap->path + jumpindex + 1, color, &fromsq0, &tosq0);
	frombb = square0_to_bitboard(fromsq0);
	if (frombb & cap->path[0].king) {
		get_fromto(cap->path + jumpindex + 1, cap->path + jumpindex + 2, color, &next_fromsq0, &next_tosq0);
		direction_this = get_direction(fromsq0 + 1, tosq0 + 1);
		direction_next = get_direction(next_fromsq0 + 1, next_tosq0 + 1);
		if (direction_this == direction_next) {

			/* Start at the from square, move past the jumped opponent, and look for a match at each
			 * possible jumped to square.
			 */
			free = ~(cap->path[jumpindex].black | cap->path[jumpindex].white) & ALL_SQUARES;
			switch (direction_this) {
			case DIR_LF:
			case DIR_RF:
				if (direction_this == DIR_LF)
					shift = LEFT_FWD_SHIFT;
				else
					shift = RIGHT_FWD_SHIFT;
				frombb <<= shift;
				while (frombb & free)
					frombb <<= shift;

				/* Skip past the jumped piece. */
				if (frombb & ~(cap->path[jumpindex].black | cap->path[jumpindex].white))
					return(0);

				frombb <<= shift;
				while (frombb & free) {
					tosq0 = bitnum_to_square0(LSB64(frombb));
					if (tosq0 + 1 == jumpto_square)
						return(1);
					frombb <<= shift;
				}
				return(0);
				break;

			case DIR_LB:
			case DIR_RB:
				if (direction_this == DIR_LB)
					shift = LEFT_BACK_SHIFT;
				else
					shift = RIGHT_BACK_SHIFT;
				frombb >>= shift;
				while (frombb & free)
					frombb >>= shift;

				/* Skip past the jumped piece. */
				if (frombb & ~(cap->path[jumpindex].black | cap->path[jumpindex].white))
					return(0);

				frombb >>= shift;
				while (frombb & free) {
					tosq0 = bitnum_to_square0(LSB64(frombb));
					if (tosq0 + 1 == jumpto_square)
						return(1);
					frombb >>= shift;
				}
				return(0);
				break;
			}
		}
	}
	if (tosq0 + 1 == jumpto_square)
		return(1);
	else
		return(0);
}


int build_movelist_path(BOARD *board, int color, MOVELIST *movelist, CAPTURE_INFO *path)
{
	int count;

	count = build_jump_list<true>(board, movelist, color, path);
	if (!count)
		count = build_nonjump_list(board, movelist, color);
	return(count);
}


int has_a_king_move(BOARD *p, int color)
{
	BITBOARD free, m, kingmask;

	if (color == BLACK)
		kingmask = p->black & p->king;
	else
		kingmask = p->white & p->king;
	if (kingmask) {
		m = kingmask << LEFT_FWD_SHIFT;
		m |= kingmask << RIGHT_FWD_SHIFT;
		m |= kingmask >> LEFT_BACK_SHIFT;
		m |= kingmask >> RIGHT_BACK_SHIFT;
		free = ~(p->black | p->white) & ALL_SQUARES;
		if (m & free)
			return 1;
	}
	return 0;
}


/*
 * Return the mask of squares that black can safely move to.
 */
BITBOARD black_safe_moveto(BOARD *board)
{
	BITBOARD free, m, safe, prop;

	free = ~(board->black | board->white) & ALL_SQUARES;

	/* Left forward moves. */
	safe = (board->black << LEFT_FWD_SHIFT) & free;

	/* Test for RB captures. */
	safe &= ~(board->white >> RIGHT_BACK_SHIFT);

	/* Test for LB captures. */
	safe &= ~((board->white >> LEFT_BACK_SHIFT) & (free << RIGHT_FWD_SHIFT));

	/* Test for RF captures. */
	safe &= ~((board->white << RIGHT_FWD_SHIFT) & (free >> LEFT_BACK_SHIFT));

	/* Make sure this black man is not protecting another man from capture. */
	/* Left back captures. */
	prop = (((board->white >> LEFT_BACK_SHIFT) & board->black) << 1);

	/* Right fwd captures. */
	prop |= (((board->white << RIGHT_FWD_SHIFT) & board->black) << (LEFT_FWD_SHIFT + RIGHT_FWD_SHIFT));

	/* Left fwd captures. */
	prop |= (((board->white << LEFT_FWD_SHIFT) & board->black) << (2 * LEFT_FWD_SHIFT)) & ~(ROW9 | COLUMN0);
	safe &= ~prop;

	/* Right forward moves. */
	m = (board->black << RIGHT_FWD_SHIFT) & free;

	/* Test for LB captures. */
	m &= ~((board->white >> LEFT_BACK_SHIFT));

	/* Test for RB captures. */
	m &= ~((board->white >> RIGHT_BACK_SHIFT) & (free << LEFT_FWD_SHIFT));

	/* Test for LF captures. */
	m &= ~((board->white << LEFT_FWD_SHIFT) & (free >> RIGHT_BACK_SHIFT));

	/* Make sure this black man is not protecting another man from capture. */
	/* Right back captures. */
	prop = (((board->white >> RIGHT_BACK_SHIFT) & board->black) >> 1);

	/* Left fwd captures. */
	prop |= (((board->white << LEFT_FWD_SHIFT) & board->black) << (LEFT_FWD_SHIFT + RIGHT_FWD_SHIFT));

	/* Right fwd captures. */
	prop |= (((board->white << RIGHT_FWD_SHIFT) & board->black) << (2 * RIGHT_FWD_SHIFT)) & ~COLUMN9;
	safe |= (m & ~prop);

	return(safe);
}


/*
 * Return the mask of squares that white can safely move to.
 */
BITBOARD white_safe_moveto(BOARD *board)
{
	BITBOARD free, m, safe, prop;

	free = ~(board->black | board->white) & ALL_SQUARES;

	/* Right back moves. */
	safe = (board->white >> RIGHT_BACK_SHIFT) & free;

	/* Test for LF captures. */
	safe &= ~(board->black << LEFT_FWD_SHIFT);

	/* Test for RF captures. */
	safe &= ~((board->black << RIGHT_FWD_SHIFT) & (free >> LEFT_BACK_SHIFT));

	/* Test for LB captures. */
	safe &= ~((board->black >> LEFT_BACK_SHIFT) & (free << RIGHT_FWD_SHIFT));

	/* Make sure this white man is not protecting another man from capture. */
	/* Right fwd captures. */
	prop = ((board->black << RIGHT_FWD_SHIFT) & board->white) >> 1;

	/* Left back captures. */
	prop |= (((board->black >> LEFT_BACK_SHIFT) & board->white) >> (RIGHT_BACK_SHIFT + LEFT_BACK_SHIFT));

	/* Right back captures. */
	prop |= (((board->black >> RIGHT_BACK_SHIFT) & board->white) >> (2 * RIGHT_BACK_SHIFT)) & ~(ROW0 | COLUMN9);
	safe &= ~prop;

	/* Left back moves. */
	m = (board->white >> LEFT_BACK_SHIFT) & free;

	/* Test for RF captures. */
	m &= ~(board->black << RIGHT_FWD_SHIFT);

	/* Test for LF captures. */
	m &= ~((board->black << LEFT_FWD_SHIFT) & (free >> RIGHT_BACK_SHIFT));

	/* Test for RB captures. */
	m &= ~((board->black >> RIGHT_BACK_SHIFT) & (free << LEFT_FWD_SHIFT));

	/* Make sure this white man is not protecting another man from capture. */
	/* Left fwd captures. */
	prop = (((board->black << LEFT_FWD_SHIFT) & board->white) << 1);

	/* Right back captures. */
	prop |= (((board->black >> RIGHT_BACK_SHIFT) & board->white) >> (RIGHT_BACK_SHIFT + LEFT_BACK_SHIFT));

	/* Left back captures. */
	prop |= (((board->black >> LEFT_BACK_SHIFT) & board->white) >> (2 * LEFT_BACK_SHIFT)) & ~COLUMN0;
	safe |= (m & ~prop);

	return(safe);
}


int build_movelist(BOARD *board, int color, MOVELIST *movelist)
{
	int count;

	count = build_jump_list<false>(board, movelist, color);
	if (!count)
		count = build_nonjump_list(board, movelist, color);
	return(count);
}


void get_fromto(BITBOARD fromboard, BITBOARD toboard, int *fromsq, int *tosq)
{
	BITBOARD from, to;

	from = fromboard &~toboard;
	*fromsq = bitboard_to_square0(from);
	to = ~fromboard & toboard;
	*tosq = bitboard_to_square0(to);
}


void get_fromto(BOARD *fromboard, BOARD *toboard, int color, int *fromsq, int *tosq)
{
	get_fromto(fromboard->pieces[color], toboard->pieces[color] , fromsq, tosq);
}


