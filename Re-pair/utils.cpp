#include <stdint.h>
#include <assert.h>
#include <vector>
#include "repair.h"


double calc_entropy(std::vector<uint32_t> &frequencies) 
{
    double entropy, prob;
    int64_t totfreq;
    size_t i;

    for (totfreq = i = 0; i < frequencies.size(); i++)
		totfreq += frequencies[i];

    for (entropy = 0.0, i = 0; i < frequencies.size(); i++)
		if (frequencies[i]) {
			prob = (double)frequencies[i] / totfreq;
			entropy += -prob * log2(prob);
		}

    return entropy;
}


static void get_symbol_length(int sym, std::vector<Repair::Rule> &symbols, std::vector<uint16_t> &lengths)
{
	if (symbols[sym].left == sym)
		lengths[sym] = 1;
	else {
		if (lengths[symbols[sym].left] == 0)
			get_symbol_length(symbols[sym].left, symbols, lengths);
		if (lengths[symbols[sym].right] == 0)
			get_symbol_length(symbols[sym].right, symbols, lengths);

		lengths[sym] = lengths[symbols[sym].left] + lengths[symbols[sym].right];
	}
}


void get_symbol_lengths(std::vector<Repair::Rule> &symbols, std::vector<uint16_t> &lengths)
{
	lengths.clear();
	lengths.resize(symbols.size(), 0);
	for (int i = 0; i < (int)symbols.size(); ++i)
		get_symbol_length(i, symbols, lengths);
}

