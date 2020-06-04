#include "bitbuf.h"


int Bitbuf::open(const char *filename, Mode open_mode)
{
	mode = open_mode;
	buffer.word64 = 0;
	bits_in_buffer = 0;
	if (mode == BB_WRITE) {
		fp = fopen(filename, "wb");
		if (fp == nullptr) {
			printf("Cannot open %s for writing\n", filename);
			return(1);
		}
	}
	if (mode == BB_READ) {
		fp = fopen(filename, "rb");
		if (fp == nullptr) {
			printf("Cannot open %s for reading\n", filename);
			return(1);
		}
	}

	return(0);
}


int Bitbuf::write(uint32_t value, uint8_t length)
{
	size_t status;

	assert(length <= 32);
	assert((value & ~((1 << length) - 1)) == 0);
	if (bufsize - bits_in_buffer < length) {

		/* Write the upper 32 bits of the buffer to the file. */
		status = fwrite(&buffer.word32[1], 4, 1, fp);
		if (status != 1)
			return(1);

		buffer.word64 <<= 32;
		bits_in_buffer -= 32;
	}

	/* Maintain the buffer left-justified. */
	buffer.word64 |= ((uint64_t)value << (bufsize - bits_in_buffer - length));
	bits_in_buffer += length;
	return(0);
}


int Bitbuf::read32(uint32_t &value)
{
	size_t status;

	if (bits_in_buffer < 32) {

		/* Read 32 bits from the file. The buffer is right-justified. */
		buffer.word32[1] = buffer.word32[0];
		status = fread(&buffer.word32[0], 4, 1, fp);
		if (status != 1)
			return(1);

		bits_in_buffer += 32;
	}

	value = (uint32_t)(buffer.word64 >> (bits_in_buffer - 32));
	return(0);
}


void Bitbuf::unload(uint8_t length)
{
	bits_in_buffer -= length;
}


int Bitbuf::close(void)
{
	size_t status;

	if (fp == nullptr)
		return(0);

	status = 1;
	if (mode == BB_WRITE) {
		if (bits_in_buffer)
			write(0, 32);

		/* Write the last words to the file and close it. */
		if (bits_in_buffer >= 32) {
			status = fwrite(&buffer.word32[1], 4, 1, fp);
			status = fwrite(&buffer.word32[0], 4, 1, fp);
		}
		else
			status = fwrite(&buffer.word32[1], 4, 1, fp);
	}

	fclose(fp);
	fp = nullptr;
	return(status != 1);
}


