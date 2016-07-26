#include "egdb/crc.h"
#include "egdb/egdb_intl.h"
#include "engine/project.h"	// ARRAY_SIZE
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define MAXFILENAME MAX_PATH


typedef struct {
	EGDB_TYPE egdb_type;
	char *name;
	int pieces;
	unsigned int crc;
} EGDB_FIND_INFO;

EGDB_FIND_INFO egdb_find_table[] = {
	{EGDB_WLD_TUN_V2, "db9-5040.idx1", 9, 0xef847fc5},
	{EGDB_WLD_TUN_V1, "db9-5040.idx", 9, 0x1f866ec6},
	{EGDB_WLD_RUNLEN, "db9-5040.idx", 9, 0xa6c3208e},	/* Version 2, with more resolved. */
	{EGDB_WLD_RUNLEN, "db9-5040.idx", 9, 0xa473f0eb},	/* first version. */
	{EGDB_WLD_TUN_V2, "db8-4040.idx1", 8, 0x40993827},
	{EGDB_WLD_TUN_V1, "db8-4040.idx", 8, 0x97ed951e},	/* complete. */
	{EGDB_WLD_TUN_V1, "db8-4040.idx", 8, 0xad6ccff2},	/* incomplete. */
	{EGDB_WLD_RUNLEN, "db8-4040.idx", 8, 0x9a9df5bc},		/* incomplete. */
	{EGDB_MTC_RUNLEN, "db8-0503.idx_mtc", 8, 0x7493956f},
	{EGDB_WLD_TUN_V2, "db7-4030.idx1", 7, 0x713fa989},
	{EGDB_WLD_TUN_V1, "db7-4030.idx", 7, 0xa1067e2b},
	{EGDB_WLD_RUNLEN, "db7-4030.idx", 7, 0x68913d08},
	{EGDB_MTC_RUNLEN, "db7-0412.idx_mtc", 7, 0xb4c92c3e},
	{EGDB_WLD_TUN_V2, "db6-3030.idx1", 6, 0xc07467f2},
	{EGDB_WLD_TUN_V1, "db6-3030.idx", 6, 0xf3693c6c},
	{EGDB_WLD_RUNLEN, "db6-3030.idx", 6, 0xd661d188},
	{EGDB_MTC_RUNLEN, "db6-0312.idx_mtc", 6, 0xd764c8ec},
	{EGDB_WLD_RUNLEN, "db6-3030.idx", 6, 0x947dff31},		/* Re-generated. */
	{EGDB_WLD_TUN_V2, "db5.idx1", 5, 0xc5912d8f},
	{EGDB_WLD_TUN_V1, "db5-3020.idx", 5, 0xc008c727},
	{EGDB_WLD_RUNLEN, "db5-3020.idx", 5, 0xeee459ed},
	{EGDB_MTC_RUNLEN, "db5-0311.idx_mtc", 5, 0x582faed7},
	{EGDB_WLD_TUN_V2, "db4.idx1", 4, 0xc3a84295},
	{EGDB_WLD_TUN_V1, "db4.idx", 4, 0x66389130},
	{EGDB_WLD_RUNLEN, "db4.idx", 4, 0xc5f47d67},
	{EGDB_MTC_RUNLEN, "db4.idx_mtc", 4, 0x2d675cd2},
	{EGDB_WLD_TUN_V2, "db3.idx1", 3, 0x8e96b77d},
	{EGDB_WLD_TUN_V1, "db3.idx", 3, 0x85aade3a},
	{EGDB_WLD_RUNLEN, "db3.idx", 3, 0x82f1a44e},
	{EGDB_MTC_RUNLEN, "db3.idx_mtc", 3, 0x2aebac80},
	{EGDB_WLD_TUN_V2, "db2.idx1", 2, 0x07a9f0f3},
	{EGDB_WLD_TUN_V1, "db2.idx", 2, 0x1b731f71},
	{EGDB_WLD_RUNLEN, "db2.idx", 2, 0xa833eebf},
};


int egdb_identify(char *directory, EGDB_TYPE *egdb_type, int *max_pieces)
{
	int i, len, pieces;
	unsigned int crc;
	FILE *fp;
	char *sep;
	EGDB_FIND_INFO *tablep;
	char name[MAXFILENAME];

	len = (int)strlen(directory);
	if (len && (directory[len - 1] != '\\'))
		sep = "\\";
	else
		sep = "";

	for (pieces = 9; pieces >= 2; --pieces) {
		for (i = 0; i < ARRAY_SIZE(egdb_find_table); ++i) {
			tablep = egdb_find_table + i;
			if (tablep->pieces != pieces)
				continue;
			std::sprintf(name, "%s%s%s", directory, sep, tablep->name);
			fp = std::fopen(name, "rb");
			if (fp) {

				crc = file_crc_calc(fp, 0);
				std::fclose(fp);
				if (egdb_find_table[i].crc == 0 || crc == egdb_find_table[i].crc) {
					*egdb_type = tablep->egdb_type;
					*max_pieces = tablep->pieces;
					return(0);
				}
			}
		}
	}

	/* Nothing found. */
	return(1);
}


