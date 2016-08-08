#include "builddb/compression_tables.h"
#include "egdb/egdb_intl.h"

namespace egdb_interface {

/*
 * skip numbers range from 5...MAXSKIP defined above, there are SKIPS of them.
 */
int skip[SKIPS] = {
	5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
	31,32,36,40,44,48,52,56,60,70,80,90,100,150,200,250,300,400,500,650,800,
	1000,1200,1400,1600,2000,2400,3200,4000,5000,7500,MAXSKIP
};

/* skip table for incomplete databases. */
int skip_inc[SKIPS_INC] = {
	3,4,5,6,7,8,9,10,11,12,
	13,14,15,16,17,18,19,20,21,22,
	23,24,25,26,27,28,29,30,31,32,
	33,34,35,70,106,MAXSKIP_INC
};

int mtc_skip[MTC_SKIPS] = {
/* 0 */		1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
/* 1 */		11, 12, 13, 14, 15, 16, 17, 18, 19, 20,	
/* 2 */		21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
/* 3 */		31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
/* 4 */		41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
/* 5 */		100, 150, 200, 250, 300, 350, 400, 450, 500, 550,
/* 6 */		600, 650, 700, 750, 800, 850, 900, 950, 1000, 2000,	
/* 7 */		3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000, 30000,
/* 8 */		40000, 50000, 60000, 70000, 800000, 90000, 100000, 300000, 1000000, 3000000, 
/* 9 */		10000000, 30000000, 100000000, MAX_MTCSKIP
};

int skipindex[MAXSKIP + 1]; // holds the index of the next smaller or equal skip.

int runlength[256];					/* holds the run length for every byte. */
int runlength_inc[256];				/* holds the run length for every byte. */
int runlength_4val[256];
int runlength_mtc[256];
unsigned short runlength16[65536];	/* the run length of each 2 bytes. */
int compressed_value[256];			/* holds the value for every byte. */
int compressed_value_inc[256];		/* holds the value for every byte. */
int compressed_value_4val[256];


/*
 * Initialize skipindex.
 */
void init_skips()
{
	int i, j;

	for (i = 0; i <= MAXSKIP; i++) {
		for (j = SKIPS - 1; j >= 0; j--) {
			if (skip[j] <= i) {
				skipindex[i] = j;
				break;
			}
		}
	}
}


/*
 * Initialize tables of runlengths and compressed values.
 */
void init_runlengths()
{
	int i;

	/* Init WLD runlengths. */
	for (i = 0; i < 81; i++)
		runlength[i] = 4;
	for (i = 81; i < 256; i++) {
		runlength[i] = skip[(i - 81) % SKIPS];
		compressed_value[i] = ((i - 81) / SKIPS);
	}
	for (i = 0; i < 65536; ++i)
		runlength16[i] = runlength[i & 0xff] + runlength[i >> 8];

	/* Init MTC runlengths. */
	for (i = 0; i < 256; ++i)
		if (i < MTC_SKIPS)
			runlength_mtc[i] = mtc_skip[i];
		else
			runlength_mtc[i] = 1;
}


void init_runlengths_inc()
{
	int i;

	/* Init WLD runlengths. */
	for (i = 0; i < 36; i++)
		runlength_inc[i] = 2;
	for (i = 36; i < 256; i++) {
		runlength_inc[i] = skip_inc[(i - 36) % SKIPS_INC];
		compressed_value_inc[i] = ((i - 36) / SKIPS_INC);
	}
}


void init_compression_tables()
{
	init_skips();
	init_runlengths();
	init_runlengths_inc();
}

}	// namespace egdb_interface

