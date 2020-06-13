#include "engine/reverse.h"
#include "engine/bicoef.h"
#include "engine/bitcount.h"
#include "engine/board.h"
#include "engine/bool.h"
#include <stdint.h>

namespace egdb_interface {

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


BITBOARD reverse_bitboard(BITBOARD bb)
{
	int bit;
	uint64_t rev;

	rev = 0;
	while (bb) {
		bit = LSB64(bb);
		bb = clear_lsb(bb);
		rev |= ((uint64_t)1 << mirrored(bit));
	}
	return(rev);
}

}	// namespace egdb_interface
