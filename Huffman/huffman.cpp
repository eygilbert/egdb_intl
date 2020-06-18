#include <algorithm>
#include <list>
#include <assert.h>
#include "huffman.h"
#include "bitbuf.h"


static int compare_codetable_entries(const void *p1, const void *p2);


void Huffman::get_frequencies(std::vector<Value> &input)
{
	Value maxvalue = 0;
	frequencies.resize(MAX_VALUES, 0);

	for (auto i = input.begin(); i != input.end(); ++i) {
		assert(*i < MAX_VALUES);
		++frequencies[*i];
		maxvalue = std::max(maxvalue, *i);
	}
	frequencies.resize(maxvalue + 1);
}

static void dbg(){}

std::list<Huffman::Huffnode *>::iterator Huffman::get_lowest_freq(std::list<Huffnode *> &list)
{
	std::list<Huffnode *>::iterator lowest, i;
	
	assert(list.size() > 0);
	lowest = list.begin();
	i = lowest;
	for (++i; i != list.end(); ++i) {
		if ((*i)->freq < (*lowest)->freq || ((*i)->freq == (*lowest)->freq && (*i)->depth < (*lowest)->depth)) {
			if ((*i)->freq == (*lowest)->freq && (*i)->depth < (*lowest)->depth)
				dbg();
			lowest = i;
		}

	}
	return(lowest);
}


void Huffman::build_tree(void)
{
	Huffnode *node = nullptr;
	std::list<Huffnode *> nodes;

	for (auto i = frequencies.begin(); i != frequencies.end(); ++i) {
		if (*i == 0)
			continue;
		node = (Huffnode *)malloc(sizeof(Huffnode));
		assert(node != nullptr);
		node->value = (Value)(i - frequencies.begin());
		node->depth = 0;
		node->freq = *i;
		node->left = nullptr;
		node->right = nullptr;
		node->parent = nullptr;
		nodes.push_back(node);
	}

	while (nodes.size() > 1) {
		std::list<Huffnode *>::iterator low1, low2;
		Huffnode *n1, *n2;

		low1 = get_lowest_freq(nodes);
		n1 = *low1;
		nodes.erase(low1);
		low2 = get_lowest_freq(nodes);
		n2 = *low2;
		nodes.erase(low2);
		node = (Huffnode *)malloc(sizeof(Huffnode));
		assert(node != nullptr);
		node->value = INTERNAL_NODE;
		node->depth = 1 + std::max(n1->depth, n2->depth);
		node->freq = n1->freq + n2->freq;
		node->left = n1;
		n1->parent = node;
		node->right = n2;
		n2->parent = node;
		node->parent = nullptr;
		nodes.push_back(node);
	}
	tree = node;
}


void Huffman::get_codelengths(Huffnode *node, int depth)
{
	Codetable entry;

	if (!node)
		return;

	if (node->left != nullptr)
		get_codelengths(node->left, depth + 1);

	if (node->right != nullptr)
		get_codelengths(node->right, depth + 1);
	
	if (node->left == nullptr || node->right == nullptr || node->value != INTERNAL_NODE) {
		assert(node->value != INTERNAL_NODE);
		entry.codelength = std::max(depth, 1);	/* Handle degenerate case of 1 code. */
		entry.huffcode = 0;
		entry.value = node->value;
		entry.freq = frequencies[node->value];
		codetable.push_back(entry);
	}
}


void Huffman::get_codelengths(void)
{
	get_codelengths(tree, 0);
}


/*
 * Sort the table by code lengths and symbol values.
 * Code lengths in decreasing order.
 * Symbol values in increasing order (when code lengths are equal).
 */
void Huffman::sort_codetable_by_length(void)
{
	qsort(&codetable[0], codetable.size(), sizeof(Codetable), compare_codetable_entries);
}


void Huffman::delete_tree(Huffnode *node)
{
	if (node == nullptr)
		return;
	if (node->left != nullptr)
		delete_tree(node->left);
	if (node->right != nullptr)
		delete_tree(node->right);
	free(node);
}


std::string Huffman::binary_format(uint32_t code, int length, int formatted_width)
{
	int i;
	uint32_t mask;
	std::string result;

	code = left_justify32(code, length);
	for (i = 0; i < formatted_width; ++i) {
		mask = 1 << (i + 32 - formatted_width);
		if (mask & code)
			result = "1" + result;
		else
			result = "0" + result;
	}
	return(result);
}


void Huffman::print_codetable(void)
{
	for (size_t i = 0; i < codetable.size(); ++i)
		printf("sym %4d,  codelength %2d,  code 0x%08x (%s),  freq %10u\n", 
				codetable[i].value,
				codetable[i].codelength, 
				left_justify32(codetable[i].huffcode, codetable[i].codelength), 
				binary_format(codetable[i].huffcode, codetable[i].codelength, codetable[0].codelength).c_str(),
				codetable[i].freq);
}


