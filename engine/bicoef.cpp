#include "engine/bicoef.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

unsigned int bicoef[MAXSQUARE_BICOEF + 1][MAXPIECES_BICOEF + 1];


static unsigned int choose(int n, int k)
{
	int64_t result = 1;
	int i;

	i = k;
	while (i) {
		result *= (n - i + 1);
		i--;
	}

	i = k;
	while (i) {
		result /= i;
		i--;
	}

	return((unsigned int)(result));
}


/*
 * Initialize binomial coefficients
 * bicoef[n][k] is supposed to be the number of ways you can choose k from n.
 */
void initbicoef(void)
{
	int i, j;

	for (i = 0; i <= MAXSQUARE_BICOEF; i++) {
		for (j = 1; j <= (std::min)(MAXPIECES_BICOEF, i); j++) {

			// choose j from i:
			bicoef[i][j] = choose(i, j);
		}
		// define choosing 0 for simplicity 
		bicoef[i][0] = 1;
	}

	// choosing n from 0: bicoef = 0
	for (i = 1; i <= MAXPIECES_BICOEF; i++)
		bicoef[0][i] = 0;
}


