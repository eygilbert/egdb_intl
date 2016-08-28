#pragma once

// -------------------------------------------------------------
// when using Visual C++, define this to rely on the Windows API
// -------------------------------------------------------------

#ifdef _MSC_VER
	#define USE_WIN_API 
#endif

// ------------
// 32 vs 64 bit
// ------------

#ifdef _MSC_VER

	#if _WIN64
		#define ENVIRONMENT64
	#else
		#define ENVIRONMENT32
	#endif

#else

	#if __x86_64__ || __ppc64__
		#define ENVIRONMENT64
	#else
		#define ENVIRONMENT32
	#endif

#endif

// --------------
// C++98 vs C++11
// --------------

#if __cplusplus < 201103L
	#define NULLPTR NULL
#else
	#define NULLPTR nullptr
#endif

// ---------------
// multi-threading
// ---------------

#define USE_MULTI_THREADING

// ------------------
// System information
// ------------------

#ifdef _MSC_VER

	#include <intrin.h>
	#include <Windows.h>

	#define MAXFILENAME MAX_PATH

	namespace egdb_interface {

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

	}	// namespace

#else

	#include <unistd.h>

	namespace egdb_interface {

	#define MAXFILENAME 260	// equal to Windows MAX_PATH

	inline
	unsigned long get_page_size()
	{
		return sysconf(_SC_PAGESIZE);
	}

	inline
	unsigned long get_allocation_granularity()
	{
		return sysconf(_SC_PAGESIZE);
	}

	inline
	int get_mem_available_mb(void)
	{
		return (int)(sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE) / (1024 * 1024));
	}

	inline
	bool check_cpu_has_popcount()
	{
		return __builtin_cpu_supports("popcnt");
	}

	}	// namespace
#endif

// ------
// Memory
// ------

#ifdef _MSC_VER

	#ifdef USE_WIN_API

		#include <Windows.h>

		namespace egdb_interface {

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

		}	// namespace
	#else

		// Visual C++ does not support C11 yet

		#include <malloc.h>

		namespace egdb_interface {

		inline
		void *aligned_large_alloc(size_t size)
		{
			// https://msdn.microsoft.com/en-us/library/8z34s9c6.aspx
			// NOTE: order of arguments is (size, alignment)
			return _aligned_malloc(size, get_allocation_granularity());
		}

		inline
		void virtual_free(void *ptr)
		{
			_aligned_free(ptr);
		}

		}	// namespace
	#endif

#else

	#include <cstdlib>

	namespace egdb_interface {

	inline
	void *aligned_large_alloc(size_t size)
	{
		// http://en.cppreference.com/w/c/memory/aligned_alloc
		// NOTE: order of arguments is (alignment, size)
		return aligned_alloc(get_allocation_granularity(), size);
	}

	inline
	void virtual_free(void *ptr)
	{
		std::free(ptr);
	}

	}	// namespace
#endif

// --------
// File I/O
// --------

#if defined(_MSC_VER) && defined(USE_WIN_API)

	#include <cassert>
	#include <stdint.h>
	
	namespace egdb_interface {

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
		FILE_HANDLE return_value = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
		assert(return_value != NULL);	// Win API is inconsistent which invalid handle values it returns
										// CreateFile uses -1, but other functions use 0
										// the assert guards against the outside chance that 0 is a *valid handle* from CreateFile
		return return_value == INVALID_HANDLE_VALUE ? NULLPTR : return_value;
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

	}	// namespace
#else

	#include <stdint.h>
	#include <cstdio>

	namespace egdb_interface {

	typedef std::FILE*	FILE_HANDLE;
	typedef bool		BOOL_T;
	typedef size_t		DWORD_T;

	inline
	FILE_HANDLE open_file(char const* name)
	{
		return std::fopen(name, "rb");	// read-only
	}

	}	// namespace

	#ifdef _MSC_VER
		// On both 32-bit and 64-bit Windows, a <long> is 32-bit, not 64-bit

		#include <stdio.h>

		namespace egdb_interface {

		inline
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
			return _fseeki64(stream, offset, SEEK_SET);
		}

		}	// namespace
	#else

		namespace egdb_interface {

		inline
		int64_t get_file_size(FILE_HANDLE stream)
		{
			int64_t const curr = std::ftell(stream);
			std::fseek(stream, 0, SEEK_END);
			int64_t const size = std::ftell(stream);
			std::fseek(stream, curr, SEEK_SET);
			return size;
		}

		inline
		int set_file_pointer(FILE_HANDLE stream, int64_t offset)
		{
			return std::fseek(stream, offset, SEEK_SET);
		}

		}	// namespace

	#endif

	namespace egdb_interface {

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

	}	// namespace

#endif

// -------
// Locking
// -------

#include "engine/lock.h"

// -------------
// Bit twiddling
// -------------

#ifdef _MSC_VER

	#include <intrin.h>
	#include <stdint.h>

	namespace egdb_interface {

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

	}	// namespace

	#ifdef ENVIRONMENT64

		namespace egdb_interface {

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

		}	// namespace

	#endif

#else 

	#include <stdint.h>

	namespace egdb_interface {

	inline
	int bit_scan_forward(uint32_t x)
	{
		return x ? __builtin_ctzll(x) : 0;
	}

	inline
	int bit_scan_reverse(uint32_t x)
	{
		return x ? 31 - __builtin_clzll(x) : 0;
	}

	inline
	int bit_pop_count(uint32_t x)
	{
		return __builtin_popcount(x);
	}

	}	// namespace

	#ifdef ENVIRONMENT64

		namespace egdb_interface {

		inline
		int bit_scan_forward64(uint64_t x)
		{
			return x ? __builtin_ctzll(x) : 0;
		}

		inline
		int bit_scan_reverse64(uint64_t x)
		{
			return x ? 63 - __builtin_clzll(x) : 0;
		}

		inline
		int bit_pop_count64(uint64_t x)
		{
			return __builtin_popcountll(x);
		}

		}	// namespace
	#endif

#endif

// -------
// Strings
// -------

#ifdef _MSC_VER

	#include <string.h>

	namespace egdb_interface {
	
	inline 
	int strcasecmp(char const *a, char const *b) { return _stricmp(a, b); }

	}	// namespace

#else

	#include<string.h>	// provides strcasecmp

#endif
