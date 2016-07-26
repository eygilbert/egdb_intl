#pragma once
#include "engine/board.h"

BITBOARD ungapped_to_gapped_bitboard(BITBOARD bb);
BITBOARD gapped_to_ungapped_bitboard(BITBOARD bb);

void gapped_to_ungapped_board(BOARD *gboard, BOARD *ugboard);
void ungapped_to_gapped_board(BOARD *ugboard, BOARD *gboard);

/* Conversions in place. */
void gapped_to_ungapped_board(BOARD *board);
void ungapped_to_gapped_board(BOARD *board);
