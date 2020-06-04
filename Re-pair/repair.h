/* 
 *  Copyright (c) 2011 Shirou Maruyama
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *   1. Redistributions of source code must retain the above Copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above Copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the authors nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 */
#ifndef REPAIR_H
#define REPAIR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <string>
#include <vector>

#define UNDEFINED_CODE (CODE)32767
#define UNDEFINED_POS UINT_MAX

class Repair {
public:
	const int max_symbol_length = 65535;

	typedef uint16_t Input_t;
	typedef uint16_t CODE;

	struct Sequence {
		CODE code;
		uint32_t next;
		uint32_t prev;
	};

	struct Pair {
		CODE left;
		CODE right;
		uint32_t freq;
		uint32_t f_pos;	/* Head of pair list in the sequence array. */
		uint32_t b_pos;	/* Tail of pair list in the squence array. */
		Pair *ht_next;	/* Used for singly-linked list from hashtable. */
		Pair *pq_next;	/* Used for doubly-linked list from priority queue. */
		Pair *pq_prev;
	};

	struct Rule {
		CODE left;
		CODE right;
	};

	/* Info on symbols for assigned pairs and symbols in the input stream. */
	struct Symbols {
		uint16_t highest_defined_sym;
		uint32_t num_syms;
		std::vector<Rule> symtable;
	};

	~Repair(void) {close();};
	void compress(std::vector<Input_t> &input, int maxcodes);
	int verify_compressed(std::vector<Input_t> &original, std::vector<uint16_t> &compressed);
	void close(void);
	void uncompress_file(const char *filename, std::vector<Input_t> &uncompressed);
	void write_compressed(const char *filename, bool write_header);
	void get_compressed(std::vector<uint16_t> &data, std::vector<Rule> &syms);
	int get_symbol_length(int sym, std::vector<Repair::Rule> &symbols);

private:
	Pair *find_pair(CODE left, CODE right);
	void resize_hashtable(void);
	inline uint32_t hashtable_index(CODE left, CODE right);
	void pq_insert(Pair *target, uint32_t p_num);
	void pq_delete(Pair *target, uint32_t p_num);
	void increment_pair_freq(Pair *target);
	void decrement_pair_freq(Pair *target);
	Pair *new_pair(CODE left, CODE right, uint32_t f_pos);
	void delete_pair(Pair *target);
	void pq_delete(uint32_t p_num);
	void link_pairs(void);
	void repair_init(std::vector<Input_t> &input);
	Pair *get_maxfreq_pair(void);
	uint32_t pq_left_pos(uint32_t pos);
	uint32_t pq_right_pos(uint32_t pos);
	void seq_unlink_pos(uint32_t target_pos);
	void seq_update_block(CODE new_code, uint32_t target_pos);
	uint32_t replace_pairs(Pair *max_pair, CODE new_code);
	void create_symtable(void);
	CODE new_symbol(Pair *max_pair);
	void print_seq(char *description);
	uint32_t get_sequence_length(void);
	void assign_input_rules(void);
	void print_symbols(void);
	void expand(uint16_t code, std::vector<Rule> &syms, std::vector<Input_t> &uncompressed);
	uint32_t high_freq_list_length(void);
	void get_freqs(std::vector<uint32_t> &frequencies);
	void print_initial_stats(void);

	uint32_t text_length;
	Sequence *seq;
	uint32_t num_pairs;
	uint32_t ht_size;					/* hashtable size should be a power of 2. */
	Pair **hashtable;					/* pair hashtable. */
	std::vector<Pair *> priority_queue;	/* Table of pair lists sorted by frequency. */
	uint32_t pq_index;					/* index to the most recent max frequencies found. */
	Symbols symbols;					/* Assigned pairs and input symbols. */
};

void get_symbol_lengths(std::vector<Repair::Rule> &symbols, std::vector<uint16_t> &lengths);
double calc_entropy(std::vector<uint32_t> &frequencies);

#endif
