#include "egdb/dtw_search.h"
#include "egdb/egdb_intl.h"
#include "engine/board.h"
#include "engine/move_api.h"
#include <algorithm>

namespace egdb_interface {

int dtw_search::search(BOARD *board, int color, int depth, int maxdepth, int expected_wld_val)
{
	int dtw_val, successor_wld_val;
	bool capture;
	int i, movecount, bestvalue, bestmove;
	MOVELIST movelist;

	++nodes;
	if (depth > maxdepth_reached)
		maxdepth_reached = depth;

	if (!canjump(board, color)) {
		capture = false;
		dtw_val = egdb_lookup(dtw, (EGDB_POSITION *)board, color, 0);
		if (dtw_val >= 0) {
			/* Got a legitimate depth value.
			 * Translate to plies. Wins are odd, losses even.
			 */
			if (expected_wld_val == EGDB_WIN)
				return(2 * dtw_val + 1);
			if (expected_wld_val == EGDB_LOSS)
				return(2 * dtw_val);
		}
	}
	else
		capture = true;
	
	if (depth >= maxdepth)
		return(dtw_unknown);

	/* Have to do a search. */
	if (capture)
		movecount = build_jump_list<false>(board, &movelist, color);
	else
		movecount = build_nonjump_list(board, &movelist, color);

	/* If no moves then its a loss (DTW 0). */
	if (movecount == 0)
		return(0);

	/* look up all successors. */
	bestmove = 0;
	if (expected_wld_val == EGDB_WIN)
		bestvalue = 511;
	else
		bestvalue = 0;
	for (i = 0; i < movecount; ++i) {
		successor_wld_val = wld.lookup_with_search(movelist.board + i, OTHER_COLOR(color), false);
		if (timeout && (clock() - start_time) >= timeout)
			return(dtw_aborted);
		if (successor_wld_val == EGDB_UNKNOWN)
			return(dtw_unknown);
		if (expected_wld_val == EGDB_WIN)
			if (successor_wld_val != EGDB_LOSS)
				continue;
		if (expected_wld_val == EGDB_LOSS) {
			assert(successor_wld_val == EGDB_WIN);
		}
		dtw_val = search(movelist.board + i, OTHER_COLOR(color), depth + 1, maxdepth, wld_opposite(expected_wld_val));
		if (dtw_val == dtw_unknown)
			return(dtw_unknown);

		if (expected_wld_val == EGDB_WIN) {
			if (dtw_val < bestvalue) {
				bestvalue = dtw_val;
				bestmove = i;
			}
		}
		else {
			if (dtw_val > bestvalue) {
				bestvalue = dtw_val;
				bestmove = i;
			}
		}
	}
	return(1 + bestvalue);
}


int dtw_search::lookup_with_search(BOARD *board, int color, std::vector<move_distance> &dists)
{
	int wld_val, dtw_val;
	int i, maxdepth, movecount;
	bool go_deeper;
	MOVELIST movelist;
	const int Maxdepth = 32;

	dists.clear();
	nodes = 0;
	maxdepth_reached = 0;
	start_time = clock();
	elapsed = 0;
	if (timeout)
		wld.set_search_timeout(timeout);

	wld_val = wld.lookup_with_search(board, color, false);
	switch (wld_val) {
	case EGDB_DRAW:
		return(dtw_draw);
	case EGDB_UNKNOWN:
		elapsed = clock() - start_time;
		return(dtw_unknown);
	}

	if (canjump(board, color))
		movecount = build_jump_list<false>(board, &movelist, color);
	else
		movecount = build_nonjump_list(board, &movelist, color);
	
	if (movecount == 0)
		return(0);

	/* look up all successors. */
	go_deeper = false;
	for (maxdepth = 0; maxdepth < Maxdepth; ++maxdepth) {
		for (i = 0; i < movecount; ++i) {
			int successor_wld_val;

			if (has_move(dists, i))
				continue;

			successor_wld_val = wld.lookup_with_search(movelist.board + i, OTHER_COLOR(color), false);
			if (successor_wld_val == EGDB_UNKNOWN) {
				elapsed = clock() - start_time;
				return(dtw_unknown);
			}
			if (wld_val == EGDB_WIN)
				if (successor_wld_val != EGDB_LOSS)
					continue;
			if (wld_val == EGDB_LOSS) {
				assert(successor_wld_val == EGDB_WIN);
			}
			if (timeout && (clock() - start_time) >= timeout) {
				elapsed = clock() - start_time;
				return(dtw_unknown);
			}
			dtw_val = search(movelist.board + i, OTHER_COLOR(color), 0, maxdepth, wld_opposite(wld_val));
			if (dtw_val == dtw_aborted) {
				elapsed = clock() - start_time;
				return(dtw_unknown);
			}
			if (dtw_val == dtw_unknown) {
				go_deeper = true;
				continue;
			}
			if (dtw_val == dtw_draw)
				continue;

			dists.push_back({dtw_val + 1, i});
		}
		if (!go_deeper)
			break;
	}
	if (wld_val == EGDB_WIN)
		std::sort(dists.begin(), dists.end(), [](const move_distance &l, const move_distance &r) {return(l.distance < r.distance);});
	else
		std::sort(dists.begin(), dists.end(), [](const move_distance &l, const move_distance &r) {return(l.distance > r.distance);});

	elapsed = clock() - start_time;
	return(dists[0].distance);
}


}	// namespace
