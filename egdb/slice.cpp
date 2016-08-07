#include "egdb/slice.h"
#include <algorithm>

void SLICE::reset(void)
{
	npieces = 2;
	nb = 1;
	nw = 1;
	nbm = 0;
	nbk = 1;
	nwm = 0;
	nwk = 1;
}


int SLICE::advance(void)
{
	if (nwk > 0) {
		--nwk;
		++nwm;
	}
	else if (nbk > 0) {
		--nbk;
		++nbm;
		if (nb == nw) {
			nwm = nbm;
			nwk = nbk;
		}
		else {
			nwk = nw;
			nwm = 0;
		}
	}
	else if (nb < (std::min)(maxpiece, npieces - 1)) {
		++nb;
		--nw;
		nbm = 0;
		nbk = nb;
		nwm = 0;
		nwk = npieces - nb;
	}
	else {
		++npieces;
		nb = (npieces + 1) / 2;
		nw = npieces - nb;
		nbm = 0;
		nbk = nb;
		nwm = 0;
		nwk = nw;
	}
	return(npieces);
}

