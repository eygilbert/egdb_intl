#pragma once
#include <algorithm>

class SLICE {
public:
	void reset(void);
	int advance(void);
	int getnbm(void) {return(nbm);}
	int getnbk(void) {return(nbk);}
	int getnwm(void) {return(nwm);}
	int getnwk(void) {return(nwk);}
	int getnpieces(void) {return(npieces);}

private:
	const int maxpiece = 5;
	int nbm;
	int nbk;
	int nwm;
	int nwk;
	int nb;
	int nw;
	int npieces;
};


