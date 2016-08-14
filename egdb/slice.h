#pragma once
#include <iosfwd>   // ostream

namespace egdb_interface {

    /*
    // Iteration over all slices with 2 through N (exclusive) pieces
    // This corresponds to the half-open range [2, N), not to [2, N] (inclusive)

    for (Slice slice(2), last(N); slice != last; slice.advance()) {
        // any code
    }

    */

    class Slice
    {
        static const int min_ncolor = 1;
        static const int max_ncolor = 5;

        int npieces_;
        int nb_;
        int nw_;
        int nbm_;
        int nbk_;
        int nwm_;
        int nwk_;

        void assert_invariants() const;

    public:
        // first slice of <n> pieces
        explicit Slice(int n)
        :
            Slice{ (n + 1) / 2, n - (n + 1) / 2 }
        {}

        // first slice of <b, w> pieces
        Slice(int b, int w)
        :
            npieces_{ b + w },
            nb_{ b },
            nw_{ w },
            nbm_{ 0 },
            nbk_{ nb_ },
            nwm_{ 0 },
            nwk_{ nw_ }
        {
            assert_invariants();
        }

        // slice of exactly <bm, bk, wm, wk> pieces
        Slice(int bm, int bk, int wm, int wk)
        :
            npieces_{ bm + bk + wm + wk },
            nb_{ bm + bk },
            nw_{ wm + wk },
            nbm_{ bm },
            nbk_{ bk },
            nwm_{ wm },
            nwk_{ wk }
        {
            assert_invariants();
        }

        void advance();

        int npieces() const { return npieces_; }
        int nb()      const { return nb_; }
        int nw()      const { return nw_; }
        int nbm()     const { return nbm_; }
        int nbk()     const { return nbk_; }
        int nwm()     const { return nwm_; }
        int nwk()     const { return nwk_; }
    };

    const int Slice::min_ncolor;
    const int Slice::max_ncolor;

    bool operator==(Slice const& lhs, Slice const& rhs);

    inline
    bool operator!=(Slice const& lhs, Slice const& rhs)
    {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& str, Slice const& s);

}   // namespace egdb_interface
