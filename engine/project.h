#pragma once

#ifdef _DEBUG
#undef NDEBUG
#else
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <cassert>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define L1_CACHE_SIZE 64
#define CACHE_ALIGN __declspec(align(L1_CACHE_SIZE))

/* types */
typedef signed char int8;
typedef unsigned char uint8;

typedef signed short int16;
typedef unsigned short uint16;

typedef signed int int32;
typedef unsigned int uint32;

typedef signed long long int int64;
typedef unsigned long long int uint64;


