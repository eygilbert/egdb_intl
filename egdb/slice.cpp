#include "egdb/slice.h" // Slice
#include <algorithm>    // min
#include <cassert>      // assert
#include <iostream>     // ostream

namespace egdb_interface {

const int Slice::min_ncolor;
const int Slice::max_ncolor;

    void Slice::assert_invariants() const
    {
        assert(nbm() + nbk() == nb());
        assert(nwm() + nwk() == nw());
        assert(nb() + nw_ == npieces());
        assert(min_ncolor <= nb()); assert(nb() <= max_ncolor);
        assert(min_ncolor <= nw()); assert(nw() <= max_ncolor);
    }

    void Slice::increment()
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

    bool operator==(Slice const& lhs, Slice const& rhs)
    {
        return
        		lhs.nbm() == rhs.nbm() &&
				lhs.nbk() == rhs.nbk() &&
				lhs.nwm() == rhs.nwm() &&
				lhs.nwk() == rhs.nwk()
		;
    }

    std::ostream& operator<<(std::ostream& str, Slice const& s)
    {
        return str << "db" << s.npieces() << "-" << s.nbm() << s.nbk() << s.nwm() << s.nwk();
    }

}   // namespace egdb_interface
