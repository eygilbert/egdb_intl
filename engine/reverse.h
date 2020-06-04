#pragma once
#include "engine/board.h"
#include <stdint.h>

namespace egdb_interface {

void init_reverse();
BITBOARD reverse_bitboard(BITBOARD bb);


inline BOARD reverse(const BOARD &board)
{
	BOARD revboard;

	revboard.black = reverse_bitboard(board.white);
	revboard.white = reverse_bitboard(board.black);
	revboard.king = reverse_bitboard(board.king);
	return(revboard);
}


inline void reverse(BOARD *dest, BOARD *src)
{
	dest->black = reverse_bitboard(src->white);
	dest->white = reverse_bitboard(src->black);
	dest->king = reverse_bitboard(src->king);
}

}	// namespace egdb_interface
