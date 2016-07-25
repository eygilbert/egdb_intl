#include "bitcount.h"

// engine
#include "bool.h"
#include "project.h"

bool cpu_has_popcount; 
char bitcount_table[0x10000];


/*
 * Return the number of bits which are set in a 32-bit integer.
 * Slower than a table-based bitcount if many bits are set.
 * Only used to make the table for the table-based bitcount
 * on initialization.
 */
int iterativebitcount(uint32 n)
{
	int r;

	r = 0;
	while (n) {
		n = clear_lsb(n);
		++r;
	}
	return r;
}


bool check_cpu_has_popcount()
{
	int cpuinfo[4] = {-1};

	__cpuid(cpuinfo, 1);
	return((cpuinfo[2] >> 23) & 1);
}


void init_bitcount()
{
	uint32 i;

#ifdef _WIN64
	cpu_has_popcount = check_cpu_has_popcount();
#endif
	for (i = 0; i < ARRAY_SIZE(bitcount_table); ++i)
		bitcount_table[i] = iterativebitcount(i);
}


