#pragma once
#include "egdb/egdb_intl.h"
#include "engine/board.h"
#include <csetjmp>
#include <stdint.h>

namespace egdb_interface {

struct egdb_info {
	EGDB_DRIVER *handle;
	int max_pieces;
	EGDB_TYPE dbtype;
	egdb_info() {clear();}
	void clear(void) {
		max_pieces = 0;
		handle = nullptr;
	}
};

typedef struct {
	short int og;
	short int eg;
	short int val;
	short int og_mir;
	short int eg_mir;
	short int val_mir;
	int index;
	int index_mir;
} LRSC;

typedef struct {
	int nbm;
	int nbk;
	int nwm;
	int nwk;
	int nb;
	int nw;
	int npieces;
	int npieces_1_side;
	uint64_t blackmen;
	uint64_t blackkings;
	uint64_t whitemen;
	uint64_t whitekings;
	int phase;
	LRSC patscores[4];
	LRSC lrbal;
	LRSC tempo;
	LRSC men;
	LRSC first_king;
	LRSC multi_kings;
	int value;
} MATERIAL;

class wld_search {

public:
	wld_search(void) { clear(); }
	wld_search(EGDB_DRIVER *handle_)
	{
		clear();
		handle = handle_;
		type = egdb_get_type(handle);
		if (type == EGDB_WLD_TUN_V2)
			egdb_excludes_some_nonside_caps = true;
		else
			egdb_excludes_some_nonside_caps = false;
		egdb_get_pieces(handle, &dbpieces, &dbpieces_1side);
	}
	void clear(void) {
		handle = 0;
		dbpieces = 0;
		dbpieces_1side = 5;
		egdb_excludes_some_nonside_caps = true;		/* Set false for db version 1. */
		maxdepth_reached = 0;
		timeout = 5000;			/* 0 means no limit. */
		maxnodes = 0;			/* 0 means no limit. */
	}
	bool requires_nonside_capture_test(BOARD *p);
	bool is_lookup_possible_pieces(MATERIAL *mat);
	bool is_lookup_possible_with_nonside_caps(MATERIAL *mat);
	bool is_lookup_possible(BOARD *board, int color, MATERIAL *mat);
	int lookup_with_search(BOARD *p, int color, bool force_root_search);
	int get_maxdepth() { return(maxdepth_reached); }
	void set_maxnodes(int nodes) { maxnodes = nodes; }
	void reset_maxdepth() { maxdepth_reached = 0; }
	void set_search_timeout(int msec) { timeout = msec; }

	EGDB_DRIVER *handle;
	EGDB_TYPE type;
	int dbpieces;
	int dbpieces_1side;
	bool egdb_excludes_some_nonside_caps;

private:
	static const int maxrepdepth = 64;
	int timeout;
	int t0;
	int maxnodes;
	int nodes;
	int maxdepth_reached;
	int lookup_with_rep_check(BOARD *p, int color, int depth, int maxdepth, int alpha, int beta, bool force_root_search);
	int negate(int value);
	bool is_greater_or_equal(int left, int right);
	bool is_greater(int left, int right);
	int bestvalue_improve(int value, int bestvalue);
	void log_tree(int value, int depth, int color);
	jmp_buf env;
	BOARD rep_check_positions[maxrepdepth + 1];

};

bool is_repetition(BOARD *history, BOARD *p, int depth);

}	// namespace
