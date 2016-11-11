#include "egdb/platform.h"
#include <cstdint>
#include <ctime>
#include <inttypes.h>
#include <thread>

using namespace egdb_interface;

const int maxthreads = 6;
const int64_t maxiters = 10000000;
LOCKT lock;

int64_t shared;

void incfunc(int64_t incr)
{
	int64_t i;

	for (i = 0; i < maxiters; ++i) {
		take_lock(lock);
		shared += incr;
		release_lock(lock);
	}
	for (i = 0; i < maxiters; ++i) {
		take_lock(lock);
		shared -= incr;
		release_lock(lock);
	}
}


int main()
{
	int i;
	std::clock_t t0;
 	struct thread_t {
		std::thread threadobj;
	};
	thread_t tinfo[maxthreads];

	t0 = std::clock();
	init_lock(lock);
	for (i = 0; i < maxthreads; ++i)
		tinfo[i].threadobj = std::thread(incfunc, (int64_t)1);
 	for (i = 0; i < maxthreads; ++i) {
		printf("Join thread %d\n", i);
		tinfo[i].threadobj.join();
	}

	std::printf("%.1f secs, final shared value is %" PRId64 " check\n", (std::clock() - t0) / (double)CLOCKS_PER_SEC, shared);

	return 0;
}

