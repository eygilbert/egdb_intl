#pragma once

// -------------------------------------------------------------
// when using Visual C++, define this to rely on the Windows API
// -------------------------------------------------------------

#if defined(_MSC_VER)
        #define USE_WIN_API
#endif

// ------------
// 32 vs 64 bit
// ------------

#if defined(_MSC_VER)

        #if defined(_WIN64)
                #define ENVIRONMENT64
        #else
                #define ENVIRONMENT32
        #endif

#else

        // no large file support in 32-bit Linux
        #define ENVIRONMENT64

#endif

// ---------------
// multi-threading
// ---------------

#define USE_MULTI_THREADING

#ifdef USE_MULTI_THREADING

        #define USE_MUTEX    0
        #define USE_EXCHANGE 1
        #define USE_SPINLOCK 2

        #define LOCKING_METHOD 1

#endif

// --------------
// C++98 vs C++11
// --------------

#if __cplusplus < 201103L
        #define NULLPTR NULL
#else
        #define NULLPTR nullptr
#endif

// --------------
// cache alignment
// --------------

#define L1_CACHE_SIZE 64
#ifdef _MSC_VER
        #define CACHE_ALIGN __declspec(align(L1_CACHE_SIZE))
#else
        #define CACHE_ALIGN __attribute__((aligned(L1_CACHE_SIZE)))
#endif
