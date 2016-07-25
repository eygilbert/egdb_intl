#include "reverse.h"

// engine
#include "bicoef.h"
#include "bitcount.h"
#include "board.h"
#include "bool.h"
#include "project.h"

#include <Windows.h>

unsigned char ReverseByte[256];


void init_reverse()
{
	int i, j;

	/* ReverseByte contains the "reverse" of each byte 0..255. For
	 * example, entry 17 (binary 00010001) is a (10001000) - reverse
	 * the bits.
	 */
	for (i = 0; i < 256; i++) {
		ReverseByte[i] = 0;
		for (j = 0; j < 8; j++)
			if (i & (1L << j))
				ReverseByte[i] |= (1L << (7 - j));
	}
}


/*
 * Reverse a board mask.
 */
unsigned int reverse_image(unsigned int image)
{
	union {
		unsigned char bytes[4];
		unsigned int word;
	} conv_out;

	conv_out.bytes[0] = (unsigned char)ReverseByte[((unsigned char *)(&image))[3]];
	conv_out.bytes[1] = (unsigned char)ReverseByte[((unsigned char *)(&image))[2]];
	conv_out.bytes[2] = (unsigned char)ReverseByte[((unsigned char *)(&image))[1]];
	conv_out.bytes[3] = (unsigned char)ReverseByte[((unsigned char *)(&image))[0]];
	return(conv_out.word);
}


uint64 reverse_board50(uint64 image)
{
	int bit;
	uint64 rev;

	rev = 0;
	while (image) {
		bit = LSB64(image);
		image = image & (image - 1);
		rev |= ((uint64)1 << (NUM_BITBOARD_BITS - 1 - bit));
	}
	return(rev);
}

