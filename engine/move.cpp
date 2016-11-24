#include "egdb/platform.h"
#include "engine/bitcount.h"
#include "engine/move.h"
#include "engine/board.h"
#include "engine/bool.h"
#include "engine/move_api.h"
#include "engine/project.h"	// ARRAY_SIZE
#include <cstdio>
#include <cstring>

namespace egdb_interface {


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

void init_move_tables();

class init_move_tables_c {
public:
	init_move_tables_c(void) {init_move_tables();}
};

static init_move_tables_c init;

extern DIAGS diag_tbl[NUM_BITBOARD_BITS];

struct Local {
	MOVELIST *movelist;
	CAPTURE_INFO *cap;
	BITBOARD free;
	BOARD intermediate;
	int largest_num_jumps;
};

template <bool save_capture_info, int color>
static void man_jump(BITBOARD all_jumped, MOVELIST *movelist, BITBOARD from, int num_jumps, Local *local);

template <int color>
void add_king_capture_path(int num_jumps, BITBOARD jumper, BITBOARD jumped, Local *local)
{
	BOARD *board = local->cap[local->movelist->count].path + num_jumps;
	if (color == BLACK) {
		board->black = local->intermediate.black | jumper;
		board->white = local->intermediate.white & ~jumped;
		board->king = (local->intermediate.king | jumper) & ~jumped;
	}
	else {
		board->white = local->intermediate.white | jumper;
		board->black = local->intermediate.black & ~jumped;
		board->king = (local->intermediate.king | jumper) & ~jumped;
	}
}


template <bool save_capture_info, int color>
void lf_king_capture(BITBOARD jumped, BITBOARD jumper, int num_jumps, Local *local)
{
	int bitnum;
	BITBOARD new_jumper, mask, jumps, opponents;

	/* Loop over each empty square in the captured direction. */
	new_jumper = jumper;
	if (color == BLACK)
		opponents = local->intermediate.white & ~jumped;
	else
		opponents = local->intermediate.black & ~jumped;
	while (1) {
		if (save_capture_info)
			add_king_capture_path<color>(num_jumps, new_jumper, jumped, local);

		/* Get occupied squares along the rf diagonal. */
		bitnum = LSB64(new_jumper);
		mask = (rf_diag << bitnum) & ~local->free;

		/* Get closest occupied square. */
		jumps = mask & -(SIGNED_BITBOARD)mask;

		/* Capture is possible if the next rf square is free. */
		if (jumps & opponents & (local->free >> RIGHT_FWD_SHIFT))
			rf_king_capture<save_capture_info, color>(jumped | jumps, jumps << RIGHT_FWD_SHIFT, num_jumps + 1, local);

		/* Get occupied squares along the lb diagonal. */
		mask = (lb_diag >> (S50_bit - bitnum)) & ~local->free;

		/* Get closest occupied square. */
		jumps = (BITBOARD)1 << MSB64(mask);

		/* Capture is possible if the next lb square is free. */
		if (jumps & opponents & (local->free << LEFT_BACK_SHIFT))
			lb_king_capture<save_capture_info, color>(jumped | jumps, jumps >> LEFT_BACK_SHIFT, num_jumps + 1, local);

		/* Save this move if we didn't find a continuation above. */
		if (num_jumps >= local->largest_num_jumps)
			add_king_capture<save_capture_info, color>(new_jumper, jumped, num_jumps, local);

		new_jumper <<= LEFT_FWD_SHIFT;

		/* Exit when reach an occupied or off-the-board square. */
		if (!(new_jumper & local->free))
			break;
	}

	/* Reached an occupied or off-the-board square. See if it's an opponent and following is a free square. */
	if (new_jumper & opponents & (local->free >> LEFT_FWD_SHIFT)) {
		if (save_capture_info)
			add_king_capture_path<color>(num_jumps, jumper, jumped, local);

		lf_king_capture<save_capture_info, color>(jumped | new_jumper, new_jumper << LEFT_FWD_SHIFT, num_jumps + 1, local);
	}
}


template <bool save_capture_info, int color>
void rf_king_capture(BITBOARD jumped, BITBOARD jumper, int num_jumps, Local *local)
{
	int bitnum;
	BITBOARD new_jumper, mask, jumps, opponents;

	/* Loop over each empty square in the captured direction. */
	new_jumper = jumper;
	if (color == BLACK)
		opponents = local->intermediate.white & ~jumped;
	else
		opponents = local->intermediate.black & ~jumped;
	while (1) {
		if (save_capture_info)
			add_king_capture_path<color>(num_jumps, new_jumper, jumped, local);

		/* Get occupied squares along the lf diagonal. */
		bitnum = LSB64(new_jumper);
		mask = (lf_diag << bitnum) & ~local->free;

		/* Get closest occupied square. */
		jumps = mask & -(SIGNED_BITBOARD)mask;

		/* Capture is possible if the next lf square is free. */
		if (jumps & opponents & (local->free >> LEFT_FWD_SHIFT))
			lf_king_capture<save_capture_info, color>(jumped | jumps, jumps << LEFT_FWD_SHIFT, num_jumps + 1, local);

		/* Get occupied squares along the rb diagonal. */
		mask = (rb_diag >> (S50_bit - bitnum)) & ~local->free;

		/* Get closest occupied square. */
		jumps = (BITBOARD)1 << MSB64(mask);

		/* Capture is possible if the next rb square is free. */
		if (jumps & opponents & (local->free << RIGHT_BACK_SHIFT))
			rb_king_capture<save_capture_info, color>(jumped | jumps, jumps >> RIGHT_BACK_SHIFT, num_jumps + 1, local);

		/* Save this move if we didn't find a continuation above. */
		if (num_jumps >= local->largest_num_jumps)
			add_king_capture<save_capture_info, color>(new_jumper, jumped, num_jumps, local);

		new_jumper <<= RIGHT_FWD_SHIFT;

		/* Exit when reach an occupied or off-the-board square. */
		if (!(new_jumper & local->free))
			break;
	}

	/* Reached an occupied or off-the-board square. See if it's an opponent and following is a free square. */
	if (new_jumper & opponents & (local->free >> RIGHT_FWD_SHIFT)) {
		if (save_capture_info)
			add_king_capture_path<color>(num_jumps, jumper, jumped, local);

		rf_king_capture<save_capture_info, color>(jumped | new_jumper, new_jumper << RIGHT_FWD_SHIFT, num_jumps + 1, local);
	}
}


template <bool save_capture_info, int color>
void lb_king_capture(BITBOARD jumped, BITBOARD jumper, int num_jumps, Local *local)
{
	int bitnum;
	BITBOARD new_jumper, mask, jumps, opponents;

	/* Loop over each empty square in the captured direction. */
	new_jumper = jumper;
	if (color == BLACK)
		opponents = local->intermediate.white & ~jumped;
	else
		opponents = local->intermediate.black & ~jumped;
	while (1) {
		if (save_capture_info)
			add_king_capture_path<color>(num_jumps, new_jumper, jumped, local);

		/* Get occupied squares along the lf diagonal. */
		bitnum = LSB64(new_jumper);
		mask = (lf_diag << bitnum) & ~local->free;

		/* Get closest occupied square. */
		jumps = mask & -(SIGNED_BITBOARD)mask;

		/* Capture is possible if the next lf square is free. */
		if (jumps & opponents & (local->free >> LEFT_FWD_SHIFT))
			lf_king_capture<save_capture_info, color>(jumped | jumps, jumps << LEFT_FWD_SHIFT, num_jumps + 1, local);

		/* Get occupied squares along the rb diagonal. */
		mask = (rb_diag >> (S50_bit - bitnum)) & ~local->free;

		/* Get closest occupied square. */
		jumps = (BITBOARD)1 << MSB64(mask);

		/* Capture is possible if the next lb square is free. */
		if (jumps & opponents & (local->free << RIGHT_BACK_SHIFT))
			rb_king_capture<save_capture_info, color>(jumped | jumps, jumps >> RIGHT_BACK_SHIFT, num_jumps + 1, local);

		/* Save this move if we didn't find a continuation above. */
		if (num_jumps >= local->largest_num_jumps)
			add_king_capture<save_capture_info, color>(new_jumper, jumped, num_jumps, local);

		new_jumper >>= LEFT_BACK_SHIFT;

		/* Exit when reach an occupied or off-the-board square. */
		if (!(new_jumper & local->free))
			break;
	}

	/* Reached an occupied or off-the-board square. See if it's an opponent and following is a free square. */
	if (new_jumper & opponents & (local->free << LEFT_BACK_SHIFT)) {
		if (save_capture_info)
			add_king_capture_path<color>(num_jumps, jumper, jumped, local);

		lb_king_capture<save_capture_info, color>(jumped | new_jumper, new_jumper >> LEFT_BACK_SHIFT, num_jumps + 1, local);
	}
}


template <bool save_capture_info, int color>
void rb_king_capture(BITBOARD jumped, BITBOARD jumper, int num_jumps, Local *local)
{
	int bitnum;
	BITBOARD new_jumper, mask, jumps, opponents;

	/* Loop over each empty square in the captured direction. */
	new_jumper = jumper;
	if (color == BLACK)
		opponents = local->intermediate.white & ~jumped;
	else
		opponents = local->intermediate.black & ~jumped;
	while (1) {
		if (save_capture_info)
			add_king_capture_path<color>(num_jumps, new_jumper, jumped, local);

		/* Get occupied squares along the rf diagonal. */
		bitnum = LSB64(new_jumper);
		mask = (rf_diag << bitnum) & ~local->free;

		/* Get closest occupied square. */
		jumps = mask & -(SIGNED_BITBOARD)mask;

		/* Capture is possible if the next rf square is free. */
		if (jumps & opponents & (local->free >> RIGHT_FWD_SHIFT))
			rf_king_capture<save_capture_info, color>(jumped | jumps, jumps << RIGHT_FWD_SHIFT, num_jumps + 1, local);

		/* Get occupied squares along the lb diagonal. */
		mask = (lb_diag >> (S50_bit - bitnum)) & ~local->free;

		/* Get closest occupied square. */
		jumps = (BITBOARD)1 << MSB64(mask);

		/* Capture is possible if the next lb square is free. */
		if (jumps & opponents & (local->free << LEFT_BACK_SHIFT))
			lb_king_capture<save_capture_info, color>(jumped | jumps, jumps >> LEFT_BACK_SHIFT, num_jumps + 1, local);

		/* Save this move if we didn't find a continuation above. */
		if (num_jumps >= local->largest_num_jumps)
			add_king_capture<save_capture_info, color>(new_jumper, jumped, num_jumps, local);

		new_jumper >>= RIGHT_BACK_SHIFT;

		/* Exit when reach an occupied or off-the-board square. */
		if (!(new_jumper & local->free))
			break;
	}

	/* Reached an occupied or off-the-board square. See if it's an opponent and following is a free square. */
	if (new_jumper & opponents & (local->free << RIGHT_BACK_SHIFT)) {
		if (save_capture_info)
			add_king_capture_path<color>(num_jumps, jumper, jumped, local);

		rb_king_capture<save_capture_info, color>(jumped | new_jumper, new_jumper >> RIGHT_BACK_SHIFT, num_jumps + 1, local);
	}
}


template <bool save_capture_info, int color>
void add_king_capture(BITBOARD jumper, BITBOARD jumped, int num_jumps, Local *local)
{
	if (num_jumps >= local->largest_num_jumps) {
		int i;
		MOVELIST *movelist = local->movelist;

		if (num_jumps > local->largest_num_jumps) {
			if (save_capture_info)
				std::memcpy(local->cap, local->cap + movelist->count, sizeof(local->cap[0]));
			movelist->count = 0;
			local->largest_num_jumps = num_jumps;
		}

		if (color == BLACK) {
			movelist->board[movelist->count].black = local->intermediate.black | jumper;
			movelist->board[movelist->count].white = local->intermediate.white & ~jumped;
			movelist->board[movelist->count].king = (local->intermediate.king | jumper) & ~jumped;
		}
		else {
			movelist->board[movelist->count].white = local->intermediate.white | jumper;
			movelist->board[movelist->count].black = local->intermediate.black & ~jumped;
			movelist->board[movelist->count].king = (local->intermediate.king | jumper) & ~jumped;
		}

		if (save_capture_info) {
			local->cap[movelist->count].capture_count = num_jumps;

			/* Copy the path to the next potential jump move. */
			copy_path(local->cap, movelist);
		}

		/* Remove duplicate moves (same result board). */
		if (num_jumps >= 4) {
			for (i = 0; i < movelist->count; ++i)
				if (memcmp(movelist->board + i, movelist->board + movelist->count, sizeof(BOARD)) == 0)
					return;
		}
		movelist->count++;
	}
}


static void copy_path(CAPTURE_INFO *cap, MOVELIST *movelist)
{
	/* Copy the path to the next potential jump move. */
	std::memcpy(cap[movelist->count + 1].path, cap[movelist->count].path, sizeof(cap[0].path));
}


const BITBOARD lf_diag = (S1 << LEFT_FWD_SHIFT) | (S1 << 2 * LEFT_FWD_SHIFT) |
						(S1 << 3 * LEFT_FWD_SHIFT) | (S1 << 4 * LEFT_FWD_SHIFT) |
						(S1 << 5 * LEFT_FWD_SHIFT) | (S1 << 6 * LEFT_FWD_SHIFT) |
						(S1 << 7 * LEFT_FWD_SHIFT) | (S1 << 8 * LEFT_FWD_SHIFT);
const BITBOARD rf_diag = (S1 << RIGHT_FWD_SHIFT) | (S1 << 2 * RIGHT_FWD_SHIFT) |
						(S1 << 3 * RIGHT_FWD_SHIFT) | (S1 << 4 * RIGHT_FWD_SHIFT) |
						(S1 << 5 * RIGHT_FWD_SHIFT) | (S1 << 6 * RIGHT_FWD_SHIFT) |
						(S1 << 7 * RIGHT_FWD_SHIFT) | (S1 << 8 * RIGHT_FWD_SHIFT);
const BITBOARD lb_diag = (S50 >> LEFT_BACK_SHIFT) | (S50 >> 2 * LEFT_BACK_SHIFT) |
						(S50 >> 3 * LEFT_BACK_SHIFT) | (S50 >> 4 * LEFT_BACK_SHIFT) |
						(S50 >> 5 * LEFT_BACK_SHIFT) | (S50 >> 6 * LEFT_BACK_SHIFT) |
						(S50 >> 7 * LEFT_BACK_SHIFT) | (S50 >> 8 * LEFT_BACK_SHIFT);
const BITBOARD rb_diag = (S50 >> RIGHT_BACK_SHIFT) | (S50 >> 2 * RIGHT_BACK_SHIFT) |
						(S50 >> 3 * RIGHT_BACK_SHIFT) | (S50 >> 4 * RIGHT_BACK_SHIFT) |
						(S50 >> 5 * RIGHT_BACK_SHIFT) | (S50 >> 6 * RIGHT_BACK_SHIFT) |
						(S50 >> 7 * RIGHT_BACK_SHIFT) | (S50 >> 8 * RIGHT_BACK_SHIFT);

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

#ifdef ENVIRONMENT64

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
	BITBOARD man, king, opponent;

