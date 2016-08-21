#pragma once
#include "egdb/egdb_intl.h"     // EGDB_POSITION
#include "egdb/slice.h"         // Slice
#include <cstdint>              // int64_t
#include <cstddef>              // ptrdiff_t
#include <iterator>             // distance, forward_iterator_tag, random_access_iterator_tag

namespace egdb_interface {

class EGDB_DRIVER_V2
{
	EGDB_DRIVER* handle_ = nullptr;

public:
	EGDB_DRIVER_V2() = default;

	EGDB_DRIVER_V2(char const *options, int cache_mb, char const *directory, void (*msg_fn)(char const *msg))
	:
		handle_{egdb_open(options, cache_mb, directory, msg_fn)}
	{}

	~EGDB_DRIVER_V2()
	{
		safe_close();
	}

	int reopen(char const *options, int cache_mb, char const *directory, void (*msg_fn)(char const *msg))
	{
		safe_close();
		handle_ = egdb_open(options, cache_mb, directory, msg_fn);
	}

	bool is_open() const
	{
		return handle_ != nullptr;
	}

	int lookup(EGDB_POSITION *position, int color, int cl)
	{
		return egdb_lookup(handle_, position, color, cl);
	}

	void reset_stats()
	{
		return egdb_reset_stats(handle_);
	}

	EGDB_STATS *get_stats()
	{
		return egdb_get_stats(handle_);
	}

	int verify(void (*msg_fn)(char const *msg), int *abort, EGDB_VERIFY_MSGS *msgs)
	{
		return egdb_verify(handle_, msg_fn, abort, msgs);
	}

	int get_pieces(int *max_pieces, int *max_pieces_1side, int *max_9pc_kings, int *max_8pc_kings_1side)
	{
		return egdb_get_pieces(handle_, max_pieces, max_pieces_1side, max_9pc_kings, max_8pc_kings_1side);
	}

private:
	void safe_close()
	{
		if (is_open() && !egdb_close(handle_)) {
			std::exit(1);
		}
	}
};

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

    // for combatibility with Boost.Iterator (the operator++ overloads make Slice model Incrementable)
    inline Slice& operator++(Slice& s)      {               s.increment(); return s;   }
    inline Slice  operator++(Slice& s, int) { auto tmp = s; ++s;           return tmp; }

    // alternative: using slice_iterator = boost::counting_iterator<Slice, std::forward_iterator_tag>
    class slice_iterator
    {
        Slice slice_;
    public:
        // for combatibility with std::iterator_traits
        using iterator_category = std::forward_iterator_tag;
        using value_type        = Slice;
        using difference_type   = std::ptrdiff_t;
        using pointer           = value_type*;
        using reference         = value_type&;

        slice_iterator() = default;

        explicit slice_iterator(int npieces)
        :
            slice_{ npieces }
        {}

        slice_iterator(int nb, int nw)
        :
            slice_{ nb, nw }
        {}

        slice_iterator(int nbm, int nbk, int nwm, int nwk)
        :
            slice_{ nbm, nbk, nwm, nwk }
        {}

        slice_iterator& operator++()    {                   ++slice_; return *this; }
        slice_iterator  operator++(int) { auto tmp = *this; ++*this;  return tmp;   }

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

    // alternative: boost::counting_range<slice_iterator>
    class slice_range
    {
        slice_iterator first_, last_;
    public:
        // for combatibility with Boost.Range
        using       iterator = slice_iterator;
        using const_iterator = slice_iterator;

        slice_range() = default;

        slice_range(slice_iterator b, slice_iterator e)
        :
            first_{ b }, last_{ e }
        {}

        slice_range(int b, int e)
        :
            slice_range{ slice_iterator{ b }, slice_iterator{ e } }
        {}

        slice_iterator begin()       { return first_; }
        slice_iterator begin() const { return first_; }
        slice_iterator end()         { return last_; }
        slice_iterator end()   const { return last_; }
        std::ptrdiff_t size()  const { return std::distance(begin(), end()); }
    };

    // alternative: using position_iterator_slice = boost::counting_iterator<int64_t, std::random_access_iterator_tag>
    class position_iterator_slice
    {
        EGDB_POSITION pos_;
        Slice const& slice_;
        int64_t idx_;
    public:
        // for combatibility with std::iterator_traits
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = EGDB_POSITION;
        using difference_type   = std::ptrdiff_t;
        using pointer           = value_type*;
        using reference         = value_type&;

        position_iterator_slice() = default;

        position_iterator_slice(Slice const& s, int64_t i)
        :
            slice_{ s }, idx_{ i }
        {}

        position_iterator_slice& operator++()    {                   ++idx_;  return *this; }
        position_iterator_slice  operator++(int) { auto tmp = *this; ++*this; return tmp;   }

        position_iterator_slice& operator--()    {                   --idx_;  return *this; }
        position_iterator_slice  operator--(int) { auto tmp = *this; --*this; return tmp;   }

        position_iterator_slice& operator+=(size_t n) { idx_ += n; return *this; }
        position_iterator_slice& operator-=(size_t n) { idx_ -= n; return *this; }

        friend
        position_iterator_slice operator+(position_iterator_slice it, size_t n)
        {
            auto nrv{ it }; nrv += n; return nrv;
        }

        friend
        position_iterator_slice operator-(position_iterator_slice it, size_t n)
        {
            auto nrv{ it }; nrv -= n; return nrv;
        }

        friend
        std::ptrdiff_t operator-(position_iterator_slice lhs, position_iterator_slice rhs)
        {
            return lhs.idx_ - rhs.idx_;
        }

        friend
        bool operator==(position_iterator_slice const& lhs, position_iterator_slice const& rhs);

        friend
        bool operator!=(position_iterator_slice const& lhs, position_iterator_slice const& rhs)
        {
            return !(lhs == rhs);
        }

        reference operator*();
    };

    // alternative: using position_range_slice = boost::counting_range<position_iterator_slice>
    class position_range_slice
    {
        position_iterator_slice first_, last_;
    public:
        // for combatibility with Boost.Range
        using       iterator = position_iterator_slice;
        using const_iterator = position_iterator_slice;

        position_range_slice() = default;

        position_range_slice(position_iterator_slice b, position_iterator_slice e)
        :
            first_{ b }, last_{ e }
        {}

        position_range_slice(Slice const& s);

        position_iterator_slice begin()       { return first_; }
        position_iterator_slice begin() const { return first_; }
        position_iterator_slice end()         { return last_; }
        position_iterator_slice end()   const { return last_; }
                 std::ptrdiff_t size()  const { return std::distance(begin(), end()); }
    };

}   // namespace egdb_interface
