#include "egdb/egdb_common.h"
#include "builddb/indexing.h"
#include "egdb/egdb_intl.h"
#include "egdb/platform.h"
#include "engine/bicoef.h"
#include "engine/bitcount.h"
#include "engine/board.h"
#include "engine/bool.h"

namespace egdb_interface {

int egdb_lookup(EGDB_DRIVER *handle, EGDB_POSITION const *position, int color, int cl)
{
	return handle->lookup(handle, const_cast<EGDB_POSITION*>(position), color, cl);
}

int egdb_close(EGDB_DRIVER *handle)
{
	return handle->close(handle);
}

int egdb_verify(EGDB_DRIVER const *handle, void (*msg_fn)(char const *msg), int *abort, EGDB_VERIFY_MSGS *msgs)
{
	return handle->verify(handle, msg_fn, abort, msgs);
}

int egdb_get_pieces(EGDB_DRIVER const *handle, int *max_pieces, int *max_pieces_1side)
{
	return handle->get_pieces(const_cast<EGDB_DRIVER*>(handle), max_pieces, max_pieces_1side);
}

void egdb_reset_stats(EGDB_DRIVER *handle)
{
	handle->reset_stats(handle);
}

EGDB_STATS *egdb_get_stats(EGDB_DRIVER const *handle)
{
	return handle->get_stats(const_cast<EGDB_DRIVER*>(handle));
}

EGDB_TYPE egdb_get_type(EGDB_DRIVER const *handle)
{
	return handle->get_type(const_cast<EGDB_DRIVER *>(handle));
}

bool is_wld(EGDB_DRIVER const *handle)
{
	EGDB_TYPE type = egdb_get_type(handle);
	if (type == EGDB_WLD_RUNLEN || type == EGDB_WLD_TUN_V1 || type == EGDB_WLD_TUN_V2)
		return(true);
	else
		return(false);
}

bool is_dtw(EGDB_DRIVER const *handle)
{
	EGDB_TYPE type = egdb_get_type(handle);
	if (type == EGDB_DTW)
		return(true);
	else
		return(false);
}

bool is_mtc(EGDB_DRIVER const *handle)
{
	EGDB_TYPE type = egdb_get_type(handle);
	if (type == EGDB_MTC_RUNLEN)
		return(true);
	else
		return(false);
}

int get_num_subslices(int bm, int bk, int wm, int wk, uint32_t subslice_size)
{
	int64_t size, last;
	int num_subslices;

	size = getdatabasesize_slice(bm, bk, wm, wk);
	if (size > subslice_size) {
		num_subslices = (int)(size / (int64_t)subslice_size);
		last = size - (int64_t)num_subslices * (int64_t)subslice_size;
		if (last)
			++num_subslices;
	}
	else
		num_subslices = 1;
	return(num_subslices);
}


/*
 * Read size bytes from a file.
 * Since the file is read using ReadFile and the handle may have 
 * been opened with FILE_FLAG_NO_BUFFERING, the reads must be in
 * integer multiples of pagesize, and the read buffer must be sized accordingly.
 * Return 1 on success, 0 on error.
 */
int read_file(FILE_HANDLE fp, unsigned char *buf, size_t size, int pagesize)
{
	BOOL_T stat;
	DWORD_T bytes_read;
	DWORD_T request_size;
	const int CHUNKSIZE = 0x100000;

	while (size > 0) {
		if (size >= CHUNKSIZE)
			request_size = CHUNKSIZE;
		else
			request_size = ROUND_UP((int)size, pagesize);
		stat = read_from_file(fp, buf, request_size, &bytes_read);
		if (!stat)
			return(0);
		if (bytes_read < request_size)
			return(1);
		buf += bytes_read;
		size -= bytes_read;
	}
	return(1);
}

}	// namespace egdb_interface
