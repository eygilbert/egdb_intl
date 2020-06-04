#pragma once
#include <Windows.h>
#include <chrono>

class QPCTimer {
public:
	QPCTimer() {QueryPerformanceFrequency(&frequency);} 
	void reset(void) {QueryPerformanceCounter(&t0);}
	double elapsed_usec(void) {
		LARGE_INTEGER t1, delta_time;
		QueryPerformanceCounter(&t1);
		delta_time.QuadPart = t1.QuadPart - t0.QuadPart;
		delta_time.QuadPart *= 1000000;
		delta_time.QuadPart /= frequency.QuadPart;
		return((double)delta_time.QuadPart);
	}

private:
	LARGE_INTEGER frequency;
	LARGE_INTEGER t0;
};


class Timer {
public:
	void reset(void) {start = clock::now();}
	double elapsed_usec(void) {
		double elapsed = (double)(clock::now() - start).count();
		return(elapsed / 1000.0);
	}

private:
	typedef std::chrono::high_resolution_clock clock;

	clock::time_point start;
};
