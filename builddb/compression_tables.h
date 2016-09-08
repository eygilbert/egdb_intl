#pragma once

namespace egdb_interface {

#define SKIPS 58        /* the number of skips, for fully resolved dbs. */
#define MAXSKIP 10000   /* the largest skip value, for fully resolved dbs. */

#define SKIPS_INC 36	/* for incomplete dbs with unresolved positions. */
#define MAXSKIP_INC 500

#define MAX_MTCSKIP 1000000000

/* Encode a moves-to-conv number that is >= the threshold for saving. */
#define MTC_ENCODE(mtc) (MTC_SKIPS + (mtc) / 2)

/* Decode a moves-to-conv number that is >= the threshold for saving. */
#define MTC_DECODE(val) (2 * ((val) - MTC_SKIPS))
#define NOT_SINGLEVALUE 127
#define MTC_SKIPS 94

#define SINGLEVALUE_CODES ".+-=?!"

void init_compression_tables();
void init_runlength16_mtc();

extern int skip[];
extern int skip_inc[];
extern int mtc_skip[];
extern int skipindex[MAXSKIP + 1];			/* index of the next smaller or equal skip. */
extern int skipindex_inc[MAXSKIP_INC + 1];	/* index of the next smaller or equal skip (for incomplete dbs). */

extern int runlength[256];					/* run length for every byte. */
extern int runlength_inc[256];				/* run length for every byte (for incomplete dbs). */
extern unsigned short runlength16[65536];	/* run length of each 2 bytes. */
extern int compressed_value[256];			/* the db value for every byte. */
extern int compressed_value_inc[256];		/* the db value for every byte (for incomplete dbs). */
extern int runlength_mtc[256];
extern unsigned int *runlength16_mtc;
}	// namespace egdb_interface
