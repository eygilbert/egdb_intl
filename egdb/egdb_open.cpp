// egdb
#include "egdb_common.h"
#include "egdb_intl.h"

// engine
#include "Lock.h"
#include "project.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

EGDB_DRIVER *egdb_open_wld_runlen(int pieces, int kings_1side_8pcs, int cache_mb, char *directory, void (*msg_fn)(char *), EGDB_TYPE db_type);
EGDB_DRIVER *egdb_open_mtc_runlen(int pieces, int kings_1side_8pcs, int cache_mb, char *directory, void (*msg_fn)(char *), EGDB_TYPE db_type);
EGDB_DRIVER *egdb_open_wld_huff(int pieces, int kings_1side_8pcs, int cache_mb, char *directory, void (*msg_fn)(char *), EGDB_TYPE db_type);
EGDB_DRIVER *egdb_open_wld_tun_v1(int pieces, int kings_1side_8pcs, int cache_mb, char *directory, void (*msg_fn)(char *), EGDB_TYPE db_type);
EGDB_DRIVER *egdb_open_wld_tun_v2(int pieces, int kings_1side_8pcs, int cache_mb, char *directory, void (*msg_fn)(char *), EGDB_TYPE db_type);


static void parse_options(char *options, int *pieces, int *kings_1side_8pcs)
{
	char *p;

	*pieces = 0;
	*kings_1side_8pcs = -1;
	p = strstr(options, MAXPIECES_OPTSTR);
	if (p) {
		p += strlen(MAXPIECES_OPTSTR);
		while (*p != '=')
			++p;
		++p;
		while (isspace(*p))
			++p;
		*pieces = atoi(p);
	}
	p = strstr(options, MAXKINGS_1SIDE_8PCS_OPTSTR);
	if (p) {
		p += strlen(MAXKINGS_1SIDE_8PCS_OPTSTR);
		while (*p != '=')
			++p;
		++p;
		while (isspace(*p))
			++p;
		*kings_1side_8pcs = atoi(p);
	}
}


EGDB_DRIVER *egdb_open(char *options, 
						int cache_mb,
						char *directory,
						void (*msg_fn)(char *))
{
	int stat;
	int max_pieces, pieces, kings_1side_8pcs;
	EGDB_TYPE db_type;
	char msg[MAXMSG];
	EGDB_DRIVER *handle = 0;

	stat = egdb_identify(directory, &db_type, &max_pieces);
	if (stat) {
		sprintf(msg, "No egdb found\n");
		(*msg_fn)(msg);
		return(0);
	}
	parse_options(options, &pieces, &kings_1side_8pcs);
	if (pieces > 0)
		pieces = __min(max_pieces, pieces);
	else
		pieces = max_pieces;

	switch (db_type) {
	case EGDB_WLD_RUNLEN:
		handle = egdb_open_wld_runlen(pieces, kings_1side_8pcs, cache_mb, directory, msg_fn, db_type);
		break;

	case EGDB_WLD_HUFFMAN:
		handle = egdb_open_wld_huff(pieces, kings_1side_8pcs, cache_mb, directory, msg_fn, db_type);
		break;

	case EGDB_WLD_TUN_V1:
		handle = egdb_open_wld_tun_v1(pieces, kings_1side_8pcs, cache_mb, directory, msg_fn, db_type);
		break;

	case EGDB_WLD_TUN_V2:
		handle = egdb_open_wld_tun_v2(pieces, kings_1side_8pcs, cache_mb, directory, msg_fn, db_type);
		break;

	case EGDB_MTC_RUNLEN:
		handle = egdb_open_mtc_runlen(pieces, kings_1side_8pcs, cache_mb, directory, msg_fn, db_type);
		break;
	}

	return(handle);
}


