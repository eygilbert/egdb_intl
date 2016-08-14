#include "egdb/egdb_intl_ext.h"
#include "builddb/indexing.h"   // indextoposition_slice, getdatabasesize_slice
#include <tuple>                // make_tuple

namespace egdb_interface {

    bool operator==(position_iterator_slice const& lhs, position_iterator_slice const& rhs)
    {
        auto const tied = [](position_iterator_slice it) { return std::make_tuple(it.slice_, it.idx_); };
        return tied(lhs) == tied(rhs);
    }

    position_iterator_slice::reference position_iterator_slice::operator*()
    {
        indextoposition_slice(idx_, &pos_, slice_.nbm(), slice_.nbk(), slice_.nwm(), slice_.nwk());
        return pos_;
    }

    position_range_slice::position_range_slice(Slice const& s)
    :
        position_range_slice{
            position_iterator_slice{ s, 0 },
            position_iterator_slice{ s, getdatabasesize_slice(s.nbm(), s.nbk(), s.nwm(), s.nwk()) }
        }
    {}

}   // namespace egdb_interface