/*
 * Assign Huffman code values to the huffcode field of codetable[].
 * codetable[] has been sorted by decreasing code lengths, and by increasing symbol values
 * when the lengths are equal.
 */
void Huffman::generate_codes(void)
{
	int last_length, length, code;

	code = 0;
	last_length = codetable[0].codelength;
	for (size_t i = 0; i < codetable.size(); ++i, ++code) {
		length = codetable[i].codelength;
		code >>= (last_length - length);
		last_length = length;
		codetable[i].huffcode = code;
	}
}


/*
 * Make a table that has the lowest huffman code of each length.
 * Used to decode huffman codes during decompression.
 */
void Huffman::build_length_table(void)
{
	int i;
	Lengthtable entry;

	/* codetable has the longest symbols first, so walk the table in reverse order. */
	entry.codelength = codetable[codetable.size() - 1].codelength;
	for (i = (int)codetable.size() - 2; i >= 0; --i) {
		if (codetable[i].codelength > entry.codelength) {
			entry.huffcode = left_justify32(codetable[i + 1].huffcode, entry.codelength);
			entry.codetable_index = i + 1;
			lengthtable.push_back(entry);
			entry.codelength = codetable[i].codelength;
		}
	}
	entry.huffcode = left_justify32(codetable[0].huffcode, entry.codelength);
	entry.codetable_index = 0;
	lengthtable.push_back(entry);
}


int Huffman::write_header_file(char *base_filename, uint32_t input_length)
{
	size_t i, status;
	std::string filename;
	FILE *fp;

	filename = std::string(base_filename) + ".hdr";
	fp = fopen(filename.c_str(), "wb");
	if (!fp) {
		printf("Cannot open %s for writing\n", filename.c_str());
		return(1);
	}

	/* Write the total length of the uncompressed file. */
	status = fwrite(&input_length, sizeof(input_length), 1, fp);
	if (status != 1) {
		printf("Error writing to header file.\n");
		return(1);
	}

	/* Write the number of distinct symbols (codes) in the file. */
	uint32_t nsyms = (uint32_t)codetable.size();
	status = fwrite(&nsyms, sizeof(nsyms), 1, fp);
	if (status != 1) {
		printf("Error writing to header file.\n");
		return(1);
	}

	/* Write each symbol value and its length. */
	for (i = 0; i < codetable.size(); ++i) {
		status = fwrite(&codetable[i].value, sizeof(codetable[i].value), 1, fp);
		if (status != 1) {
			printf("Error writing to header file.\n");
			return(1);
		}
		status = fwrite(&codetable[i].codelength, sizeof(codetable[i].codelength), 1, fp);
		if (status != 1) {
			printf("Error writing to header file.\n");
			return(1);
		}
	}

	fclose(fp);
	return(0);
}


int Huffman::read_header_file(char *base_filename, uint32_t &uncompressed_length)
{
	size_t i, status;
	std::string filename;
	uint32_t nsyms;
	FILE *fp;

	filename = std::string(base_filename) + ".hdr";
	fp = fopen(filename.c_str(), "rb");
	if (!fp) {
		printf("Cannot open %s for reading\n", filename.c_str());
		return(1);
	}

	/* Read the total length of the uncompressed file. */
	status = fread(&uncompressed_length, sizeof(uncompressed_length), 1, fp);
	if (status != 1) {
		printf("Error reading header file.\n");
		return(1);
	}

	/* Read the number of distinct symbols (codes) in the file. */
	status = fread(&nsyms, sizeof(nsyms), 1, fp);
	if (status != 1) {
		printf("Error reading header file.\n");
		return(1);
	}

	/* Read each symbol value and its length. */
	codetable.clear();
	for (i = 0; i < nsyms; ++i) {
		Codetable entry = {0, 0, 0, 0};

		status = fread(&entry.value, sizeof(entry.value), 1, fp);
		if (status != 1) {
			printf("Error reading header file.\n");
			return(1);
		}
		status = fread(&entry.codelength, sizeof(entry.codelength), 1, fp);
		if (status != 1) {
			printf("Error reading header file.\n");
			return(1);
		}
		codetable.push_back(entry);
	}

	fclose(fp);
	return(0);
}


/* Make a table of huffman codes and lengths that can be indexed by the input values. */
void Huffman::build_compr_map(void)
{
	compr_map.resize(frequencies.size(), {0, 0});

	for (auto i = codetable.begin(); i != codetable.end(); ++i) {
		compr_map[i->value].code = i->huffcode;
		compr_map[i->value].length = i->codelength;
	}
}


