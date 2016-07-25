#pragma once

#include <intrin.h>


#define hiword(x) (((x) & 0xFFFF0000) >> 16)
#define loword(x) ((x) & 0xFFFF)


inline uint32 clear_lsb(uint32 x)
{
	return(x & (x - 1));
}


inline uint64 clear_lsb(uint64 x)
{
	return(x & (x - 1));
}


inline int LSB(uint32 x)
{
	unsigned long bitpos;

	if (_BitScanForward(&bitpos, x))
		return(bitpos);
	else
		return(0);
}


inline int MSB(uint32 x)
{
	unsigned long bitpos;

	if (_BitScanReverse(&bitpos, x))
		return(bitpos);
	else
		return(0);
}


#ifdef _WIN64
inline int LSB64(uint64 x)
{
	unsigned long bitpos;

	if (_BitScanForward64(&bitpos, x))
		return(bitpos);
	else
		return(0);
}

inline int MSB64(uint64 x)
{
	unsigned long bitpos;

	if (_BitScanReverse64(&bitpos, x))
		return(bitpos);
	else
		return(0);
}

#else

inline int LSB64(uint64 x)
{
	if (x & 0xffffffff)
		return(LSB((uint32)(x & 0xffffffff)));
	else
		return(32 + LSB((uint32)(x >> 32)));
}

inline int MSB64(uint64 x)
{
	if (x & 0xffffffff00000000)
		return(32 + MSB((uint32)(x >> 32)));
	else
		return(MSB((uint32)(x & 0xffffffff)));
}
#endif


typedef union {
	uint64 all;
	struct {
		unsigned int low32;
		unsigned int high32;
	};
} BITBOARD_SPLIT_VIEW;


inline unsigned int rows01(uint64 *bb)
{
	return(((BITBOARD_SPLIT_VIEW *)bb)->low32 & (0x3ff));
}


inline unsigned int rows23(uint64 *bb)
{
	return((((BITBOARD_SPLIT_VIEW *)bb)->low32 >> 11) & (0x3ff));
}


inline unsigned int rows45(uint64 *bb)
{
	return((((BITBOARD_SPLIT_VIEW *)bb)->low32 >> 22) & (0x3ff));
}


inline unsigned int rows67(uint64 *bb)
{
	return((((BITBOARD_SPLIT_VIEW *)bb)->high32 >> 1) & (0x3ff));
}


inline unsigned int rows89(uint64 *bb)
{
	return((((BITBOARD_SPLIT_VIEW *)bb)->high32 >> 12) & (0x3ff));
}


