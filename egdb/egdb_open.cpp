#include "egdb/egdb_common.h"
#include "egdb/egdb_intl.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace egdb_interface {

EGDB_DRIVER *egdb_open_wld_runlen(int pieces, int cache_mb, char const *directory, void (*msg_fn)(char const*), EGDB_TYPE db_type);
EGDB_DRIVER *egdb_open_mtc_runlen(int pieces, int cache_mb, char const *directory, void (*msg_fn)(char const*), EGDB_TYPE db_type);
EGDB_DRIVER *egdb_open_wld_tun_v1(int pieces, int cache_mb, char const *directory, void (*msg_fn)(char const*), EGDB_TYPE db_type);
EGDB_DRIVER *egdb_open_wld_tun_v2(int pieces, int cache_mb, char const *directory, void (*msg_fn)(char const*), EGDB_TYPE db_type);
EGDB_DRIVER *egdb_open_dtw(int pieces, int cache_mb, char const *directory, void (*msg_fn)(char const*), EGDB_TYPE db_type);


static void parse_options(char const *options, int *pieces)
{
	*pieces = 0;
	if (options == NULL)
		return;

	{
		char const *p = std::strstr(options, "maxpieces");
		if (p) {
			p += std::strlen("maxpieces");
			while (*p != '=')
				++p;
			++p;
			while (std::isspace(*p))
				++p;
			*pieces = std::atoi(p);
		}
	}
}


EGDB_DRIVER *egdb_open(char const *options, 
						int cache_mb,
						char const *directory,
						void (*msg_fn)(char const*))
{
	int stat;
	int max_pieces, pieces;
	EGDB_TYPE db_type;
	char msg[MAXMSG];
	EGDB_DRIVER *handle = 0;

	stat = egdb_identify(directory, &db_type, &max_pieces);
	if (stat) {
		std::sprintf(msg, "No egdb found\n");
		(*msg_fn)(msg);
		return(0);
	}
	parse_options(options, &pieces);
	if (pieces > 0)
		pieces = (std::min)(max_pieces, pieces);
	else
		pieces = max_pieces;

	switch (db_type) {
	case EGDB_WLD_RUNLEN:
		handle = egdb_open_wld_runlen(pieces, cache_mb, directory, msg_fn, db_type);
		break;

	case EGDB_WLD_TUN_V1:
		handle = egdb_open_wld_tun_v1(pieces, cache_mb, directory, msg_fn, db_type);
		break;

	case EGDB_WLD_TUN_V2:
		handle = egdb_open_wld_tun_v2(pieces, cache_mb, directory, msg_fn, db_type);
		break;

	case EGDB_MTC_RUNLEN:
		handle = egdb_open_mtc_runlen(pieces, cache_mb, directory, msg_fn, db_type);
		break;

	case EGDB_DTW:
		handle = egdb_open_dtw(pieces, cache_mb, directory, msg_fn, db_type);
		break;
	}

	return(handle);
}

}	// namespace egdb_interface

