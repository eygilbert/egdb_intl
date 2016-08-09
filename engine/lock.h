#pragma once
#include "engine/project.h"	// CACHE_ALIGN, L1_CACHE_SIZE

#if defined(_MSC_VER) && defined(USE_WIN_API)

	#define USE_CRITICAL_SECTION 0

	#if USE_CRITICAL_SECTION

		#include <Windows.h>

		#define LOCKT CRITICAL_SECTION
		#define STATIC_DEFINE_LOCK(lock) static LOCKT lock
		#define init_lock(a) InitializeCriticalSection(&a)
		#define release_lock(a) LeaveCriticalSection(&a)
		#define take_lock(a) EnterCriticalSection(&a)

	#else 

		#include <intrin.h> 

namespace egdb_interface {

typedef volatile CACHE_ALIGN long LOCKT[L1_CACHE_SIZE / sizeof(long)]; 
typedef volatile long *const LOCKT_PTR; 
#pragma intrinsic (_InterlockedExchange) 
#define interlocked_exchange(target, value) _InterlockedExchange(target, value) 

		#define is_locked(lock) ((lock)[0])
		#define set_lock_state(lock, state) ((lock)[0] = (state))
		#define STATIC_DEFINE_LOCK(lock) static LOCKT lock
		#define init_lock(lock) set_lock_state(lock, 0) 
		static inline void volatile take_lock(LOCKT_PTR lock) { while (is_locked(lock) || interlocked_exchange(lock, 1)); }
		static inline void volatile release_lock(LOCKT_PTR lock) {set_lock_state(lock, 0);}

}	// namespace

	#endif

#else

	#include <atomic>

	namespace egdb_interface {
	
	typedef std::atomic_flag LOCKT;

	#define STATIC_DEFINE_LOCK(lock) static LOCKT lock = ATOMIC_FLAG_INIT
	#define init_lock(lock)
	static inline void take_lock(LOCKT& lock) { while (lock.test_and_set(std::memory_order_acquire)) { ; } }
	static inline void release_lock(LOCKT& lock) { lock.clear(std::memory_order_release); }

	}	// namespace
	
#endif
