#pragma once

// ------------------
// System information
// ------------------

#ifdef _MSC_VER

	#include <intrin.h>
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

	inline
	bool check_cpu_has_popcount()
	{
		int cpuinfo[4] = { -1 };

		__cpuid(cpuinfo, 1);
		return((cpuinfo[2] >> 23) & 1);
	}

#endif

//#define USE_WIN_API 

// ------
// Memory
// ------

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

#endif

// --------
// File I/O
// --------

#ifdef USE_WIN_API

	#include <cstdint>
	
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

#endif

// ------
// Memory
// ------

#ifndef USE_WIN_API

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

#endif

// --------
// File I/O
// --------

#ifndef USE_WIN_API

	#include <cstdint>

	typedef std::FILE*	FILE_HANDLE;
	typedef bool		BOOL_T;
	typedef size_t		DWORD_T;

	inline
	FILE_HANDLE open_file(char const* name)
	{
		FILE_HANDLE stream = std::fopen(name, "rb");	// read-only
		if (stream != nullptr)
			std::setbuf(stream, NULL);				// no-buffering
		return stream;
	}

	inline
	// On 64-bit Windows, a <long> is 32-bit, not 64-bit
	//long 
	int64_t get_file_size(FILE_HANDLE stream)
	{
		int64_t const curr = _ftelli64(stream);
		_fseeki64(stream, 0, SEEK_END);
		int64_t const size = _ftelli64(stream);
		_fseeki64(stream, curr, SEEK_SET);
		return size;
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

// ---------
// Threading
// ---------

#include "engine/lock.h"

// -------------
// Bit twiddling
// -------------

#ifdef _MSC_VER

#include <intrin.h>
#include <cstdint>

	inline 
	int bit_scan_forward(uint32_t x)
	{
		unsigned long bitpos;

		if (_BitScanForward(&bitpos, x))
			return(bitpos);
		else
			return(0);
	}

	inline
	int bit_scan_reverse(uint32_t x)
	{
		unsigned long bitpos;

		if (_BitScanReverse(&bitpos, x))
			return(bitpos);
		else
			return(0);
	}

	inline
	int bit_pop_count(uint32_t x)
	{
		return __popcnt(x);
	}

	#ifdef _WIN64

		inline 
		int bit_scan_forward64(uint64_t x)
		{
			unsigned long bitpos;

			if (_BitScanForward64(&bitpos, x))
				return(bitpos);
			else
				return(0);
		}

		inline 
		int bit_scan_reverse64(uint64_t x)
		{
			unsigned long bitpos;

			if (_BitScanReverse64(&bitpos, x))
				return(bitpos);
			else
				return(0);
		}

		inline
		int bit_pop_count64(uint64_t x)
		{
			return static_cast<int>(__popcnt64(x));
		}

	#endif

#endif

