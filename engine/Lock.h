#include <intrin.h>

#define USE_CRITICAL_SECTION 0

#if USE_CRITICAL_SECTION

#define LOCKT CRITICAL_SECTION
#define init_lock(a) InitializeCriticalSection(&a)
#define take_lock(a) EnterCriticalSection(&a)
#define release_lock(a) LeaveCriticalSection(&a)

#else

#include <intrin.h> 

typedef volatile CACHE_ALIGN long LOCKT[L1_CACHE_SIZE / sizeof(long)]; 
typedef volatile long *const LOCKT_PTR; 
#pragma intrinsic (_InterlockedExchange) 
#define interlocked_exchange(target, value) _InterlockedExchange(target, value) 

#define is_locked(lock) ((lock)[0])
#define set_lock_state(lock, state) ((lock)[0] = (state)) 
#define init_lock(lock) set_lock_state(lock, 0) 
static inline void volatile release_lock(LOCKT_PTR lock) {set_lock_state(lock, 0);} 
static inline void volatile take_lock(LOCKT_PTR lock) {while(is_locked(lock) || interlocked_exchange(lock, 1));}
#endif
