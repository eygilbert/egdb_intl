#pragma once
#include "engine/config.h"

#ifdef USE_MULTI_THREADING

	#if defined(_MSC_VER) && defined(USE_WIN_API)

		#if (LOCKING_METHOD == USE_MUTEX)

			#include <Windows.h>

                        namespace egdb_interface {

                        class critical_section
                        {
                                CRITICAL_SECTION m_critical_section;
                        public:
                                critical_section()
                                {
                                        InitializeCriticalSection(&m_critical_section);
                                }

                                void lock()
                                {
                                        EnterCriticalSection(&m_critical_section);
                                }

                                void unlock()
                                {
                                        LeaveCriticalSection(&m_critical_section);
                                }
                        };

                        typedef critical_section LOCK_TYPE;

                        }       // namespace

		#elif (LOCKING_METHOD == USE_EXCHANGE)

			#include <intrin.h>
                        #pragma intrinsic (_InterlockedExchange)

                        namespace egdb_interface {

                        class interlocked_exchange
                        {
                                CACHE_ALIGN volatile long m_data[L1_CACHE_SIZE / sizeof(long)] = { 0 };
                        public:
                                void lock()
                                {
                                        while(m_lock[0] || _InterlockedExchange(&m_lock[0], 1));
                                }

                                void unlock()
                                {
                                        m_lock[0] = 0;
                                }
                        };

                        typedef interlocked_exchange LOCK_TYPE;

                        }       // namespace
                #endif

	#else
                #if (LOCKING_METHOD == USE_MUTEX)

                        #include <mutex>

                        typedef std::mutex LOCK_TYPE;

                #elif (!USE_WIN_API && LOCKING_METHOD == USE_EXCHANGE)

                        namespace egdb_interface {

                        class interlocked_exchange
                        {
                                CACHE_ALIGN volatile char m_lock[L1_CACHE_SIZE] = { 0 };
                        public:
                                void lock()
                                {
                                        while(m_lock[0] || __sync_lock_test_and_set(&m_lock[0], 1));
                                }

                                void unlock()
                                {
                                        __sync_lock_release(&m_lock[0]);
                                }
                        };

                        typedef interlocked_exchange LOCK_TYPE;

                        }       // namespace

                #elif (LOCKING_METHOD == USE_SPINLOCK)

                        #include <atomic>

                        namespace egdb_interface {

                        class spinlock
                        {
                                std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
                        public:
                                void lock()
                                {
                                        while(m_flag.test_and_set(std::memory_order_acquire));
                                }

                                void unlock()
                                {
                                        m_flag.clear(std::memory_order_release);
                                }
                        };

                        typedef spinlock LOCK_TYPE;

                        }       // namespace

                 #endif

	#endif

#else

        namespace egdb_interface {

        class null_lock
        {
        public:
                void lock(){}
                void unlock() {}
        };

        typedef null_lock LOCK_TYPE;

        }       // namespace

#endif
