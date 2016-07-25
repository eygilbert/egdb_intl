#pragma once

#include "project.h"

#define BLACK 0
#define WHITE 1
#define OTHER_COLOR(color) ((color) ^ 1)

#define MAXMOVES 128
#define MAXPIECES_JUMPED 20
#define NUMSQUARES 50
#define NUM_BITBOARD_BITS 54
#define ROWSIZE 5

#define S1 (BITBOARD)1
#define S2 (BITBOARD)2
#define S3 (BITBOARD)4
#define S4 (BITBOARD)8
#define S5 (BITBOARD)0x10
#define S6 (BITBOARD)0x20
#define S7 (BITBOARD)0x40
#define S8 (BITBOARD)0x80
#define S9 (BITBOARD)0x100
#define S10 (BITBOARD)0x200
#define S11 (BITBOARD)0x800
#define S12 (BITBOARD)0x1000
#define S13 (BITBOARD)0x2000
#define S14 (BITBOARD)0x4000
#define S15 (BITBOARD)0x8000
#define S16 (BITBOARD)0x10000
#define S17 (BITBOARD)0x20000
#define S18 (BITBOARD)0x40000
#define S19 (BITBOARD)0x80000
#define S20 (BITBOARD)0x100000
#define S21 (BITBOARD)0x400000
#define S22 (BITBOARD)0x800000
#define S23 (BITBOARD)0x1000000
#define S24 (BITBOARD)0x2000000
#define S25 (BITBOARD)0x4000000
#define S26 (BITBOARD)0x8000000
#define S27 (BITBOARD)0x10000000
#define S28 (BITBOARD)0x20000000
#define S29 (BITBOARD)0x40000000
#define S30 (BITBOARD)0x80000000
#define S31 (BITBOARD)0x200000000
#define S32 (BITBOARD)0x400000000
#define S33 (BITBOARD)0x800000000
#define S34 (BITBOARD)0x1000000000
#define S35 (BITBOARD)0x2000000000
#define S36 (BITBOARD)0x4000000000
#define S37 (BITBOARD)0x8000000000
#define S38 (BITBOARD)0x10000000000
#define S39 (BITBOARD)0x20000000000
#define S40 (BITBOARD)0x40000000000
#define S41 (BITBOARD)0x100000000000
#define S42 (BITBOARD)0x200000000000
#define S43 (BITBOARD)0x400000000000
#define S44 (BITBOARD)0x800000000000
#define S45 (BITBOARD)0x1000000000000
#define S46 (BITBOARD)0x2000000000000
#define S47 (BITBOARD)0x4000000000000
#define S48 (BITBOARD)0x8000000000000
#define S49 (BITBOARD)0x10000000000000
#define S50 (BITBOARD)0x20000000000000

#define S1_bit 0
#define S2_bit 1
#define S3_bit 2
#define S4_bit 3
#define S5_bit 4
#define S6_bit 5
#define S7_bit 6
#define S8_bit 7
#define S9_bit 8
#define S10_bit 9
#define S11_bit 11
#define S12_bit 12
#define S13_bit 13
#define S14_bit 14
#define S15_bit 15
#define S16_bit 16
#define S17_bit 17
#define S18_bit 18
#define S19_bit 19
#define S20_bit 20
#define S21_bit 22
#define S22_bit 23
#define S23_bit 24
#define S24_bit 25
#define S25_bit 26
#define S26_bit 27
#define S27_bit 28
#define S28_bit 29
#define S29_bit 30
#define S30_bit 31
#define S31_bit 33
#define S32_bit 34
#define S33_bit 35
#define S34_bit 36
#define S35_bit 37
#define S36_bit 38
#define S37_bit 39
#define S38_bit 40
#define S39_bit 41
#define S40_bit 42
#define S41_bit 44
#define S42_bit 45
#define S43_bit 46
#define S44_bit 47
#define S45_bit 48
#define S46_bit 49
#define S47_bit 50
#define S48_bit 51
#define S49_bit 52
#define S50_bit 53


#define COLUMN0 (S5 | S15 | S25 | S35 | S45)
#define COLUMN1 (S10 | S20 | S30 | S40 | S50)
#define COLUMN2 (S4 | S14 | S24 | S34 | S44)
#define COLUMN3 (S9 | S19 | S29 | S39 | S49)
#define COLUMN4 (S3 | S13 | S23 | S33 | S43)
#define COLUMN5 (S8 | S18 | S28 | S38 | S48)
#define COLUMN6 (S2 | S12 | S22 | S32 | S42)
#define COLUMN7 (S7 | S17 | S27 | S37 | S47)
#define COLUMN8 (S1 | S11 | S21 | S31 | S41)
#define COLUMN9 (S6 | S16 | S26 | S36 | S46)

#define ROW0 (S1 | S2 | S3 | S4 | S5)
#define ROW1 (S6 | S7 | S8 | S9 | S10)
#define ROW2 (S11 | S12 | S13 | S14 | S15)
#define ROW3 (S16 | S17 | S18 | S19 | S20)
#define ROW4 (S21 | S22 | S23 | S24 | S25)
#define ROW5 (S26 | S27 | S28 | S29 | S30)
#define ROW6 (S31 | S32 | S33 | S34 | S35)
#define ROW7 (S36 | S37 | S38 | S39 | S40)
#define ROW8 (S41 | S42 | S43 | S44 | S45)
#define ROW9 (S46 | S47 | S48 | S49 | S50)

#define ALL_ROWS (ROW0 | ROW1 | ROW2 | ROW3 | ROW4 | ROW5 | ROW6 | ROW7 | ROW8 | ROW9)
#define ALL_SQUARES ALL_ROWS

#define BLACK_KING_RANK_MASK ROW9
#define WHITE_KING_RANK_MASK ROW0


typedef uint64 BITBOARD;
typedef int64 SIGNED_BITBOARD;


/* Definition of a board.
 * bit 0 means square 1, bit 1 is square 2, ...
 * There are gaps at bits 10, 21, 32, and 43.
 */
typedef struct {
	union {
		struct {
			BITBOARD pieces[2];		/* Indexed by BLACK or WHITE. */
			BITBOARD king;
		};
		struct {
			BITBOARD black;
			BITBOARD white;
			BITBOARD king;
		};
	};
} BOARD;


extern char square0_to_bitnum_table[NUMSQUARES];
extern BITBOARD square0_to_bitboard_table[NUMSQUARES];
extern char bitnum_to_square0_table[54];
extern int rank_to_rank0_shift_table[10];
extern int bitboard_to_square0(BITBOARD bb);
extern int bitboard_to_square(BITBOARD bb);


inline int mirrored(int sq)
{
	return(S50_bit - sq);
}


inline int square0_to_bitnum(int square0)
{
	return(square0_to_bitnum_table[square0]);
}


inline BITBOARD square0_to_bitboard(int square0)
{
	return(square0_to_bitboard_table[square0]);
}


inline int bitnum_to_square0(int bitnum)
{
	return(bitnum_to_square0_table[bitnum]);
}


inline int rank_to_rank0_shift(int rank)
{
	return(rank_to_rank0_shift_table[rank]);
}


