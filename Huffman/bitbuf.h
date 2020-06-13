#pragma once
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

class Bitbuf {
public:
	enum Mode {BB_READ, BB_WRITE};
	union highlow64 {
		uint64_t word64;
		uint32_t word32[2];
	};
	Bitbuf() {
		buffer.word64 = 0;
		fp = nullptr;
		bits_in_buffer = 0;
	}
	~Bitbuf() {
		close();
	}
	int open(const char *filename, Mode mode);
	int close(void);
	int read32(uint32_t &value);
	void unload(uint8_t length);
	int write(uint32_t code, uint8_t length);

private:
	Mode mode;
	highlow64 buffer;
	const int bufsize = 8 * sizeof(buffer);
	FILE *fp;
	int bits_in_buffer;
};

