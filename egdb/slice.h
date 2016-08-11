#pragma once
#include <algorithm>	// min
#include <cassert>		// assert
#include <iostream>		// ostream
#include <iterator>		// orward_iterator_tag, iterator_category, iterator_traits
#include <type_traits>	// is_same
#include <tuple>		// make_tuple

class Slice
{
	static const int max_ncolor = 5;
	static const int min_ncolor = 1;

	int npieces_;
	int nb_;
	int nw_;
	int nbm_;
	int nbk_;
	int nwm_;
	int nwk_;

	void assert_invariants() const
	{
		assert(nbm() + nbk() == nb());
		assert(nwm() + nwk() == nw());
		assert(nb() + nw_ == npieces());
		assert(min_ncolor <= nb()); assert(nb() <= max_ncolor);
		assert(min_ncolor <= nw()); assert(nw() <= max_ncolor);
	}

public:
	// first slice of <n> pieces
	Slice(int n = 2)
	:
		Slice{(n + 1) / 2, n - (n + 1)/ 2}
	{
		assert(npieces() == n);
	}

	// first slice of <b, w> pieces
	Slice(int b, int w)
	:
		npieces_{b + w},
		nb_{b},
		nw_{w},
		nbm_{0},
		nbk_{nb_},
		nwm_{0},
		nwk_{nw_}
	{
		assert_invariants();
	}

	// slice of exactly <bm, bk, wm, wk> pieces
	Slice(int bm, int bk, int wm, int wk)
	:
		npieces_{bm + bk + wm + wk},
		nb_{bm + bk},
		nw_{wm + wk},
		nbm_{bm},
		nbk_{bk},
		nwm_{wm},
		nwk_{wk}
	{
		assert_invariants();
	}

	Slice&       operator*()       { return *this; }
	Slice const& operator*() const { return *this; }

	Slice& operator++()    {                   increment(); return *this; }
	Slice  operator++(int) { auto tmp = *this; increment(); return tmp;   }

	int npieces() const { return npieces_; }
	int nb()      const { return nb_; }
	int nw()      const { return nw_; }
	int nbm()     const { return nbm_; }
	int nbk()     const { return nbk_; }
	int nwm()     const { return nwm_; }
	int nwk()     const { return nwk_; }

private:
	void increment()
	{
		if (nwk() > 0) {
			++nwm_;
			--nwk_;
			assert_invariants();
			return;
		}

		if (nbk() > 0) {
			++nbm_;
			--nbk_;
			if (nb() == nw()) {
				nwm_ = nbm();
				nwk_ = nbk();
			}
			else {
				nwk_ = nw();
				nwm_ = 0;
			}
			assert_invariants();
			return;
		}

		if (nb() < (std::min)(npieces() - min_ncolor, max_ncolor)) {
			*this = Slice(nb() + 1, nw() - 1);
			return;
		}

		*this = Slice(npieces_ + 1);
	}
};

const int Slice::max_ncolor;
const int Slice::min_ncolor;

inline
bool operator==(Slice const& lhs, Slice const& rhs)
{
	auto const as_tuple = [](Slice const& s) {
		return std::make_tuple(s.nbm(), s.nbk(), s.nwm(), s.nwk());
	};
	return as_tuple(lhs) == as_tuple(rhs);
}

inline
bool operator!=(Slice const& lhs, Slice const& rhs)
{
	return !(lhs == rhs);
}

inline
std::ostream& operator<<(std::ostream& str, Slice const& s)
{
	return str << "db" << s.npieces() << "-" << s.nbm() << s.nbk() << s.nwm() << s.nwk();
}
