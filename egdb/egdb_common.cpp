#include "egdb_common.h"

// builddb
#include "indexing.h"

// egdb
#include "egdb_intl.h"

// engine
#include "bicoef.h"
#include "bitcount.h"
#include "board.h"
#include "bool.h"
#include "lock.h"
#include "project.h"

#include <Windows.h>

int get_num_subslices(int nbm, int nbk, int nwm, int nwk)
{
	int64 size, last;
	int num_subslices;

	size = getdatabasesize_slice(nbm, nbk, nwm, nwk);
	if (size > MAX_SUBSLICE_INDICES) {
		num_subslices = (int)(size / (int64)MAX_SUBSLICE_INDICES);
		last = size - (int64)num_subslices * (int64)MAX_SUBSLICE_INDICES;
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
int read_file(HANDLE fp, unsigned char *buf, size_t size, int pagesize)
{
	int stat;
	DWORD bytes_read;
	DWORD request_size;
	const int CHUNKSIZE = 0x100000;

	while (size > 0) {
		if (size >= CHUNKSIZE)
			request_size = CHUNKSIZE;
		else
			request_size = ROUND_UP((int)size, pagesize);
		stat = ReadFile(fp, buf, request_size, &bytes_read, NULL);
		if (!stat)
			return(0);
		if (bytes_read < request_size)
			return(1);
		buf += bytes_read;
		size -= bytes_read;
	}
	return(1);
}


int set_file_pointer(HANDLE handle, int64 pos)
{
	I64_HIGH_LOW filepos;

	/* Seek the start position. */
	filepos.word64 = pos;
	if (SetFilePointer(handle, filepos.words32.low32, (long *)&filepos.words32.high32, FILE_BEGIN) == -1)
		return(1);

	return(0);
}


int64 get_file_size(HANDLE handle)
{
	I64_HIGH_LOW filesize;

	filesize.words32.low32 = GetFileSize(handle, &filesize.words32.high32);
	return(filesize.word64);
}


void *aligned_large_alloc(size_t size)
{
	void *blockp;
	blockp = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
	return(blockp);
}


int get_mem_available_mb(void)
{
	MEMORYSTATUS memstat;

	GlobalMemoryStatus(&memstat);
	return((int)(memstat.dwAvailPhys / (1024 * 1024)));
}


int get_pagesize(void)
{
	SYSTEM_INFO sysinfo;

	GetSystemInfo(&sysinfo);
	return(sysinfo.dwPageSize);
}

