#pragma once
#include <algorithm>	// min
#include <cassert>		// assert
#include <cstddef>		// ptrdiff_t
#include <iostream>		// ostream
#include <iterator>		// orward_iterator_tag, iterator_category, iterator_traits
#include <type_traits>	// is_same
#include <tuple>		// make_tuple
#include <utility>		// forward

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

namespace detail {

// semantically equivalent to boost::counting_iterator<Slice>
template<bool IsConst>
class slice_iterator
{
	Slice slice_;
public:
	// typedefs for std::iterator_traits
	using iterator_category = std::forward_iterator_tag;
	using value_type 		= Slice;
	using difference_type 	= std::ptrdiff_t;
	using pointer			= typename std::conditional<IsConst, Slice const*, Slice*>::type;
	using reference			= typename std::conditional<IsConst, Slice const&, Slice&>::type;

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

	// allow conversions of iterator to const_iterator
	template<bool>
	friend
	class slice_iterator;

	template<bool Dummy = IsConst, typename std::enable_if<Dummy>::type* = nullptr>
	explicit slice_iterator(slice_iterator<!Dummy> it)
	:
		slice_{it.slice_}
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

	template<bool Dummy = IsConst, typename std::enable_if<!Dummy>::type* = nullptr>
	reference operator*() { return slice_; }

	template<bool Dummy = IsConst, typename std::enable_if<Dummy>::type* = nullptr>
	reference operator*() const { return slice_; }
};

}	// namespace detail

using slice_iterator = detail::slice_iterator<false>;
using const_slice_iterator = detail::slice_iterator<true>;

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
		slice_range{slice_iterator{b}, slice_iterator{e}}
	{}

	      slice_iterator begin()       { return first_; }
	const_slice_iterator begin() const { return const_slice_iterator{first_}; }
	      slice_iterator end()         { return last_; }
	const_slice_iterator end()   const { return const_slice_iterator{last_}; }
	std::ptrdiff_t size()  const { return std::distance(begin(), end()); }
};

