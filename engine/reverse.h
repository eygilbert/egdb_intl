#pragma once
#include "engine/board.h"
#include <cstdint>

namespace egdb_interface {

void init_reverse();
unsigned int reverse_image(unsigned int image);
uint64_t reverse_board50(uint64_t image);


inline void reverse(BOARD *dest, BOARD *src)
{
	dest->black = reverse_board50(src->white);
	dest->white = reverse_board50(src->black);
	dest->king = reverse_board50(src->king);
}

}	// namespace egdb_interface
