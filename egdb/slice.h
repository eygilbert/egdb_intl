#pragma once
#include <builddb/indexing.h>
#include <egdb/egdb_intl.h>
#include <algorithm>    // min
#include <cassert>      // assert
#include <cstdint>      // int64_t
#include <cstddef>      // ptrdiff_t
#include <iostream>     // ostream
#include <iterator>     // distance, forward_iterator_tag, iterator_category
#include <tuple>        // make_tuple

/*
// Iteration over all slices with 2 through N (exclusive) pieces
// This corresponds to the half-open range [2, N), not to [2, N] (inclusive)

for (Slice slice(2), last(N); slice != last; slice.advance()) {
	// any code
}

*/

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
	explicit Slice(int n)
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

	void advance()
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

		*this = Slice(npieces() + 1);
	}

	int npieces() const { return npieces_; }
	int nb()      const { return nb_; }
	int nw()      const { return nw_; }
	int nbm()     const { return nbm_; }
	int nbk()     const { return nbk_; }
	int nwm()     const { return nwm_; }
	int nwk()     const { return nwk_; }
};

const int Slice::max_ncolor;
const int Slice::min_ncolor;

inline
bool operator==(Slice const& lhs, Slice const& rhs)
{
	// convert a slice to a std::tuple of the various piece counts
	auto const as_tuple = [](Slice const& s) {
		return std::make_tuple(s.nbm(), s.nbk(), s.nwm(), s.nwk());
	};

	// use std::tuple::operator== for Slice comparisons
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

/*
// Iteration over all slices with 2 through N (exclusive) pieces
// This corresponds to the half-open range [2, N), not to [2, N] (inclusive)

std::for_each(slice_iterator(2), slice_iterator(N), [&](Slice const& slice) {
	// any code, but no 'break' and 'continue' replaced with 'return'
});

for (slice_iterator first(2), last(N); first != last; ++first) {
	// any code, but replace slice with *first
}

*/

class slice_iterator
{
	Slice slice_;
public:
	// typedefs for std::iterator_traits
    using iterator_category = std::forward_iterator_tag;
    using value_type        = Slice;
    using difference_type   = std::ptrdiff_t;
    using pointer           = value_type*;
    using reference	        = value_type&;

	slice_iterator() = default;

	explicit slice_iterator(int npieces)
	:
		slice_(npieces)
	{}

	slice_iterator(int nb, int nw)
	:
		slice_(nb, nw)
	{}

	slice_iterator(int nbm, int nbk, int nwm, int nwk)
	:
		slice_(nbm, nbk, nwm, nwk)
	{}

	slice_iterator& operator++()    {                   slice_.advance(); return *this; }
	slice_iterator  operator++(int) { auto tmp = *this; slice_.advance(); return tmp;   }

	friend
	bool operator==(slice_iterator const& lhs, slice_iterator const& rhs)
	{
		return lhs.slice_ == rhs.slice_;
	}

	friend
	bool operator!=(slice_iterator const& lhs, slice_iterator const& rhs)
	{
		return !(lhs == rhs);
	}

	reference operator*() { return slice_; }
};

/*
// Iteration over all slices with 2 through N (exclusive) pieces
// This corresponds to the half-open range [2, N), not to [2, N] (inclusive)

for (Slice const& slice : slice_range(2, N)) {
	// any code
});

*/

// semantically equivalent to boost::counting_range<slice_iterator>
class slice_range
{
    slice_iterator first_;
    slice_iterator last_;
public:
    slice_range() = default;

    slice_range(slice_iterator b, slice_iterator e)
    :
	    first_{b},
	    last_{e}
    {}

    slice_range(int b, int e)
    :
	    slice_range{
            slice_iterator{b}, 
            slice_iterator{e}
        }
    {}

    slice_iterator begin()       { return first_; }
    slice_iterator begin() const { return first_; }
    slice_iterator end()         { return last_;  }
    slice_iterator end()   const { return last_;  }
    std::ptrdiff_t size()  const { return std::distance(begin(), end()); }
};

class position_iterator_slice
{
    egdb_interface::EGDB_POSITION pos_;
    Slice const& slice_;
    int64_t idx_;
public:
    // typedefs for std::iterator_traits
    using iterator_category = std::forward_iterator_tag;    // TODO strengthen to std::random_access_iterator_tag
    using value_type        = egdb_interface::EGDB_POSITION;
    using difference_type   = std::ptrdiff_t;
    using pointer           = value_type*;
    using reference         = value_type&;

    position_iterator_slice() = default;

    position_iterator_slice(Slice const& s, int64_t i)
    :
        slice_{s},
        idx_{i}
    {}

    position_iterator_slice& operator++()    {                   ++idx_; return *this; }
    position_iterator_slice  operator++(int) { auto tmp = *this; ++idx_; return tmp;   }

    // TODO: operators +=, -=, +, -

    friend
    bool operator==(position_iterator_slice const& lhs, position_iterator_slice const& rhs)
    {
        auto const tied = [](position_iterator_slice it) { return std::make_tuple(it.slice_, it.idx_); };
        return tied(lhs) == tied(rhs);
    }

    friend
    bool operator!=(position_iterator_slice const& lhs, position_iterator_slice const& rhs)
    {
        return !(lhs == rhs);
    }
    
    reference operator*() 
    { 
        egdb_interface::indextoposition_slice(idx_, &pos_, slice_.nbm(), slice_.nbk(), slice_.nwm(), slice_.nwk());
        return pos_;  
    }
};

class position_range_slice
{
    position_iterator_slice first_;
    position_iterator_slice last_;
public:
    position_range_slice() = default;

    position_range_slice(position_iterator_slice b, position_iterator_slice e)
    :
        first_{ b },
        last_{ e }
    {}

    position_range_slice(Slice const& s)
    :
        position_range_slice{
            position_iterator_slice{s, 0},
            position_iterator_slice{s, egdb_interface::getdatabasesize_slice(s.nbm(), s.nbk(), s.nwm(), s.nwk()) }
        }
    {}

    position_iterator_slice begin()       { return first_; }
    position_iterator_slice begin() const { return first_; }
    position_iterator_slice end()         { return last_; }
    position_iterator_slice end()   const { return last_; }
             std::ptrdiff_t size()  const { return std::distance(begin(), end()); }
};