	free = ~(board->black | board->white) & ALL_SQUARES;
	man = board->pieces[color] & ~board->king;
	opponent = board->pieces[OTHER_COLOR(color)];

	/* Test for man captures. */
	/* Jumps forward to the left. */
	mask = (((man << LEFT_FWD_SHIFT) & opponent) << LEFT_FWD_SHIFT);

	/* Jumps forward to the right. */
	mask |= (((man << RIGHT_FWD_SHIFT) & opponent) << RIGHT_FWD_SHIFT);

	/* Jumps back to the left. */
	mask |= (((man >> LEFT_BACK_SHIFT) & opponent) >> LEFT_BACK_SHIFT);

	/* Jumps back to the right. */
	mask |= (((man >> RIGHT_BACK_SHIFT) & opponent) >> RIGHT_BACK_SHIFT);

	if (mask & free)
		return(1);

	/* Test for king captures. */
	king = board->pieces[color] & board->king;
	if (king) {
		if (has_king_jumps_fwd(king, free, opponent, LEFT_FWD_SHIFT))
			return(1);
		if (has_king_jumps_fwd(king, free, opponent, RIGHT_FWD_SHIFT))
			return(1);
		if (has_king_jumps_back(king, free, opponent, LEFT_BACK_SHIFT))
			return(1);
		if (has_king_jumps_back(king, free, opponent, RIGHT_BACK_SHIFT))
			return(1);
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
	BITBOARD man, king, opponent;

	free = ~(board->black | board->white) & ALL_SQUARES;
	man = board->pieces[color] & ~board->king;
	opponent = board->pieces[OTHER_COLOR(color)];

	/* Jumps forward to the left. */
	mask = (((man << LEFT_FWD_SHIFT) & opponent) << LEFT_FWD_SHIFT);

	/* Jumps forward to the right. */
	mask |= (((man << RIGHT_FWD_SHIFT) & opponent) << RIGHT_FWD_SHIFT);

	/* Jumps back to the left. */
	mask |= (((man >> LEFT_BACK_SHIFT) & opponent) >> LEFT_BACK_SHIFT);

	/* Jumps back to the right. */
	mask |= (((man >> RIGHT_BACK_SHIFT) & opponent) >> RIGHT_BACK_SHIFT);

	if (mask & free)
		return(1);

	king = board->pieces[color] & board->king;
	while (king) {

		/* Strip off the next king. */
		mask = king & -(SIGNED_BITBOARD)king;
		king &= ~mask;
		bitnum = LSB64(mask);

		if (has_king_jumps(diag_tbl[bitnum].lf, free, opponent, diag_tbl[bitnum].lflen))
			return(1);
		if (has_king_jumps(diag_tbl[bitnum].rf, free, opponent, diag_tbl[bitnum].rflen))
			return(1);
		if (has_king_jumps(diag_tbl[bitnum].lb, free, opponent, diag_tbl[bitnum].lblen))
			return(1);
		if (has_king_jumps(diag_tbl[bitnum].rb, free, opponent, diag_tbl[bitnum].rblen))
			return(1);
	}
	return(0);
}
#endif

#define BLACK_KING_MOVE(shift_op, revshift_op, shift) \
	/* Left forward kings. */	\
	mask = (bk shift_op shift) & free;	\
\
	/* Add each to the list. */	\
	while (mask) {	\
		to = mask & -(SIGNED_BITBOARD)mask;	\
		mask -= to;	\
		from = to revshift_op shift;	\
		while (1) {	\
			mlp->black = board->black - from + to;	\
			mlp->king = board->king - from + to;	\
			mlp->white = board->white;	\
			++mlp;	\
			to = (to shift_op shift) & free; \
			if (!to)	\
				break;	\
		}	\
	}

#define WHITE_KING_MOVE(shift_op, revshift_op, shift) \
	/* Left forward kings. */	\
	mask = (wk shift_op shift) & free;	\
\
	/* Add each to the list. */	\
	while (mask) {	\
		to = mask & -(SIGNED_BITBOARD)mask;	\
		mask -= to;	\
		from = to revshift_op shift;	\
		while (1) {	\
			mlp->white = board->white - from + to;	\
			mlp->king = board->king - from + to;	\
			mlp->black = board->black;	\
			++mlp;	\
			to = (to shift_op shift) & free; \
			if (!to)	\
				break;	\
		}	\
	}

/*
 * Build a movelist of non-jump moves.
 * Return the number of moves in the list.
 */
int build_nonjump_list(BOARD *board, MOVELIST *movelist, int color)
{
	BITBOARD free;
	BITBOARD mask;
	BITBOARD bk, wk;
	BITBOARD from, to;
	BOARD *mlp;

	mlp = movelist->board;
	free = ~(board->black | board->white) & ALL_SQUARES;
	if (color == BLACK) {

		/* Left forward moves. */
		mask = ((board->black & ~board->king) << LEFT_FWD_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to >> RIGHT_BACK_SHIFT;

			/* Add it to the movelist. */
			mlp->black = (board->black & ~from) | to;
			mlp->white = board->white;
			mlp->king = board->king | (to & BLACK_KING_RANK_MASK);
			++mlp;
		}

		/* Right forward moves. */
		mask = ((board->black & ~board->king) << RIGHT_FWD_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to >> LEFT_BACK_SHIFT;

			/* Add it to the movelist. */
			mlp->black = (board->black & ~from) | to;
			mlp->white = board->white;
			mlp->king = board->king | (to & BLACK_KING_RANK_MASK);
			++mlp;
		}

		/* Black kings. */
		bk = board->black & board->king;
		if (bk) {
			BLACK_KING_MOVE(<<, >>, LEFT_FWD_SHIFT);
			BLACK_KING_MOVE(<<, >>, RIGHT_FWD_SHIFT);
			BLACK_KING_MOVE(>>, <<, LEFT_BACK_SHIFT);
			BLACK_KING_MOVE(>>, <<, RIGHT_BACK_SHIFT);
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

			/* Add it to the movelist. */
			mlp->white = (board->white & ~from) | to;
			mlp->black = board->black;
			mlp->king = board->king | (to & WHITE_KING_RANK_MASK);
			++mlp;
		}

		/* Right back moves. */
		mask = ((board->white & ~board->king) >> RIGHT_BACK_SHIFT) & free;
		for ( ; mask; mask = mask & (mask - 1)) {

			/* Peel off the lowest set bit in mask. */
			to = mask & -(SIGNED_BITBOARD)mask;
			from = to << LEFT_FWD_SHIFT;

			/* Add it to the movelist. */
			mlp->white = (board->white & ~from) | to;
			mlp->black = board->black;
			mlp->king = board->king | (to & WHITE_KING_RANK_MASK);
			++mlp;
		}

		/* White kings. */
		wk = board->white & board->king;
		if (wk) {
			WHITE_KING_MOVE(<<, >>, LEFT_FWD_SHIFT);
			WHITE_KING_MOVE(<<, >>, RIGHT_FWD_SHIFT);
			WHITE_KING_MOVE(>>, <<, LEFT_BACK_SHIFT);
			WHITE_KING_MOVE(>>, <<, RIGHT_BACK_SHIFT);
		}
	}
	movelist->count = (int)(mlp - movelist->board);
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
	int bitnum, i, len, opp_color;
	BITBOARD free;
	BITBOARD king, opponent;
	BITBOARD from;
	BITBOARD *pdiag;
	BOARD *mlp;

	movelist->count = 0;
	free = ~(board->black | board->white);
	king = board->pieces[color] & board->king;
	opp_color = OTHER_COLOR(color);
	opponent = board->pieces[opp_color];

	while (king) {

		/* Strip off the next black king. */
		from = king & -(SIGNED_BITBOARD)king;
		king &= ~from;
		bitnum = LSB64(from);

		pdiag = diag_tbl[bitnum].lf;
		len = diag_tbl[bitnum].lflen;
		for (i = 0; i < len; ++i) {
			if (pdiag[i] & free) {
				mlp = movelist->board + movelist->count++;
				mlp->pieces[opp_color] = board->pieces[opp_color];
				mlp->pieces[color] = board->pieces[color] & ~from;
				mlp->pieces[color] |= pdiag[i];
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
				mlp->pieces[opp_color] = board->pieces[opp_color];
				mlp->pieces[color] = board->pieces[color] & ~from;
				mlp->pieces[color] |= pdiag[i];
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
				mlp->pieces[opp_color] = board->pieces[opp_color];
				mlp->pieces[color] = board->pieces[color] & ~from;
				mlp->pieces[color] |= pdiag[i];
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
				mlp->pieces[opp_color] = board->pieces[opp_color];
				mlp->pieces[color] = board->pieces[color] & ~from;
				mlp->pieces[color] |= pdiag[i];
				mlp->king = board->king & ~from;
				mlp->king |= pdiag[i];
			}
			else
				break;
		}
	}
	return(movelist->count);
}

#define BLACK_MAN_JUMP(shift_op, revshift_op, shift)	\
	mask = (((bm shift_op shift) & board->white) shift_op shift) & free;	\
	for ( ; mask; mask = mask & (mask - 1)) {	\
		/* Peel off the lowest set bit in mask. */	\
		lands = mask & -(SIGNED_BITBOARD)mask;	\
		jumps = lands revshift_op shift;	\
		from = jumps revshift_op shift;		\
		local.free = free | from;		\
		local.intermediate.black = board->black & ~from;	\
		if (save_capture_info)	\
			cap[movelist->count].path[0] = *board;	\
		man_jump<save_capture_info, BLACK>(jumps, movelist, lands, 1, &local);	\
	}

#define WHITE_MAN_JUMP(shift_op, revshift_op, shift)	\
	mask = (((wm shift_op shift) & board->black) shift_op shift) & free;	\
	for ( ; mask; mask = mask & (mask - 1)) {	\
		/* Peel off the lowest set bit in mask. */	\
		lands = mask & -(SIGNED_BITBOARD)mask;	\
		jumps = lands revshift_op shift;	\
		from = jumps revshift_op shift;		\
		local.free = free | from;		\
		local.intermediate.white = board->white & ~from;	\
		if (save_capture_info)	\
			cap[movelist->count].path[0] = *board;	\
		man_jump<save_capture_info, WHITE>(jumps, movelist, lands, 1, &local);	\
	}

/*
 * Build a movelist of jump moves.
 * Return the number of moves in the list.
 */
template <bool save_capture_info>
int build_jump_list(BOARD *board, MOVELIST *movelist, int color, CAPTURE_INFO cap[MAXMOVES])
{
	int bitnum;
	BITBOARD free;
	BITBOARD mask;
	BITBOARD bm, wm;
	BITBOARD bk, wk;
	BITBOARD from, jumps, lands;
	Local local;

	if (save_capture_info) {
		assert(cap != NULL);
	}
	movelist->count = 0;
	local.largest_num_jumps = 0;
	local.cap = cap;
	local.movelist = movelist;
	free = ~(board->black | board->white) & ALL_SQUARES;
	if (color == BLACK) {
		local.intermediate.white = board->white;
		local.intermediate.king = board->king;

		/* First do man jumps. */
		bm = board->black & ~board->king;
		BLACK_MAN_JUMP(<<, >>, LEFT_FWD_SHIFT);
		BLACK_MAN_JUMP(<<, >>, RIGHT_FWD_SHIFT);
		BLACK_MAN_JUMP(>>, <<, LEFT_BACK_SHIFT);
		BLACK_MAN_JUMP(>>, <<, RIGHT_BACK_SHIFT);

		bk = board->black & board->king;
		while (bk) {

			/* Strip off the next black king. */
			from = bk & -(SIGNED_BITBOARD)bk;
			bk &= ~from;
			bitnum = LSB64(from);
			local.free = free | from;

			if (save_capture_info) 
				cap[movelist->count].path[0] = *board;

			local.intermediate.black = board->black - from;
			local.intermediate.king = board->king - from;
			
			/* Get occupied squares along the lf diagonal. */
			mask = (lf_diag << bitnum) & ~free;

			/* Get closest occupied square. */
			jumps = mask & -(SIGNED_BITBOARD)mask;

			/* Capture is possible if the next lf square is free. */
			if (jumps & board->white & (free >> LEFT_FWD_SHIFT))
				lf_king_capture<save_capture_info, BLACK>(jumps, jumps << LEFT_FWD_SHIFT, 1, &local);

			/* Get occupied squares along the rf diagonal. */
			mask = (rf_diag << bitnum) & ~free;

			/* Get closest occupied square. */
			jumps = mask & -(SIGNED_BITBOARD)mask;

			/* Capture is possible if the next rf square is free. */
			if (jumps & board->white & (free >> RIGHT_FWD_SHIFT))
				rf_king_capture<save_capture_info, BLACK>(jumps, jumps << RIGHT_FWD_SHIFT, 1, &local);

			/* Get occupied squares along the lb diagonal. */
			mask = (lb_diag >> (S50_bit - bitnum)) & ~free;

			/* Get closest occupied square. */
			jumps = (BITBOARD)1 << MSB64(mask);

			/* Capture is possible if the next lb square is free. */
			if (jumps & board->white & (free << LEFT_BACK_SHIFT))
				lb_king_capture<save_capture_info, BLACK>(jumps, jumps >> LEFT_BACK_SHIFT, 1, &local);

			/* Get occupied squares along the rb diagonal. */
			mask = (rb_diag >> (S50_bit - bitnum)) & ~free;

			/* Get closest occupied square. */
			jumps = (BITBOARD)1 << MSB64(mask);

			/* Capture is possible if the next rb square is free. */
			if (jumps & board->white & (free << RIGHT_BACK_SHIFT))
				rb_king_capture<save_capture_info, BLACK>(jumps, jumps >> RIGHT_BACK_SHIFT, 1, &local);
		}	
	}
	else {
		local.intermediate.black = board->black;
		local.intermediate.king = board->king;

		/* First do man jumps. */
		wm = board->white & ~board->king;
		WHITE_MAN_JUMP(<<, >>, LEFT_FWD_SHIFT);
		WHITE_MAN_JUMP(<<, >>, RIGHT_FWD_SHIFT);
		WHITE_MAN_JUMP(>>, <<, LEFT_BACK_SHIFT);
		WHITE_MAN_JUMP(>>, <<, RIGHT_BACK_SHIFT);

		wk = board->white & board->king;
		while (wk) {

			/* Strip off the next white king. */
			from = wk & -(SIGNED_BITBOARD)wk;
			wk &= ~from;
			bitnum = LSB64(from);
			local.free = free | from;

			if (save_capture_info)
				cap[movelist->count].path[0] = *board;

			local.intermediate.white = board->white - from;
			local.intermediate.king = board->king - from;

			/* Get occupied squares along the lf diagonal. */
			mask = (lf_diag << bitnum) & ~free;

			/* Get closest occupied square. */
			jumps = mask & -(SIGNED_BITBOARD)mask;

			/* Capture is possible if the next lf square is free. */
			if (jumps & board->black & (free >> LEFT_FWD_SHIFT))
				lf_king_capture<save_capture_info, WHITE>(jumps, jumps << LEFT_FWD_SHIFT, 1, &local);

			/* Get occupied squares along the rf diagonal. */
			mask = (rf_diag << bitnum) & ~free;

			/* Get closest occupied square. */
			jumps = mask & -(SIGNED_BITBOARD)mask;

			/* Capture is possible if the next rf square is free. */
			if (jumps & board->black & (free >> RIGHT_FWD_SHIFT))
				rf_king_capture<save_capture_info, WHITE>(jumps, jumps << RIGHT_FWD_SHIFT, 1, &local);

			/* Get occupied squares along the lb diagonal. */
			mask = (lb_diag >> (S50_bit - bitnum)) & ~free;

			/* Get closest occupied square. */
			jumps = (BITBOARD)1 << MSB64(mask);

			/* Capture is possible if the next lb square is free. */
			if (jumps & board->black & (free << LEFT_BACK_SHIFT))
				lb_king_capture<save_capture_info, WHITE>(jumps, jumps >> LEFT_BACK_SHIFT, 1, &local);

			/* Get occupied squares along the rb diagonal. */
			mask = (rb_diag >> (S50_bit - bitnum)) & ~free;

			/* Get closest occupied square. */
			jumps = (BITBOARD)1 << MSB64(mask);

			/* Capture is possible if the next rb square is free. */
			if (jumps & board->black & (free << RIGHT_BACK_SHIFT))
				rb_king_capture<save_capture_info, WHITE>(jumps, jumps >> RIGHT_BACK_SHIFT, 1, &local);
		}	
	}
	return(movelist->count);
}
	
	
/*
 * We're in the middle of a man jump.
 * If we've reached the end of the jump, then add this board to the movelist.
 * If not, call again recursively.
 */
template <bool save_capture_info, int color>
static void man_jump(BITBOARD jumped, MOVELIST *movelist, BITBOARD jumper, int num_jumps, Local *local)
{
	BITBOARD jumps;
	BITBOARD lands;
	BITBOARD opponent;

	if (save_capture_info) {
		BOARD *board = local->cap[movelist->count].path + num_jumps;
		if (color == BLACK) {
			board->black = local->intermediate.black | jumper;
			board->white = local->intermediate.white & ~jumped;
			board->king = local->intermediate.king & ~jumped;
		}
		else {
			board->white = local->intermediate.white | jumper;
			board->black = local->intermediate.black & ~jumped;
			board->king = local->intermediate.king & ~jumped;
		}
	}

	if (color == BLACK)
		opponent = local->intermediate.white & ~jumped;
	else
		opponent = local->intermediate.black & ~jumped;

	jumps = (jumper << LEFT_FWD_SHIFT) & opponent;
	lands = (jumps << LEFT_FWD_SHIFT) & local->free;
	if (lands)
		man_jump<save_capture_info, color>(jumped | jumps, movelist, lands, num_jumps + 1, local);

	jumps = (jumper << RIGHT_FWD_SHIFT) & opponent;
	lands = (jumps << RIGHT_FWD_SHIFT) & local->free;
	if (lands)
		man_jump<save_capture_info, color>(jumped | jumps, movelist, lands, num_jumps + 1, local);

	jumps = (jumper >> LEFT_BACK_SHIFT) & opponent;
	lands = (jumps >> LEFT_BACK_SHIFT) & local->free;
	if (lands)
		man_jump<save_capture_info, color>(jumped | jumps, movelist, lands, num_jumps + 1, local);

	jumps = (jumper >> RIGHT_BACK_SHIFT) & opponent;
	lands = (jumps >> RIGHT_BACK_SHIFT) & local->free;
	if (lands)
		man_jump<save_capture_info, color>(jumped | jumps, movelist, lands, num_jumps + 1, local);

	/* If we didn't find any move jumps, then add the board that was
	 * passed in to the movelist.
	 */
	if (num_jumps >= local->largest_num_jumps) {
		if (num_jumps > local->largest_num_jumps) {
			if (save_capture_info)
				std::memcpy(local->cap, local->cap + movelist->count, sizeof(local->cap[0]));
			movelist->count = 0;
			local->largest_num_jumps = num_jumps;
		}

		movelist->board[movelist->count].white &= ~jumped;
		movelist->board[movelist->count].king &= ~jumped;
		if (save_capture_info)
			local->cap[movelist->count].capture_count = num_jumps;

		if (color == BLACK) {
			movelist->board[movelist->count].black = local->intermediate.black | jumper;
			movelist->board[movelist->count].white = local->intermediate.white & ~jumped;
			movelist->board[movelist->count].king = local->intermediate.king & ~jumped;
			if (jumper & BLACK_KING_RANK_MASK)
				movelist->board[movelist->count].king |= jumper;
		}
		else {
			movelist->board[movelist->count].white = local->intermediate.white | jumper;
			movelist->board[movelist->count].black = local->intermediate.black & ~jumped;
			movelist->board[movelist->count].king = local->intermediate.king & ~jumped;
			if (jumper & WHITE_KING_RANK_MASK)
				movelist->board[movelist->count].king |= jumper;
		}

		/* Copy the path to the next potential jump move. */
		if (save_capture_info) {
			copy_path(local->cap, movelist);

			/* Test of capture makes a man a king. */
			if (color == BLACK) {
				if (jumper & BLACK_KING_RANK_MASK)
					local->cap[movelist->count].path[num_jumps].king |= jumper;
			}
			else {
				if (jumper & WHITE_KING_RANK_MASK)
					local->cap[movelist->count].path[num_jumps].king |= jumper;
			}
		}

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


bool is_conversion_move(BOARD *from, BOARD *to, int color)
{
	uint64_t from_bits, to_bits, moved_bits;

	/* It is a conversion move if its a capture. */
	from_bits = from->black | from->white;
	to_bits = to->black | to->white;
	if (bitcount64(from_bits) > bitcount64(to_bits))
		return(true);

	/* Return true if there is a man move. */
	moved_bits = from_bits & ~to_bits;
	if (moved_bits & from->king)
		return(false);
	else
		return(true);
}

}   // namespace egdb_interface
