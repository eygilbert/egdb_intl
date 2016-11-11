#pragma once
#include "engine/config.h"

#ifdef USE_MULTI_THREADING

	#if defined(_MSC_VER) && defined(USE_WIN_API)

		#if (LOCKING_METHOD == USE_MUTEX)

			#include <Windows.h>

			#define LOCKT CRITICAL_SECTION
			#define STATIC_DEFINE_LOCK(lock) static LOCKT lock
			#define init_lock(a) InitializeCriticalSection(&a)
			#define release_lock(a) LeaveCriticalSection(&a)
			#define take_lock(a) EnterCriticalSection(&a)

		#elif (LOCKING_METHOD == ATOMIC_EXCHANGE)

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
                #if (LOCKING_METHOD == USE_MUTEX)

                        #include <mutex>

                        #define LOCKT std::mutex
                        #define STATIC_DEFINE_LOCK(lock) static LOCKT lock
                        #define init_lock(a)
                        #define release_lock(a) (a).unlock()
                        #define take_lock(a) (a).lock()

                #elif (!USE_WIN_API && LOCKING_METHOD == USE_ATOMIC_EXCHANGE)

                        namespace egdb_interface {

                        typedef CACHE_ALIGN long LOCKT[L1_CACHE_SIZE / sizeof(long)];
                        typedef long *const LOCKT_PTR;
                        #define interlocked_exchange(target, value) __sync_lock_test_and_set(target, value)

                        #define is_locked(lock) ((lock)[0])
                        #define set_lock_state(lock, state) ((lock)[0] = (state))
                        #define STATIC_DEFINE_LOCK(lock) static LOCKT lock
                        #define init_lock(lock) set_lock_state(lock, 0)
                        static inline void take_lock(LOCKT_PTR lock) { while (is_locked(lock) || interlocked_exchange(lock, 1)); }
                        static inline void release_lock(LOCKT_PTR lock) {set_lock_state(lock, 0);}

                        }       // namespace

                #elif (LOCKING_METHOD == USE_ATOMIC_FLAG)

                        #include <atomic>

                        namespace egdb_interface {

                        typedef std::atomic_flag LOCKT;

                        #define STATIC_DEFINE_LOCK(lock) static LOCKT lock = ATOMIC_FLAG_INIT
                        #define init_lock(lock)
                        static inline void take_lock(LOCKT& lock) { while (lock.test_and_set(std::memory_order_acquire)) { ; } }
                        static inline void release_lock(LOCKT& lock) { lock.clear(std::memory_order_release); }

                        }       // namespace

                #elif (LOCKING_METHOD == USE_ATOMIC_BOOL)

                        #include <atomic>

                        namespace egdb_interface {

                        typedef std::atomic<bool> LOCKT;

                        #define STATIC_DEFINE_LOCK(lock) static LOCKT lock(false)
                        #define init_lock(lock)
                        static inline void take_lock(LOCKT& lock) { while (lock.exchange(true)) { ; } }
                        static inline void release_lock(LOCKT& lock) { lock = false; }

                        }       // namespace

                #endif

	#endif

#else

        namespace egdb_interface {

        typedef int LOCKT;

        #define STATIC_DEFINE_LOCK(lock) static LOCKT lock = 0
        #define init_lock(lock)
        static inline void take_lock(LOCKT& /*lock*/) {  }
        static inline void release_lock(LOCKT& /*lock*/) {  }

        }	// namespace

#endif
