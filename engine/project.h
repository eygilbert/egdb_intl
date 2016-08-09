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
