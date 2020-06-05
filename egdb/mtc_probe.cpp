#pragma once

#include "Egdb/dtw_search.h"
#include "egdb/egdb_intl.h"
#include "egdb/wld_search.h"
#include "Engine/bitcount.h"
#include "engine/board.h"
#include "Engine/Move_api.h"
#include <algorithm>
#include <time.h>
#include <vector>

namespace egdb_interface {

/* Return non-zero on success, with egdb score in *score, and distances in dists[].
 * Return zero if there are no conversion distances to return.
 */
int mtc_probe(wld_search &wld, EGDB_DRIVER *mtc, BOARD *board, int color, MOVELIST *movelist, int *wld_value, std::vector<move_distance> &dists)
{
	int i;
	int npieces;
	int egdb_value, parent_distance, offset;

	dists.clear();

	/* We need both an open mtc driver and egdb driver. */
	if (mtc == nullptr || wld.handle == nullptr)
		return(0);

	/* Must be few enough pieces for db lookups. */
	npieces = bitcount64(board->black | board->white);
	if (npieces > wld.dbpieces)
		return(0);

	/* If this position is not in the mtc db, then we have to do a heuristic search. */
	parent_distance = egdb_lookup(mtc, (EGDB_POSITION *)board, color, 0);
	if (parent_distance == MTC_LESS_THAN_THRESHOLD)
		return(0);

	egdb_value = wld.lookup_with_search(board, color, false);
	if (!(egdb_value == EGDB_WIN || egdb_value == EGDB_LOSS))
		return(0);

	*wld_value = egdb_value;
	if (egdb_value == EGDB_WIN) {
		for (i = 0; i < movelist->count; ++i) {
			egdb_value = wld.lookup_with_search(movelist->board + i, OTHER_COLOR(color), false);
			if (egdb_value == EGDB_LOSS) {
				move_distance dist;
				dist.distance = egdb_lookup(mtc, (EGDB_POSITION *)(movelist->board + i), OTHER_COLOR(color), 0);

				/* If any wins are short then let the search find the move. */
				if (dist.distance == MTC_LESS_THAN_THRESHOLD)
					return(0);

				dist.move = i;
				dists.push_back(dist);
			}
			else if (egdb_value != EGDB_DRAW && egdb_value != EGDB_WIN)
				return(0);
		}

		if (dists.size() == 0)
			return(0);

		/* Sort moves to pick the best one. */
		std::sort(dists.begin(), dists.end(), [](const move_distance &l, const move_distance &r) {return(l.distance < r.distance);});
	}
	else if (egdb_value == EGDB_LOSS) {
		for (i = 0; i < movelist->count; ++i) {
			move_distance dist;
			if (is_conversion_move(board, movelist->board + i))
				continue;

			dist.distance = egdb_lookup(mtc, (EGDB_POSITION *)(movelist->board + i), OTHER_COLOR(color), 0);
			if (dist.distance == MTC_LESS_THAN_THRESHOLD)
				continue;

			dist.move = i;
			dists.push_back(dist);
		}

		if (dists.size() == 0)
			return(0);

		std::sort(dists.begin(), dists.end(), [](const move_distance &l, const move_distance &r) {return(l.distance > r.distance);});
	}

	/* Fix the even/odd problem. */
	if (parent_distance == dists[0].distance) {
		offset = -1;
		for (size_t i = 0; i < dists.size(); ++i)
			dists[i].distance += offset;
	}

	return(1);
}

}	// namespace
