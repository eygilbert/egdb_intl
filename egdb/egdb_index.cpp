#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winbase.h>
#include "project.h"
#include "board.h"
#include "bool.h"
#include "egdb_intl.h"
#include "egdb_internal.h"
#include "bitcount.h"
#include "reverse.h"
#include "compression_tables.h"
#include "bicoef.h"
#include "indexing.h"
#include "crc.h"
#include "smp_enable.h"
#include "SpinLock.h"


#define MAXPIECE 5
#define MAXSQUARE NUMSQUARES
#define RANK0MAX 5


/* base man index[bm][wm][bm0] */
static int64 man_index_base[MAXPIECE + 1][MAXPIECE + 1][RANK0MAX + 1];


int64 position_to_index_slice(EGDB_POSITION *p, int bm, int bk, int wm, int wk)
{
	BITBOARD bm0_mask;
	BITBOARD bmmask, bkmask, wmmask, wkmask;
	int bm0;
	uint32 bmindex, bkindex, wmindex, wkindex, bm0index;
	uint32 bmrange, wmrange, bm0range, bkrange, wkrange;
	int64 index64;
	int64 checker_index_base, checker_index;

	bmmask = p->black & ~p->king;
	bm0_mask = bmmask & ROW0;
	bm0 = bitcount64(bm0_mask);

	/* Indices in this subdb are assigned with index 0 having the highest
	 * number of black men on rank0, ...
	 */
	checker_index_base = man_index_base[bm][wm][bm0];

	/* Set the index for the black men that are not on rank0. */
	bmindex = index_pieces_1_type(bmmask ^ bm0_mask, 5);

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

	bmrange = bicoef[40][bm - bm0];
	bm0range = bicoef[5][bm0];
	wmrange = bicoef[45 - bm][wm];
	bkrange = bicoef[MAXSQUARE - bm - wm][bk];
	wkrange = bicoef[MAXSQUARE - bm - wm - bk][wk];

	/* Calculate the checker (man) index. */
	checker_index = bm0index + checker_index_base +
					bmindex * bm0range +
					(int64)wmindex * (int64)bm0range * (int64)bmrange;

	index64 = (int64)wkindex + 
				(int64)bkindex * (int64)wkrange +
				(int64)(checker_index) * (int64)bkrange * (int64)wkrange;
	return(index64);
}


void indextoposition_slice(__int64 index, EGDB_POSITION *p, int bm, int bk, int wm, int wk)
{
	int bm0;
	BITBOARD bmmask, bkmask, wmmask, wkmask;
	uint32 bmindex, bkindex, wmindex, wkindex, bm0index;
	uint32 bmrange, wmrange, bm0range, bkrange, wkrange;
	int64 multiplier, checker_index;

	/* Initialize the board. */
	bmmask = 0;
	bkmask = 0;
	wmmask = 0;
	wkmask = 0;

	bkrange = bicoef[MAXSQUARE - bm - wm][bk];
	wkrange = bicoef[MAXSQUARE - bm - wm - bk][wk];

	/* Indices in this subdb are assigned with index 0 having the highest
	* number of black men on rank0, ...  Find the number of black men on rank0.
	*/
	multiplier = (int64)bkrange * (int64)wkrange;
	checker_index = (index / multiplier);
	index -= checker_index * multiplier;

	/* Find bm0. */
	for (bm0 = min(bm, 5); bm0 > 0; --bm0)
		if (man_index_base[bm][wm][bm0 - 1] > checker_index)
			break;

	checker_index -= man_index_base[bm][wm][bm0];

	/* Compute the range of indices for each piece type. */
	bmrange = bicoef[40][bm - bm0];
	wmrange = bicoef[45 - bm][wm];
	bm0range = bicoef[5][bm0];

	/* Extract the wmindex, bmindex, and bm0index. */
	multiplier = bmrange * bm0range;
	wmindex = (uint32)(checker_index / multiplier);
	checker_index -= wmindex * multiplier;

	multiplier = bm0range;
	bmindex = (uint32)(checker_index / multiplier);
	checker_index -= bmindex * multiplier;
	bm0index = (uint32)checker_index;

	/* Get bkindex and wkindex. */
	bkindex = (uint32)(index / wkrange);
	index -= (int64)bkindex * (int64)wkrange;

	wkindex = (uint32)(index);

	/* Place black men, except those on rank0. */
	bmmask = place_pieces_fwd_no_interferences(bmindex, 40, 5, bm - bm0);

	/* Place black men on rank0. */
	bmmask |= place_pieces_fwd_no_interferences(bm0index, 5, 0, bm0);

	/* Place white men. */
	wmmask = index2bitboard_rev(wmindex, 45, wm, bmmask);

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
	int64 base, partial;

	for (bm = 0; bm <= MAXPIECE; ++bm) {
		for (wm = 0; wm <= MAXPIECE; ++wm) {
			base = 0;
			for (bm0 = min(5, bm); bm0 >= 0; --bm0) {
				man_index_base[bm][wm][bm0] = base;

				/* First the number of men configurations excluding black's backrank. */
				partial = (int64)bicoef[40][bm - bm0] * (int64)bicoef[45 - bm + bm0][wm];

				/* Add the contribution from black's kingrow. */
				partial *= bicoef[5][bm0];
				base += partial;
			}
		}
	}
}


/* return the number of database indices for this subdb.
* needs binomial coefficients in the array bicoef[][] = choose from n, k
*/
int64 getdatabasesize_slice(int bm, int bk, int wm, int wk)
{
	int64 size;
	int64 partial;
	int black_men_backrank;

	/* Compute the number of men configurations first.  Place the black men, summing separately
	* the contributions of 0, 1, 2, 3, or 4 black men on the backrank.
	*/
	size = 0;
	for (black_men_backrank = 0; black_men_backrank <= min(bm, 5); ++black_men_backrank) {

		/* First the number of men configurations excluding black's backrank. */
		partial = (int64)bicoef[40][bm - black_men_backrank] * (int64)bicoef[45 - bm + black_men_backrank][wm];

		/* Add the contribution from black's kingrow. */
		if (black_men_backrank)
			partial *= bicoef[5][black_men_backrank];

		size += partial;
	}

	/* Add the black kings. */
	size *= bicoef[50 - bm - wm][bk];

	/* Add the white kings. */
	size *= bicoef[50 - bm - wm - bk][wk];

	return(size);
}


