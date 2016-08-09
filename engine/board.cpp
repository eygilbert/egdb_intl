#include "engine/board.h"
#include "engine/bool.h"

namespace egdb_interface {

char square0_to_bitnum_table[NUMSQUARES] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
	44, 45, 46, 47, 48, 49, 50, 51, 52, 53
};

BITBOARD square0_to_bitboard_table[NUMSQUARES] = {
	S1,   S2,  S3,  S4,  S5,  S6,  S7,  S8,  S9, S10,
	S11, S12, S13, S14, S15, S16, S17, S18, S19, S20,
	S21, S22, S23, S24, S25, S26, S27, S28, S29, S30,
	S31, S32, S33, S34, S35, S36, S37, S38, S39, S40,
	S41, S42, S43, S44, S45, S46, S47, S48, S49, S50
};

char bitnum_to_square0_table[NUM_BITBOARD_BITS] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
	 0, 10, 11, 12, 13, 14, 15, 16, 17, 18,
	19,  0, 20, 21, 22, 23, 24, 25, 26, 27,
	28, 29,  0, 30, 31, 32, 33, 34, 35, 36,
	37, 38, 39,  0, 40, 41, 42, 43, 44, 45,
	46, 47, 48, 49
};

int rank_to_rank0_shift_table[] = {
	0, 5, 11, 16, 22, 27, 33, 38, 44, 49,
};


int bitboard_to_square0(BITBOARD bb)
{
	return(bitnum_to_square0(LSB64(bb)));
}


int bitboard_to_square(BITBOARD bb)
{
	return(1 + bitnum_to_square0(LSB64(bb)));
}

}	// namespace egdb_interface
