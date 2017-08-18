#include "engine/project.h"
#include "engine/fen.h"
#include "egdb/egdb_intl.h"
#include "egdb/egdb_search.h"
#include "../egdb_intl_dll.h"
#include <Windows.h>
#include <comutil.h>
#include <stdio.h>


typedef struct {
	char filename[260];
	void (*msg_fn)(char const *msg);
	egdb_interface::EGDB_INFO db;
} DRIVER_INFO;

DRIVER_INFO drivers[4];

void msg_fn(char *filename, char const *msg)
{
	FILE *fp;

	if (strlen(filename) == 0)
		return;
	fp = fopen(filename, "a");
	if (!fp)
		return;

	fprintf(fp, "%s", msg);
	fclose(fp);
}


void msg_fn0(char const *msg)
{
	msg_fn(drivers[0].filename, msg);
}


void msg_fn1(char const *msg)
{
	msg_fn(drivers[1].filename, msg);
}


void msg_fn2(char const *msg)
{
	msg_fn(drivers[2].filename, msg);
}


void msg_fn3(char const *msg)
{
	msg_fn(drivers[3].filename, msg);
}


int check_handle(int handle)
{
	if (handle < 0 || handle >= ARRAY_SIZE(drivers))
		return(EGDB_BAD_HANDLE);

	if (drivers[handle].db.handle == nullptr)
		return(EGDB_NULL_INTERNAL_HANDLE);

	return(0);
}


extern "C" int __stdcall egdb_open(BSTR bstroptions,
			int cache_mb,
			BSTR bstrdirectory,
			BSTR bstrfilename)
{
	int i, max_pieces;
	egdb_interface::EGDB_TYPE egdb_type;
	char *options, *directory, *filename;
	FILE *fp;

	options = _com_util::ConvertBSTRToString(bstroptions);
	directory = _com_util::ConvertBSTRToString(bstrdirectory);
	filename = _com_util::ConvertBSTRToString(bstrfilename);

	drivers[0].msg_fn = msg_fn0;
	drivers[1].msg_fn = msg_fn1;
	drivers[2].msg_fn = msg_fn2;
	drivers[3].msg_fn = msg_fn3;

	/* Find an open entry in handle[]. */
	for (i = 0; i < ARRAY_SIZE(drivers); ++i)
		if (drivers[i].db.handle == nullptr)
			break;

	if (i >= ARRAY_SIZE(drivers))
		return(EGDB_DRIVER_TABLE_FULL);

	strcpy(drivers[i].filename, filename);

	/* Clear the log file. */
	if (strlen(filename) > 0) {
		fp = fopen(filename, "w");
		if (!fp)
			return(EGDB_FOPEN_FAIL);
		fclose(fp);
	}

	drivers[i].db.handle = egdb_interface::egdb_open(options, cache_mb, directory, drivers[i].msg_fn);
	if (drivers[i].db.handle) {
		int result;

		result = egdb_interface::egdb_identify(directory, &egdb_type, &max_pieces);
		if (result)
			return(EGDB_IDENTIFY_FAIL);

		if (egdb_type == egdb_interface::EGDB_WLD_TUN_V2)
			drivers[i].db.egdb_excludes_some_nonside_caps = 1;
		else
			drivers[i].db.egdb_excludes_some_nonside_caps = 0;
		drivers[i].db.dbpieces = max_pieces;
		drivers[i].db.dbpieces_1side = 5;
		drivers[i].db.db9_kings = 0;
		drivers[i].db.db8_kings_1side = 5;
	}
	else
		return(EGDB_OPEN_FAIL);

	delete []options;
	delete []directory;
	delete []filename;
	return(i);
}


extern "C" int __stdcall egdb_close(int handle)
{
	int result;

	result = check_handle(handle);
	if (result)
		return(result);

	result = egdb_interface::egdb_close(drivers[handle].db.handle);
	drivers[handle].db.handle = nullptr;
	drivers[handle].filename[0] = 0;

	return(result);
}


extern "C" int __stdcall egdb_identify(BSTR bstrdirectory, int *egdb_type, int *max_pieces)
{
	egdb_interface::EGDB_TYPE db_type;
	int result;
	char *dir = _com_util::ConvertBSTRToString(bstrdirectory);
	result = egdb_identify(dir, &db_type, max_pieces);
	*egdb_type = db_type;
	delete []dir;
	if (result)
		return(EGDB_IDENTIFY_FAIL);
	else
		return(0);
}


extern "C" int __stdcall egdb_lookup_fen(int handle, BSTR bstrfen, int cl)
{
	int color, result;
	egdb_interface::BOARD board;
	char *fen = _com_util::ConvertBSTRToString(bstrfen);

	result = check_handle(handle);
	if (result)
		return(result);

	result = parse_fen(fen, &board, &color);
	delete []fen;
	if (result)
		return(EGDB_FEN_ERROR);

	result = egdb_interface::egdb_lookup(drivers[handle].db.handle, (egdb_interface::EGDB_POSITION *)&board, color, cl);
	return(result);
}


extern "C" int __stdcall egdb_lookup_fen_with_search(int handle, BSTR bstrfen)
{
	int color, result;
	egdb_interface::BOARD board;
	char *fen = _com_util::ConvertBSTRToString(bstrfen);

	result = check_handle(handle);
	if (result)
		return(result);

	result = parse_fen(fen, &board, &color);
	delete []fen;
	if (result)
		return(EGDB_FEN_ERROR);

	result = drivers[handle].db.lookup_with_search(&board, color, 64, false);
	return(result);
}


