#pragma once
#include <algorithm>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <exception>
#include <assert.h>


class Packed_array {
	union bytes64 {
		uint64_t word64;
		uint8_t word8[8];
	};

public:
	Packed_array() {
		init(1);
	}
	Packed_array(int nbits) {
		init(nbits);
	}

	~Packed_array()
	{
		clear();
	}

	void clear(void)
	{
		size_ = 0;
		if (capacity_ > 0)
			memset(array_, 0, rawsize_);
	}

	void shrink_to_fit(void)
	{	
		if (capacity_ > size_) {
			std::bad_alloc alloc_error;

			capacity_ = size_;
			rawsize_ = bytes_needed(size_);
			array_ = (uint8_t *)realloc(array_, rawsize_);
			if (rawsize_ > 0 && array_ == nullptr) {
				throw alloc_error;
				return;
			}
		}
	}

	size_t size(void)
	{
		return(size_);
	}

	int nbits(void)
	{
		return(nbits_);
	}

	uint32_t at(size_t index)
	{
		int bits_copied, bitoffset;
		size_t start;
		bytes64 temp;

		bitoffset = (index * (uint64_t)nbits_) & 7;
		start = (index * (uint64_t)nbits_) / 8;
		temp.word8[0] = array_[start];
		bits_copied = 8 - bitoffset;
		for (int i = 1; bits_copied < nbits_; ++i, bits_copied += 8)
			temp.word8[i] = array_[start + i];

		temp.word64 >>= bitoffset;
		temp.word64 &= mask_;
		return((uint32_t)temp.word64);
	}

	uint32_t operator [](size_t index)
	{
		return(at(index));
	}

	void push_back(uint32_t value)
	{
		bytes64 temp;
		
		assert(value <= mask_);
		temp.word64 = value;
		if (size_ >= capacity_)
			reserve((size_t)((std::max)((size_t)16, 2 * capacity_)));

		/* Store value. */
		int bitoffset = (size_ * (uint64_t)nbits_) & 7;
		size_t start = (size_ * (uint64_t)nbits_) / 8;
		temp.word64 <<= bitoffset;
		array_[start] |= temp.word8[0];
		int bits_copied = 8 - bitoffset;
		for (int i = 1; bits_copied < nbits_; ++i, bits_copied += 8)
			array_[start + i] = temp.word8[i];

		++size_;
	}

	void set_at(size_t index, uint32_t value)
	{
		bytes64 temp;
		size_t nbytes;

		int bitoffset = (index * (uint64_t)nbits_) & 7;
		size_t start = (index * (uint64_t)nbits_) / 8;
		nbytes = (nbits_ + bitoffset + 7) / 8;
		memcpy(&temp, array_ + start, nbytes);
		temp.word64 &= ~(mask_ << bitoffset);
		temp.word64 |= value << bitoffset;
		memcpy(array_ + start, &temp, nbytes);
	}

	void reserve(size_t newsize)
	{
		if (newsize > capacity_) {
			std::bad_alloc alloc_error;

			size_t newrawsize;
			newrawsize = bytes_needed(newsize);
			array_ = (uint8_t *)realloc(array_, newrawsize);
			if (array_ == nullptr) {
				throw alloc_error;
				return;
			}

			/* Clear the new bytes. */
			memset(&array_[rawsize_], 0, newrawsize - rawsize_);
			capacity_ = newsize;
			rawsize_ = newrawsize;
		}
	}

	void resize(size_t newsize)
	{
		if (newsize > capacity_)
			reserve(newsize);

		size_ = newsize;
	}

	size_t capacity(void)
	{
		return(capacity_);
	}

	void raw_set(size_t index, uint8_t value)
	{
		if (index >= raw_size())
			throw 1;

		array_[index] = value;
	}


	size_t raw_size(void) 
	{
		return(bytes_needed(size_));
	}

	uint8_t raw_at(size_t index)
	{
		if (index >= raw_size())
			throw 1;

		return(array_[index]);
	}

	uint8_t *raw_buf(void)
	{
		return(array_);
	}

private:
	void init(int nbits) {
		nbits_ = nbits;
		mask_ = ((uint64_t)1 << nbits) - 1;
		size_ = 0;
		capacity_ = 0;
		rawsize_ = 0;
		array_ = nullptr;
	}

	size_t bytes_needed(size_t size)
	{
		int64_t bits_needed = (uint64_t)size * nbits_;
		size_t bytes;

		bytes = (size_t)(bits_needed / 8);
		if (bits_needed & 7)
			++bytes;
		return(bytes);
	}

	int nbits_;			/* num bits in each stored value. */
	uint64_t mask_;		/* bitmask for lowest nbits_. */
	size_t size_;		/* num values stored. */
	size_t rawsize_;	/* bytes allocated in array_. */
	size_t capacity_;	/* max values that there is space allocated for. */
	uint8_t *array_;
};

