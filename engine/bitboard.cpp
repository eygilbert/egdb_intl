#include "windows.h"
#include "project.h"
#include "board.h"
#include "bool.h"
#include "bicoef.h"
#include "bitcount.h"
#include "move_api.h"


#define S1_UG (uint64)1
#define S2_UG (uint64)2
#define S3_UG (uint64)4
#define S4_UG (uint64)8
#define S5_UG (uint64)0x10
#define S6_UG (uint64)0x20
#define S7_UG (uint64)0x40
#define S8_UG (uint64)0x80
#define S9_UG (uint64)0x100
#define S10_UG (uint64)0x200
#define S11_UG (uint64)0x400
#define S12_UG (uint64)0x800
#define S13_UG (uint64)0x1000
#define S14_UG (uint64)0x2000
#define S15_UG (uint64)0x4000
#define S16_UG (uint64)0x8000
#define S17_UG (uint64)0x10000
#define S18_UG (uint64)0x20000
#define S19_UG (uint64)0x40000
#define S20_UG (uint64)0x80000
#define S21_UG (uint64)0x100000 
#define S22_UG (uint64)0x200000
#define S23_UG (uint64)0x400000
#define S24_UG (uint64)0x800000
#define S25_UG (uint64)0x1000000
#define S26_UG (uint64)0x2000000
#define S27_UG (uint64)0x4000000
#define S28_UG (uint64)0x8000000
#define S29_UG (uint64)0x10000000
#define S30_UG (uint64)0x20000000
#define S31_UG (uint64)0x40000000
#define S32_UG (uint64)0x80000000
#define S33_UG (uint64)0x100000000
#define S34_UG (uint64)0x200000000
#define S35_UG (uint64)0x400000000
#define S36_UG (uint64)0x800000000
#define S37_UG (uint64)0x1000000000
#define S38_UG (uint64)0x2000000000
#define S39_UG (uint64)0x4000000000
#define S40_UG (uint64)0x8000000000
#define S41_UG (uint64)0x10000000000
#define S42_UG (uint64)0x20000000000
#define S43_UG (uint64)0x40000000000
#define S44_UG (uint64)0x80000000000
#define S45_UG (uint64)0x100000000000
#define S46_UG (uint64)0x200000000000
#define S47_UG (uint64)0x400000000000
#define S48_UG (uint64)0x800000000000
#define S49_UG (uint64)0x1000000000000
#define S50_UG (uint64)0x2000000000000

#define ROW0_UG (S1_UG | S2_UG | S3_UG | S4_UG | S5_UG)
#define ROW1_UG (S6_UG | S7_UG | S8_UG | S9_UG | S10_UG)
#define ROW2_UG (S11_UG | S12_UG | S13_UG | S14_UG | S15_UG)
#define ROW3_UG (S16_UG | S17_UG | S18_UG | S19_UG | S20_UG)
#define ROW4_UG (S21_UG | S22_UG | S23_UG | S24_UG | S25_UG)
#define ROW5_UG (S26_UG | S27_UG | S28_UG | S29_UG | S30_UG)
#define ROW6_UG (S31_UG | S32_UG | S33_UG | S34_UG | S35_UG)
#define ROW7_UG (S36_UG | S37_UG | S38_UG | S39_UG | S40_UG)
#define ROW8_UG (S41_UG | S42_UG | S43_UG | S44_UG | S45_UG)
#define ROW9_UG (S46_UG | S47_UG | S48_UG | S49_UG | S50_UG)


BITBOARD ungapped_to_gapped_bitboard(BITBOARD bb)
{
	BITBOARD gapped;

	gapped = bb & (ROW0_UG | ROW1_UG);
	gapped |= (bb & (ROW2_UG | ROW3_UG)) << 1;
	gapped |= (bb & (ROW4_UG | ROW5_UG)) << 2;
	gapped |= (bb & (ROW6_UG | ROW7_UG)) << 3;
	gapped |= (bb & (ROW8_UG | ROW9_UG)) << 4;
	return(gapped);
}


BITBOARD gapped_to_ungapped_bitboard(BITBOARD bb)
{
	BITBOARD ungapped;

	ungapped = bb & (ROW0 | ROW1);
	ungapped |= (bb & (ROW2 | ROW3)) >> 1;
	ungapped |= (bb & (ROW4 | ROW5)) >> 2;
	ungapped |= (bb & (ROW6 | ROW7)) >> 3;
	ungapped |= (bb & (ROW8 | ROW9)) >> 4;
	return(ungapped);
}


void gapped_to_ungapped_board(BOARD *gboard, BOARD *ugboard)
{
	gboard->black = gapped_to_ungapped_bitboard(ugboard->black);
	gboard->white = gapped_to_ungapped_bitboard(ugboard->white);
	gboard->king = gapped_to_ungapped_bitboard(ugboard->king);
}


void ungapped_to_gapped_board(BOARD *ugboard, BOARD *gboard)
{
	ugboard->black = ungapped_to_gapped_bitboard(gboard->black);
	ugboard->white = ungapped_to_gapped_bitboard(gboard->white);
	ugboard->king = ungapped_to_gapped_bitboard(gboard->king);
}


void gapped_to_ungapped_board(BOARD *board)
{
	board->black = gapped_to_ungapped_bitboard(board->black);
	board->white = gapped_to_ungapped_bitboard(board->white);
	board->king = gapped_to_ungapped_bitboard(board->king);
}


void ungapped_to_gapped_board(BOARD *board)
{
	board->black = ungapped_to_gapped_bitboard(board->black);
	board->white = ungapped_to_gapped_bitboard(board->white);
	board->king = ungapped_to_gapped_bitboard(board->king);
}

