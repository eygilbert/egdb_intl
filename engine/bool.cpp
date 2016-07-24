// 
// bool.c - performs some boolean operations
//
// must be initialized with a call to initbool()
#include "project.h"
#include "board.h"
#include "bool.h"
#include "bicoef.h"
#include "bitcount.h"

#if !_WIN64
#define USE_ASM // use assembler routines in LSB, MSB
#endif			// only works with MS visual C



#ifndef USE_ASM
static char LSBarray[256];
static char MSBarray[256];
#endif


#if !INLINE_LSB
/*
 * Return the bit number of the smallest bit set in x.
 * ie. if bit 0 is set, return 0, if bit 1 is set, return 1, ...
 * If x is 0 then return 0.
 * Note: this will not work on anything less than an 80386.
 */
int LSB(unsigned int x)
{
#ifdef USE_ASM

	__asm {
		mov	eax, 0
		bsf	eax, dword ptr x 
	}

#else

	if (x & 0x000000FF)
		return(LSBarray[x & 0x000000FF]);
	if (x & 0x0000FF00)
		return(LSBarray[(x >> 8) & 0x000000FF] + 8);
	if (x & 0x00FF0000)
		return(LSBarray[(x >> 16) & 0x000000FF] + 16);
	if (x & 0xFF000000)
		return(LSBarray[(x >> 24) & 0x000000FF] + 24);
	return -1;
#endif

}
#endif


#if !INLINE_MSB
/*
 * Return the position of the most significant bit set in 
 * a 32-bit word x, or 0 if x is 0.
 * Note: this will not work on anything less than an 80386.
 */
int MSB(unsigned int x)
{
#ifdef USE_ASM
	__asm {
		mov	eax, 0
		bsr	eax, dword ptr x 
	}
	
#else

	if (x & 0xFF000000)
		return(MSBarray[(x >> 24) & 0xFF] + 24);
	if (x & 0x00FF0000)
		return(MSBarray[(x >> 16) & 0xFF] + 16);
	if (x & 0x0000FF00)
		return(MSBarray[(x >> 8) & 0xFF] + 8);
	return(MSBarray[x & 0xFF]);
	//if x==0 return MSBarray[0], that's ok, because it is set to -1 

#endif
}
#endif


void initbool(void)
	{
	//-----------------------------------------------------------------------------------------------------
	// initialize the "bool.c" module
	// i.e. initialize lookup tables
	//-----------------------------------------------------------------------------------------------------


#ifndef USE_ASM
	int i;

	// initialize array for "LSB" 
	for(i=0;i<256;i++)
		{
		if(i&1) {LSBarray[i]=0;continue;}
		if(i&2) {LSBarray[i]=1;continue;}
		if(i&4) {LSBarray[i]=2;continue;}
		if(i&8) {LSBarray[i]=3;continue;}
		if(i&16) {LSBarray[i]=4;continue;}
		if(i&32) {LSBarray[i]=5;continue;}
		if(i&64) {LSBarray[i]=6;continue;}
		if(i&128) {LSBarray[i]=7;continue;}
		LSBarray[i]=-1;
		}

	// initialize array for "MSB" 
	for(i=0;i<256;i++)
		{
		if(i&128) {MSBarray[i]=7;continue;}
		if(i&64) {MSBarray[i]=6;continue;}
		if(i&32) {MSBarray[i]=5;continue;}
		if(i&16) {MSBarray[i]=4;continue;}
		if(i&8) {MSBarray[i]=3;continue;}
		if(i&4) {MSBarray[i]=2;continue;}
		if(i&2) {MSBarray[i]=1;continue;}
		if(i&1) {MSBarray[i]=0;continue;}
		MSBarray[i]=-1;
		}
#endif
}

