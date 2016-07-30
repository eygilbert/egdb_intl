#pragma once

#ifdef _MSC_VER
#include <Windows.h>

#define MAXFILENAME MAX_PATH

inline
unsigned long get_page_size()
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwPageSize;
}

inline
unsigned long get_allocation_granularity()
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwAllocationGranularity;
}

inline
int get_mem_available_mb(void)
{
	MEMORYSTATUS memstat;

	GlobalMemoryStatus(&memstat);
	return((int)(memstat.dwAvailPhys / (1024 * 1024)));
}

#endif

#define USE_WIN_API 
#ifdef USE_WIN_API
#include <Windows.h>

inline
void *aligned_large_alloc(size_t size)
{
	return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
}

inline 
void virtual_free(void *ptr)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}

typedef HANDLE		FILE_HANDLE;
typedef BOOL		BOOL_T;
typedef DWORD		DWORD_T;

/* Upper and lower words of a 64-bit int, for large file access. */
typedef union {
	int64_t word64;
	struct {
		unsigned long low32;
		unsigned long high32;
	} words32;
} I64_HIGH_LOW;

inline
FILE_HANDLE open_file(char const* name)
{
	FILE_HANDLE return_value = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_NO_BUFFERING, NULL);
	return return_value == INVALID_HANDLE_VALUE ? nullptr : return_value;
}

inline
int64_t get_file_size(FILE_HANDLE handle)
{
	I64_HIGH_LOW filesize;

	filesize.words32.low32 = GetFileSize(handle, &filesize.words32.high32);
	return(filesize.word64);
}

inline
int set_file_pointer(FILE_HANDLE handle, int64_t pos)
{
	I64_HIGH_LOW filepos;
	filepos.word64 = pos;
	return SetFilePointer(handle, filepos.words32.low32, (long *)&filepos.words32.high32, FILE_BEGIN) == INVALID_SET_FILE_POINTER;
}

inline
BOOL_T read_from_file(FILE_HANDLE stream, unsigned char *buffer, DWORD_T count, DWORD_T *bytes_read)
{
	return ReadFile(stream, buffer, count, bytes_read, NULL);
}

inline
int close_file(FILE_HANDLE ptr)
{
	return CloseHandle(ptr);
}

#else

#include <cstdio>
#include <cstdlib>

#include <malloc.h>

inline
void *aligned_large_alloc(size_t size)
{
	// Microsoft does not yet support C11
	// return std::aligned_alloc(size, get_allocation_granularity());
	return _aligned_malloc(size, get_allocation_granularity());	
}

inline
void virtual_free(void *ptr)
{
	// Microsoft does not yet support C11
	// std::free(ptr);
	_aligned_free(ptr);	
}

typedef std::FILE*	FILE_HANDLE;
typedef bool		BOOL_T;
typedef size_t		DWORD_T;

inline
FILE_HANDLE open_file(char const* name)
{
	FILE_HANDLE stream = std::fopen(name, "r");	// read-only
	if (stream != nullptr)
		std::setbuf(stream, nullptr);			// no-buffering
	return stream;
}

inline
// On 64-bit Windows, a <long> is 32-bit, not 64-bit
//long 
int64_t get_file_size(FILE_HANDLE stream)
{
	// std::ftell(stream);
	return _ftelli64(stream);
}

inline
int set_file_pointer(FILE_HANDLE stream, int64_t offset)
{
	// On 64-bit Windows, a <long> is 32-bit, not 64-bit
	// std::fseek(stream, offset, SEEK_SET);
	return _fseeki64(stream, offset, SEEK_SET);
}

inline
BOOL_T read_from_file(FILE_HANDLE stream, unsigned char *buffer, DWORD_T count, DWORD_T *bytes_read)
{
	*bytes_read = std::fread(buffer, 1, count, stream);
	return true;
}

inline
int close_file(FILE_HANDLE stream)
{
	return !std::fclose(stream);	// CloseHandle returns zero on failure, std::fclose returns zero on success
}

#endif