void Huffman::write_compressed_file(char *base_filename, std::vector<Huffman::Value> &input)
{
	std::string filename;
	Bitbuf bitbuf;

	filename = std::string(base_filename) + ".cpr";
	bitbuf.open(filename.c_str(), Bitbuf::BB_WRITE);
	
	/* Write the compressed output file. */
	for (auto i = input.begin(); i != input.end(); ++i) {
		Value value;

		value = *i;
		bitbuf.write(compr_map[value].code, compr_map[value].length);
	}
	bitbuf.close();
}


uint32_t Huffman::write_compressed_block(std::vector<Huffman::Value> &input, uint32_t start_index, 
									std::vector<uint32_t> &compr_block)
{
	int bits_written, block_index;
	uint32_t i;
	Words32_64 codebuf;
	uint64_t temp;
	Value value;
	int bits_in_buffer, length;

	bits_written = 0;
	block_index = 0;
	bits_in_buffer = 0;
	codebuf.word64 = 0;
	for (i = start_index; i < input.size(); ++i) {
		value = input[i];
		length = compr_map[value].length;
		if (length + bits_in_buffer > 32 && block_index >= (int)compr_block.size() - 1)
			break;

		temp = compr_map[value].code;
		temp <<= (64 - bits_in_buffer - length);
		codebuf.word64 |= temp;
		bits_in_buffer += length;
		if (bits_in_buffer >= 32) {
			compr_block[block_index++] = codebuf.word32[1];
			codebuf.word64 <<= 32;
			bits_in_buffer -= 32;
			if (block_index == compr_block.size())
				break;
		}
	}

	if (bits_in_buffer > 0) {
		assert(block_index < (int)compr_block.size());
		compr_block[block_index++] = codebuf.word32[1];
	}

	/* Return the index into the next uncompressed input. */
	return(i);
}


void Huffman::read_compressed_file(char *base_filename, uint32_t uncompressed_length, std::vector<uint16_t> &uncompressed)
{
	int i, symidx;
	size_t status;
	uint32_t nread, value;
	std::string filename;
	Bitbuf bitbuf;

	filename = std::string(base_filename) + ".cpr";
	status = bitbuf.open(filename.c_str(), Bitbuf::BB_READ);
	
	nread = 0;
	while (nread < uncompressed_length) {
		bitbuf.read32(value);
		for (i = 0; value < lengthtable[i].huffcode; ++i)
			;

		symidx = (value - lengthtable[i].huffcode) >> (32 - lengthtable[i].codelength);
		symidx += lengthtable[i].codetable_index;
		uncompressed.push_back(codetable[symidx].value);
		bitbuf.unload(lengthtable[i].codelength);
		++nread;
	}
	bitbuf.close();
}


void Huffman::compress(char *base_filename, bool iswide, bool verify)
{
	std::vector<Huffman::Value> input, uncompressed;

	read_input(base_filename, iswide, input);
	get_frequencies(input);
	build_tree();
	get_codelengths();
	sort_codetable_by_length();
	generate_codes();
	build_compr_map();
	write_header_file(base_filename, (uint32_t)input.size());
	write_compressed_file(base_filename, input);
	if (verify) {
		decompress(base_filename, uncompressed);
		if (input.size() != uncompressed.size()) {
			printf("Input size %zd, compressed size %zd\n", input.size(), uncompressed.size());
		}
		for (size_t i = 0; i < input.size(); ++i) {
			if (input[i] != uncompressed[i]) {
				printf("Values[%zd] don't match; input %u, uncompressed %u\n", 
						i, input[i], uncompressed[i]);
			}
		}
	}
}


void Huffman::process_input(std::vector<Huffman::Value> &input)
{
	get_frequencies(input);
	build_tree();
	get_codelengths();
	sort_codetable_by_length();
	generate_codes();
	build_compr_map();
}


void Huffman::decompress(char *base_filename, std::vector<uint16_t> &uncompressed)
{
	uint32_t uncompressed_length;

	read_header_file(base_filename, uncompressed_length);
	sort_codetable_by_length();
	generate_codes();
	print_codetable();
	build_length_table();
	read_compressed_file(base_filename, uncompressed_length, uncompressed);
}


void Huffman::read_input(char *filename, bool iswide, std::vector<Value> &input)
{
	FILE *fp;

	fp = fopen(filename, "rb");
	if (!fp) {
		printf("Cannot open file %s\n", filename);
		exit(1);
	}
	if (iswide) {
		uint16_t inchar;
		while (fread(&inchar, sizeof(inchar), 1, fp) == 1)
			input.push_back(inchar);
	}
	else {
		int inchar;
		while ((inchar = fgetc(fp)) != EOF)
			input.push_back(inchar);
	}

	fclose(fp);
}


