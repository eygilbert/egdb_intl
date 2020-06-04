#include "builddb/indexing.h"
#include "egdb/egdb_intl.h"
#include "engine/bicoef.h"
#include "engine/bitcount.h"
#include "engine/board.h"
#include "engine/bool.h"
#include <algorithm>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>

namespace egdb_interface {

BITBOARD free_square_bitboard_fwd(int logical_square, BITBOARD occupied)
{
	int skipcount;
	BITBOARD bitboard, behind, empty_ahead;

	occupied |= GHOSTS;
	bitboard = (BITBOARD)1 << logical_square;
	behind = bitboard - 1;
	empty_ahead = ~(occupied | behind);
	skipcount = bitcount64(behind & occupied);

	while (skipcount) {
		--skipcount;
		empty_ahead &= (empty_ahead - 1);
	}

	return((BITBOARD)1 << LSB64(empty_ahead));
}


BITBOARD free_square_bitboard_rev(int logical_square, BITBOARD occupied)
{
	int skipcount;
	BITBOARD bitboard, ahead, behind, empty_ahead;

	occupied |= GHOSTS;
	bitboard = (BITBOARD)1 << (mirrored(logical_square) + 1);
	ahead = bitboard - 1;
	empty_ahead = ~occupied & ahead;
	behind = ~ahead;
	skipcount = bitcount64(behind & occupied);

	while (skipcount) {
		--skipcount;
		empty_ahead ^= ((BITBOARD)1 << MSB64(empty_ahead));
	}

	return((BITBOARD)1 << MSB64(empty_ahead));
}


BITBOARD index2bitboard_fwd(unsigned int index, int num_squares, int first_square, int num_pieces)
{
	int piece, logical_square;
	BITBOARD bitboard;

	logical_square = num_squares - 1;
	bitboard = 0;
	for (piece = num_pieces; piece > 0; --piece) {
		while (choose(logical_square, piece) > index)
			logical_square--;
		index -= choose(logical_square, piece);
		bitboard |= square0_to_bitboard(logical_square + first_square);
	}
	return(bitboard);
}


BITBOARD index2bitboard_fwd(unsigned int index, int num_squares, int num_pieces, BITBOARD occupied)
{
	int piece, logical_square;
	BITBOARD bitboard;

	occupied |= GHOSTS;
	logical_square = num_squares - 1;
	bitboard = 0;
	for (piece = num_pieces; piece > 0; --piece) {
		while (choose(logical_square, piece) > index)
			logical_square--;
		index -= choose(logical_square, piece);
		bitboard |= free_square_bitboard_fwd(logical_square, occupied);
	}
	return(bitboard);
}


BITBOARD index2bitboard_rev(unsigned int index, int num_squares, int num_pieces, BITBOARD occupied)
{
	int piece, logical_square;
	BITBOARD bitboard;

	occupied |= GHOSTS;
	logical_square = num_squares - 1;
	bitboard = 0;
	for (piece = num_pieces; piece > 0; --piece) {
		while (choose(logical_square, piece) > index)
			logical_square--;
		index -= choose(logical_square, piece);
		bitboard |= free_square_bitboard_rev(logical_square, occupied);
	}
	return(bitboard);
}


/* base man index[bm][wm][bm0] */
static int64_t man_index_base[MAXPIECE + 1][MAXPIECE + 1][RANK0MAX + 1];


int64_t position_to_index_slice(EGDB_POSITION const *p, int bm, int bk, int wm, int wk)
{
	BITBOARD bm0_mask;
	BITBOARD bmmask, bkmask, wmmask, wkmask;
	int bm0;
	uint32_t bmindex, bkindex, wmindex, wkindex, bm0index;
	uint32_t bmrange, bm0range, bkrange, wkrange;
	int64_t index64;
	int64_t checker_index_base, checker_index;

	bmmask = p->black & ~p->king;
	bm0_mask = bmmask & ROW0;
	bm0 = bitcount64(bm0_mask);

	/* Indices in this subdb are assigned with index 0 having the highest
	 * number of black men on rank0, ...
	 */
	checker_index_base = man_index_base[bm][wm][bm0];

	/* Set the index for the black men that are not on rank0. */
	bmindex = index_pieces_1_type(bmmask ^ bm0_mask, ROWSIZE);

	/* Set the index for the black men that are on rank0. */
	bm0index = index_pieces_1_type(bm0_mask, 0);

	/* Set the index for the white men,
	 * accounting for any interferences from the black men.
	 */
	wmmask = p->white & ~p->king;
	wmindex = index_pieces_1_type_reverse(wmmask, bmmask);

	/* Set the index for the black kings, accounting for any interferences
	 * from the black men and white men.
	 */
	bkmask = p->black & p->king;
	bkindex = index_pieces_1_type(bkmask, bmmask | wmmask);

	/* Set the index for the white kings, accounting for any interferences
	 * from the black men, white men, and black kings.
	 */
	wkmask = p->white & p->king;
	wkindex = index_pieces_1_type(wkmask, bmmask | wmmask | bkmask);

	bmrange = choose(MAXSQUARE - 2 * ROWSIZE, bm - bm0);
	bm0range = choose(ROWSIZE, bm0);
	bkrange = choose(MAXSQUARE - bm - wm, bk);
	wkrange = choose(MAXSQUARE - bm - wm - bk, wk);

	/* Calculate the checker (man) index. */
	checker_index = bm0index + checker_index_base +
					bmindex * bm0range +
					(int64_t)wmindex * (int64_t)bm0range * (int64_t)bmrange;

	index64 = (int64_t)wkindex + 
				(int64_t)bkindex * (int64_t)wkrange +
				(int64_t)(checker_index) * (int64_t)bkrange * (int64_t)wkrange;
	return(index64);
}


void indextoposition_slice(int64_t index, EGDB_POSITION *p, int bm, int bk, int wm, int wk)
{
	int bm0;
	BITBOARD bmmask, bkmask, wmmask, wkmask;
	uint32_t bmindex, bkindex, wmindex, wkindex, bm0index;
	uint32_t bmrange, bm0range, bkrange, wkrange;
	int64_t multiplier, checker_index;

	/* Initialize the board. */
	bmmask = 0;
	bkmask = 0;
	wmmask = 0;
	wkmask = 0;

	bkrange = choose(MAXSQUARE - bm - wm, bk);
	wkrange = choose(MAXSQUARE - bm - wm - bk, wk);

	/* Indices in this subdb are assigned with index 0 having the highest
	* number of black men on rank0, ...  Find the number of black men on rank0.
	*/
	multiplier = (int64_t)bkrange * (int64_t)wkrange;
	checker_index = (index / multiplier);
	index -= checker_index * multiplier;

	/* Find bm0. */
	for (bm0 = (std::min)(bm, ROWSIZE); bm0 > 0; --bm0)
		if (man_index_base[bm][wm][bm0 - 1] > checker_index)
			break;

	checker_index -= man_index_base[bm][wm][bm0];

	/* Compute the range of indices for each piece type. */
	bmrange = choose(MAXSQUARE - 2 * ROWSIZE, bm - bm0);
	bm0range = choose(ROWSIZE, bm0);

	/* Extract the wmindex, bmindex, and bm0index. */
	multiplier = bmrange * bm0range;
	wmindex = (uint32_t)(checker_index / multiplier);
	checker_index -= wmindex * multiplier;

	multiplier = bm0range;
	bmindex = (uint32_t)(checker_index / multiplier);
	checker_index -= bmindex * multiplier;
	bm0index = (uint32_t)checker_index;

	/* Get bkindex and wkindex. */
	bkindex = (uint32_t)(index / wkrange);
	index -= (int64_t)bkindex * (int64_t)wkrange;

	wkindex = (uint32_t)(index);

	/* Place black men, except those on rank0. */
	bmmask = index2bitboard_fwd(bmindex, MAXSQUARE - 2 * ROWSIZE, ROWSIZE, bm - bm0);

	/* Place black men on rank0. */
	bmmask |= index2bitboard_fwd(bm0index, ROWSIZE, 0, bm0);

	/* Place white men. */
	wmmask = index2bitboard_rev(wmindex, MAXSQUARE - ROWSIZE, wm, bmmask);

	/* Place black kings. */
	bkmask = index2bitboard_fwd(bkindex, MAXSQUARE, bk, bmmask | wmmask);

	/* Place white kings. */
	wkmask = index2bitboard_fwd(wkindex, MAXSQUARE, wk, bmmask | wmmask | bkmask);

	p->black = bmmask | bkmask;
	p->white = wmmask | wkmask;
	p->king = bkmask | wkmask;
}


void build_man_index_base()
{
	int bm, wm, bm0;
	int64_t base, partial;

	for (bm = 0; bm <= MAXPIECE; ++bm) {
		for (wm = 0; wm <= MAXPIECE; ++wm) {
			base = 0;
			for (bm0 = (std::min)(ROWSIZE, bm); bm0 >= 0; --bm0) {
				man_index_base[bm][wm][bm0] = base;

				/* First the number of men configurations excluding black's backrank. */
				partial = (int64_t)choose(MAXSQUARE - 2 * ROWSIZE, bm - bm0) * (int64_t)choose(MAXSQUARE - ROWSIZE - bm + bm0, wm);

				/* Add the contribution from black's kingrow. */
				partial *= choose(ROWSIZE, bm0);
				base += partial;
			}
		}
	}
}


/* return the number of database indices for this subdb.
 * needs choose from n, k
 */
int64_t getdatabasesize_slice(int bm, int bk, int wm, int wk)
{
	int64_t size;
	int64_t partial;
	int black_men_backrank;

	/* Compute the number of men configurations first.  Place the black men, summing separately
	 * the contributions of 0, 1, 2, 3, 4, or 5 black men on the backrank.
	 */
	size = 0;
	for (black_men_backrank = 0; black_men_backrank <= (std::min)(bm, ROWSIZE); ++black_men_backrank) {

		/* First the number of men configurations excluding black's backrank. */
		partial = (int64_t)choose(MAXSQUARE - 2 * ROWSIZE, bm - black_men_backrank) * (int64_t)choose(MAXSQUARE - ROWSIZE - bm + black_men_backrank, wm);

		/* Add the contribution from black's kingrow. */
		if (black_men_backrank)
			partial *= choose(ROWSIZE, black_men_backrank);

		size += partial;
	}

	/* Add the black kings. */
	size *= choose(MAXSQUARE - bm - wm, bk);

	/* Add the white kings. */
	size *= choose(MAXSQUARE - bm - wm - bk, wk);

	return(size);
}


/*
 * Return the size with gaps (ignore interferences between men) of a major slice.
 */
int64_t getslicesize_gaps(int bm, int bk, int wm, int wk)
{
	int64_t size;

	/* Black men. */
	size = choose(NUMSQUARES - ROWSIZE, bm);

	/* White men. */
	size *= choose(NUMSQUARES - ROWSIZE, wm);

	/* Add the black kings. */
	size *= choose(NUMSQUARES - bm - wm, bk);

	/* Add the white kings. */
	size *= choose(NUMSQUARES - bm - wm - bk, wk);

	return(size);
}


}	// namespace egdb_interface {

