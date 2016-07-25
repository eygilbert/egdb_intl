#pragma once

#include "project.h"

#include <intrin.h>

extern bool cpu_has_popcount;
extern char bitcount_table[0x10000];

void init_bitcount();


inline int bitcount16_emul(uint16 n)
{
	return(bitcount_table[n]);
}


inline int bitcount32_emul(uint32 n)
{
	return(bitcount_table[n & 0x0000FFFF] + bitcount_table[n >> 16]);
}


inline int bitcount16(uint16 n)
{
#ifdef _WIN64
	if (cpu_has_popcount)
		return(__popcnt(n));
	else
#endif
		return(bitcount16_emul(n));
}


inline int bitcount(uint32 n)
{
#ifdef _WIN64
	if (cpu_has_popcount)
		return(__popcnt(n));
	else
#endif
		return(bitcount32_emul(n));
}


#ifdef _WIN64

inline int bitcount64(uint64 n)
{
	if (cpu_has_popcount)
		return((int)__popcnt64(n));
	else
		return(bitcount32_emul((uint32)(n & 0xffffffff)) + bitcount32_emul((uint32)(n >> 32) & 0xffffffff));
}

#else

inline int bitcount64(uint64 n)
{
	return(bitcount32_emul((uint32)(n & 0xffffffff)) + bitcount32_emul((uint32)(n >> 32) & 0xffffffff));
}

#endif