void Huffman::get_codelengths(std::vector<Huffcode> &codelengths)
{
	codelengths.clear();

	for (size_t i = 0; i < codetable.size(); ++i) {
		Huffcode entry;

		entry.value = codetable[i].value;
		entry.codelength = codetable[i].codelength;
		codelengths.push_back(entry);
	}

}


/* Return length in bytes of compressed input. */
uint32_t Huffman::get_compressed_length(void)
{
	int inputval;
	int64_t bitlength;

	bitlength = 0;
	for (size_t i = 0; i < codetable.size(); ++i) {
		inputval = codetable[i].value;
		bitlength += frequencies[inputval] * codetable[i].codelength;
	}

	return((uint32_t)((bitlength + 7) / 8));
}


static int compare_codetable_entries(const void *p1, const void *p2)
{
	Huffman::Codetable *s1 = (Huffman::Codetable *)p1;
	Huffman::Codetable *s2 = (Huffman::Codetable *)p2;

	if (s1->codelength < s2->codelength)
		return(1);
	if (s1->codelength == s2->codelength)
		return(s1->value - s2->value);
	return(-1);
}


int compare_huffcode_entries(const void *p1, const void *p2)
{
	Huffcode *s1 = (Huffcode *)p1;
	Huffcode *s2 = (Huffcode *)p2;

	if (s1->codelength < s2->codelength)
		return(1);
	if (s1->codelength == s2->codelength)
		return(s1->value - s2->value);
	return(-1);
}


/*
 * Assign Huffman code values to the huffcode field of codetable[].
 * codetable[] has been sorted by decreasing code lengths, and by increasing symbol values
 * when the lengths are equal.
 */
void generate_codes(std::vector<Huffcode> &codetable)
{
	int last_length, length, code;

	code = 0;
	last_length = codetable[0].codelength;
	for (size_t i = 0; i < codetable.size(); ++i, ++code) {
		length = codetable[i].codelength;
		code >>= (last_length - length);
		last_length = length;
		codetable[i].huffcode = code;
	}
}


inline uint32_t left_justify32(uint32_t code, int length)
{
	return(code << (32 - length));
}


/*
 * Make a table that has the lowest huffman code of each length.
 * Used to decode huffman codes during decompression.
 */
void build_length_table(std::vector<Huffcode> &codelengths, std::vector<Huffman::Lengthtable> &lengthtable)
{
	int i;
	Huffman::Lengthtable entry;

	/* codetable has the longest symbols first, so walk the table in reverse order. */
	entry.codelength = codelengths[codelengths.size() - 1].codelength;
	for (i = (int)codelengths.size() - 2; i >= 0; --i) {
		if (codelengths[i].codelength > entry.codelength) {
			entry.huffcode = left_justify32(codelengths[i + 1].huffcode, entry.codelength);
			entry.codetable_index = i + 1;
			lengthtable.push_back(entry);
			entry.codelength = codelengths[i].codelength;
		}
	}
	entry.huffcode = left_justify32(codelengths[0].huffcode, entry.codelength);
	entry.codetable_index = 0;
	lengthtable.push_back(entry);
}


void uncompress_block(std::vector<uint32_t> &block, uint32_t ncodes, std::vector<Huffman::Value> &uncompressed,
					std::vector<Huffman::Lengthtable> &lengthtable, std::vector<Huffcode> &codetable)
{
	int i, symidx;
	uint32_t codes_read;
	Words32_64 codebuf;
	uint64_t temp;
	int bits_in_codebuf, block_index;

	uncompressed.clear();
	block_index = 0;
	codebuf.word32[1] = block[block_index++];
	codebuf.word32[0] = block[block_index++];
	bits_in_codebuf = 64;
	codes_read = 0;
	while (codes_read < ncodes) {
		if (bits_in_codebuf < 32 && block_index < (int)block.size()) {
			temp = block[block_index++];
			temp <<= (32 - bits_in_codebuf);
			codebuf.word64 |= temp;
			bits_in_codebuf += 32;
		}

		for (i = 0; codebuf.word32[1] < lengthtable[i].huffcode; ++i)
			;

		symidx = (codebuf.word32[1] - lengthtable[i].huffcode) >> (32 - lengthtable[i].codelength);
		symidx += lengthtable[i].codetable_index;
		uncompressed.push_back(codetable[symidx].value);
		++codes_read;
		codebuf.word64 <<= lengthtable[i].codelength;
		bits_in_codebuf -= lengthtable[i].codelength;
	}
}

