#pragma once

#include "Egdb/distance.h"
#include "egdb/egdb_intl.h"
#include "egdb/wld_search.h"
#include "engine/board.h"
#include <time.h>
#include <vector>

namespace egdb_interface {

class dtw_search {

public:

	dtw_search(){clear();}
	dtw_search(wld_search &wld_, EGDB_DRIVER *dtw_) {clear(); wld = wld_; dtw = dtw_;}
	int get_nodes(void) {return(nodes);}
	int get_maxdepth(void) {return(maxdepth_reached);}
	int get_time(void) {return(elapsed);}
	int lookup_with_search(BOARD *board, int color, std::vector<move_distance> &dists);
	void set_search_timeout(int msec) { timeout = msec; }
	static const int dtw_draw = 1000;
	static const int dtw_unknown = -1;
	static const int dtw_aborted = -2;

private:
	int search(BOARD *board, int color, int depth, int maxdepth, int expected_wld_val);
	bool has_move(std::vector<move_distance> &dists, int movei)
	{
		for (size_t i = 0; i < dists.size(); ++i)
			if (dists[i].move == movei)
				return(true);
		return(false);
	}

	inline int wld_opposite(int value) {
		if (value == EGDB_WIN)
			return(EGDB_LOSS);
		if (value == EGDB_LOSS)
			return(EGDB_WIN);
		return(value);
	}

	void clear(void)
	{
		nodes = 0;
		int maxdepth_reached = 0;
		elapsed = 0;
		start_time = 0;
		timeout = 5000;
		dtw = nullptr;
	}

	int nodes;
	int maxdepth_reached;
	clock_t timeout;
	clock_t start_time;
	clock_t elapsed;
	wld_search wld;
	EGDB_DRIVER *dtw;
};

}	// namespace
