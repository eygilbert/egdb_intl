#pragma once

void init_reverse();
unsigned int reverse_image(unsigned int image);
uint64 reverse_board50(uint64 image);


inline void reverse(BOARD *dest, BOARD *src)
{
	dest->black = reverse_board50(src->white);
	dest->white = reverse_board50(src->black);
	dest->king = reverse_board50(src->king);
}

