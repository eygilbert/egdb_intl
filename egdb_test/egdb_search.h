#pragma once

#define MAXREPDEPTH 64

typedef struct {
	int nbm;
	int nbk;
	int nwm;
	int nwk;
	int nb;
	int nw;
	int npieces;
	int npieces_1_side;
	uint64 blackmen;
	uint64 blackkings;
	uint64 whitemen;
	uint64 whitekings;
} MATERIAL;

class EGDB_INFO {

public:
	EGDB_INFO(void) { maxnodes = 1000000; clear(); }
	void clear(void) {
		handle = 0;
		dbpieces = 0;
		dbpieces_1side = 0;
		db9_kings = 0;
		db8_kings_1side = 0;
		egdb_excludes_some_nonside_caps = 0;
		maxdepth_reached = 0;
	}
	bool requires_nonside_capture_test(BOARD *p);
	bool is_lookup_possible_pieces(MATERIAL *mat);
	bool is_lookup_possible_with_nonside_caps(MATERIAL *mat);
	bool is_lookup_possible(BOARD *board, int color, MATERIAL *mat);
	int lookup_with_search(BOARD *p, int color, int maxdepth, bool force_root_search);
	int get_maxdepth() { return(maxdepth_reached); }
	void reset_maxdepth() { maxdepth_reached = 0; }

	EGDB_DRIVER *handle;
	int dbpieces;
	int dbpieces_1side;
	int db9_kings;
	int db8_kings_1side;
	int egdb_excludes_some_nonside_caps;

private:
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
	BOARD rep_check_positions[MAXREPDEPTH + 1];

};

bool is_repetition(BOARD *history, BOARD *p, int depth);
