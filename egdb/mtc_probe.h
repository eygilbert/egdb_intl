#pragma once

#include "Egdb/distance.h"
#include "Egdb/dtw_search.h"
#include "egdb/egdb_intl.h"
#include "egdb/wld_search.h"
#include "engine/board.h"
#include "Engine/Move_api.h"
#include <vector>

namespace egdb_interface {

int mtc_probe(wld_search &wld, EGDB_DRIVER *mtc, BOARD *board, int color, MOVELIST *movelist, int *wld_value, std::vector<move_distance> &dists);

}	// namespace
