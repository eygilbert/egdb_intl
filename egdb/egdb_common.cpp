#include "egdb/egdb_common.h"
#include "builddb/indexing.h"
#include "egdb/egdb_intl.h"
#include "egdb/platform.h"
#include "engine/bicoef.h"
#include "engine/bitcount.h"
#include "engine/board.h"
#include "engine/bool.h"
#include "engine/lock.h"

int get_num_subslices(int nbm, int nbk, int nwm, int nwk)
{
	int64_t size, last;
	int num_subslices;

	size = getdatabasesize_slice(nbm, nbk, nwm, nwk);
	if (size > MAX_SUBSLICE_INDICES) {
		num_subslices = (int)(size / (int64_t)MAX_SUBSLICE_INDICES);
		last = size - (int64_t)num_subslices * (int64_t)MAX_SUBSLICE_INDICES;
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
