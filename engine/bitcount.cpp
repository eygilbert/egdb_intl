#include "egdb/platform.h"
#include "engine/bitcount.h"
#include "engine/bool.h"
#include "engine/project.h"	// ARRAY_SIZE

namespace egdb_interface {

bool did_init_bitcount;
bool cpu_has_popcount; 
char bitcount_table[0x10000];


/*
 * Return the number of bits which are set in a 32-bit integer.
 * Slower than a table-based bitcount if many bits are set.
 * Only used to make the table for the table-based bitcount
 * on initialization.
 */
int iterativebitcount(uint32_t n)
{
	int r;

	r = 0;
	while (n) {
		n = clear_lsb(n);
		++r;
	}
	return r;
}

void init_bitcount()
{
	uint32_t i;

#ifdef ENVIRONMENT64
	cpu_has_popcount = check_cpu_has_popcount();
#endif
	for (i = 0; i < ARRAY_SIZE(bitcount_table); ++i)
		bitcount_table[i] = iterativebitcount(i);

	did_init_bitcount = true;
}

}	// namespace egdb_interface
