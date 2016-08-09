#pragma once
#include "egdb/slice.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <tuple>

namespace dctl {

class slice
{
	static const int max_npieces = 5;
	static const int min_nb = 1;
	static const int min_nw = 1;

	int npieces;
	int nb;
	int nw;
	int nbm;
	int nbk;
	int nwm;
	int nwk;

	bool assert_invariants()
	{
		assert(nbm + nbk == nb);
		assert(nwm + nwk == nw);
		assert(nb + nw == npieces);
	}

public:
	// first slice of <n> pieces
	slice(int n = 2)
	:
		slice{(n + 1) / 2, n - (n + 1)/ 2}
	{}

	// first slice of <b, w> pieces
	slice(int b, int w)
	:
		npieces{b + w},
		nb{b},
		nw{w},
		nbm{0},
		nbk{nb},
		nwm{0},
		nwk{nw}
	{
		assert_invariants();
	}

	// slice of exactly <bm, bk, wm, wk> pieces
	slice(int bm, int bk, int wm, int wk)
	:
		npieces{bm + bk + wm + wk},
		nb{bm + bk},
		nw{wm + wk},
		nbm{bm},
		nbk{bk},
		nwm{wm},
		nwk{wk}
	{
		assert_invariants();
	}

	void reset(int n = 2) { *this = slice(n); }

	void next()
	{
		if (nwk > 0) {
			++nwm;
			--nwk;
			return;
		}

		if (nbk > 0) {
			++nbm;
			--nbk;
			if (nb == nw) {
				nwm = nbm;
				nwk = nbk;
			}
			else {
				nwk = nw;
				nwm = 0;
			}
			return;
		}

		if (nb < (std::min)(npieces - min_nw, max_npieces)) {
			*this = slice(nb + 1, nw - 1);
			return;
		}

		*this = slice(npieces + 1);
	}

	int advance() { next(); return npieces; }

	slice& operator++() { next(); return *this; }
	slice operator++(int) { auto tmp = *this; next(); return tmp; }

	slice const& operator*() const { return *this; }
	slice& operator*() { return *this; }

	friend bool operator==(slice const& lhs, slice const& rhs)
	{
		auto const tied = [](slice const& s) { return std::tie(s.nbm, s.nbk, s.nwm, s.nwk); };
		return tied(lhs) == tied(rhs);
	}

	friend bool operator!=(slice const& lhs, slice const& rhs)
	{
		return !(lhs == rhs);
	}

	friend std::ostream& operator<<(std::ostream& str, slice const& s)
	{
		str << s.nbm << s.nbk << s.nwm << s.nwk;
		return str;
	}

	friend bool operator==(SLICE& lhs, slice const& rhs)
	{
		auto const tiedL = [](SLICE& s) { return std::make_tuple(s.getnbm(), s.getnbk(), s.getnwm(), s.getnwk()); };
		auto const tiedR = [](slice const& s) { return std::tie(s.nbm, s.nbk, s.nwm, s.nwk); };
		return tiedL(lhs) == tiedR(rhs);
	}

};

const int slice::max_npieces;
const int slice::min_nb;
const int slice::min_nw;

}	// namespace dctl

inline
std::ostream& operator<<(std::ostream& str, SLICE & s)
{
	str << s.getnbm() << s.getnbk() << s.getnwm() << s.getnwk();
	return str;
}

