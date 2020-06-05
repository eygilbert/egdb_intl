#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <list>

struct Huffcode {
	uint16_t value;
	uint8_t codelength;
	uint32_t huffcode;
};

union Words32_64 {
	uint64_t word64;
	uint32_t word32[2];
};

class Huffman {
public:
	typedef uint32_t Frequency;
	typedef uint16_t Value;
	struct Huffnode {
		Value value;
		Frequency freq;
		int depth;
		Huffnode *left;
		Huffnode *right;
		Huffnode *parent;
	};

	Huffman() {tree = nullptr;}
	~Huffman() {delete_tree(tree);}

	struct Codetable {
		Value value;
		uint32_t huffcode;		/* right justified, only the lowest codelength bits are significant. */
		uint8_t codelength;
		Frequency freq;
	};

	struct Lengthtable {
		uint8_t codelength;
		uint16_t codetable_index;
		uint32_t huffcode;
	};

	struct Compress {
		uint32_t code;
		uint8_t length;
	};

	void read_input(char *filename, bool iswide, std::vector<Huffman::Value> &input);
	void process_input(std::vector<Huffman::Value> &input);
	void delete_tree(Huffnode *node);
	void print_codetable(void);
	void compress(char *base_filename, bool iswide, bool verify);
	void decompress(char *base_filename, std::vector<uint16_t> &uncompressed);
	uint32_t write_compressed_block(std::vector<Huffman::Value> &input, uint32_t input_index, 
									std::vector<uint32_t> &compr_block);
	void get_codelengths(std::vector<Huffcode> &codelengths);
	uint32_t get_compressed_length(void);	/* Return length in bytes of compressed input. */
private:
	void build_compr_map(void);
	int write_header_file(char *base_filename, uint32_t input_length);
	int read_header_file(char *base_filename, uint32_t &input_length);
	void write_compressed_file(char *base_filename, std::vector<Huffman::Value> &input);
	void read_compressed_file(char *base_filename, uint32_t uncompressed_length, std::vector<uint16_t> &filebuf);
	void sort_codetable_by_length();
	void generate_codes(void);
	void build_length_table(void);
	void get_codelengths(void);
	void get_codelengths(Huffnode *tree, int depth);
	void get_frequencies(std::vector<Value> &input);
	void build_tree(void);
	std::list<Huffnode *>::iterator get_lowest_freq(std::list<Huffnode *> &list);
	uint32_t left_justify32(uint32_t code, int length) {return(code << (32 - length));}
	std::string binary_format(uint32_t code, int length, int formatted_width);

	const int MAX_VALUES = 65536;
	const int INTERNAL_NODE = MAX_VALUES - 1;
	Huffnode *tree;
	std::vector<Frequency> frequencies;		/* Table of input value counts. */
	std::vector<Codetable> codetable;		/* sorted by decreasing code lengths, and by increasing symbol values. */
	std::vector<Lengthtable> lengthtable;	/* Table of lowest code of each length, used in decompression. */
	std::vector<Compress> compr_map;		/* Index by input value, returns huffcode and its length. */
};

int compare_huffcode_entries(const void *p1, const void *p2);
void generate_codes(std::vector<Huffcode> &codetable);
void build_length_table(std::vector<Huffcode> &codelengths, std::vector<Huffman::Lengthtable> &lengthtable);
void uncompress_block(std::vector<uint32_t> &block, uint32_t ncodes, std::vector<Huffman::Value> &uncompressed,
					std::vector<Huffman::Lengthtable> &lengthtable, std::vector<Huffcode> &codetable);

