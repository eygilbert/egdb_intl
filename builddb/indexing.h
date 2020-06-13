#pragma once
#include "egdb/egdb_intl.h"
#include "engine/bicoef.h"
#include "engine/bitcount.h"
#include "engine/board.h"
#include "engine/bool.h"
#include <stdint.h>

namespace egdb_interface {

#define MAXSQUARE NUMSQUARES
#define MAXPIECE 5
#define RANK0MAX ROWSIZE

#define MAX_SUBSLICE_INDICES (int64_t)(0x80000000)
#define DTW_SUBSLICE_INDICES (int64_t)(0x40000000)
#define GHOSTS (((BITBOARD)1 << 10) | ((BITBOARD)1 << 21) | ((BITBOARD)1 << 32) | ((BITBOARD)1 << 43))

BITBOARD free_square_bitboard_fwd(int logical_square, BITBOARD occupied);
BITBOARD free_square_bitboard_rev(int logical_square, BITBOARD occupied);
BITBOARD index2bitboard_fwd(unsigned int index, int num_squares, int first_square, int num_pieces);
BITBOARD index2bitboard_fwd(unsigned int index, int num_squares, int num_pieces, BITBOARD occupied);
BITBOARD index2bitboard_rev(unsigned int index, int num_squares, int num_pieces, BITBOARD occupied);

int64_t position_to_index_slice(EGDB_POSITION const *p, int bm, int bk, int wm, int wk);
void indextoposition_slice(int64_t index, EGDB_POSITION *p, int bm, int bk, int wm, int wk);
void build_man_index_base();
int64_t getdatabasesize_slice(int bm, int bk, int wm, int wk);
int64_t getslicesize_gaps(int bm, int bk, int wm, int wk);

inline int64_t position_to_index_slice(EGDB_POSITION *p)
{
	int nbm, nbk, nwm, nwk;

	nbm = bitcount64(p->black & ~p->king);
	nbk = bitcount64(p->black & p->king);
	nwm = bitcount64(p->white & ~p->king);
	nwk = bitcount64(p->white & p->king);
	return(position_to_index_slice((EGDB_POSITION *)p, nbm, nbk, nwm, nwk));
}


inline uint32_t index_pieces_1_type(BITBOARD bb, int offset)
{
	int piece, bitnum;
	uint32_t index;

	for (index = 0, piece = 1; bb; ++piece) {

		/* Get square of next piece. */
		bitnum = LSB64(bb);

		/* Remove him from the bitboard. */
		bb = clear_lsb(bb);

		/* Add to his index. */
		index += choose(bitnum_to_square0(bitnum) - offset, piece);
	}
	return(index);
}


inline uint32_t index_pieces_1_type(BITBOARD bb, BITBOARD interfering)
{
	int piece, bitnum, square0;
	uint32_t index;

	for (piece = 1, index = 0; bb; ++piece) {

		/* Get square of the next piece. */
		bitnum = LSB64(bb);
		bb = clear_lsb(bb);

		/* Subtract from his square the count of pieces on 
		 * squares 0....square-1.
		 */
		square0 = bitnum_to_square0(bitnum);
		square0 -= bitcount64((interfering) & (((BITBOARD)1 << bitnum) - 1)); 

		/* Add to his index. */
		index += choose(square0, piece);
	}
	return(index);
}


inline uint32_t index_pieces_1_type_reverse(BITBOARD bb, BITBOARD interfering)
{
	int piece, bitnum, square0;
	uint32_t index;

	for (index = 0, piece = 1; bb; ++piece) {
		
		/* Get square of most advanced piece. */
		bitnum = MSB64(bb);

		/* Remove him from the bitboard. */
		bb = bb ^ ((BITBOARD)1 << bitnum);

		/* Get his 'logical' square number, or how far advance he is,
		 * accounting for interfering pieces.
		 */
		square0 = MAXSQUARE - 1 - bitnum_to_square0(bitnum);
		square0 -= bitcount64(interfering & ~(((BITBOARD)1 << bitnum) - 1));

		/* Add to his index. */
		index += choose(square0, piece);
	}
	return(index);
}

}	// namespace

