#pragma once
#include "engine/board.h"
#include "engine/bool.h"

namespace egdb_interface {

struct MOVELIST {
    int count;					/* The number of moves in this list. */
    BOARD board[MAXMOVES];		/* How the board looks after each move. */
};

struct CAPTURE_INFO {
    char capture_count;
    BOARD path[MAXPIECES_JUMPED];
};

struct GAMEPOS {
    int is_fen;
    int color;
    BOARD board;
	int compare(const GAMEPOS &p1) const {
		if (color == p1.color)
			return(memcmp(&board, &p1.board, sizeof(BOARD)));
		else
			return(color - p1.color);
	}
};


int build_movelist(const BOARD *board, int color, MOVELIST *movelist);
int build_movelist_path(const BOARD *board, int color, MOVELIST *movelist, CAPTURE_INFO *path);
int build_nonjump_list(const BOARD *board, MOVELIST *movelist, int color);
int build_man_nonjump_list(const BOARD *board, MOVELIST *movelist, int color);
int build_king_nonjump_list(const BOARD *board, MOVELIST *movelist, int color);
int canjump(BOARD *board, int color);

template <bool save_capture_info>
int build_jump_list(const BOARD *board, MOVELIST *movelist, int color, CAPTURE_INFO cap[MAXMOVES] = NULL);

int has_a_king_move(BOARD *p, int color);
BITBOARD black_safe_moveto(BOARD *board);
BITBOARD white_safe_moveto(BOARD *board);
void get_jumped_square(int jumpi, int movei, int color, CAPTURE_INFO cap[], BITBOARD *jumped);
int has_jumped_square(int movei, int color, CAPTURE_INFO cap[], BITBOARD jumped);
int has_landed_square(int movei, int color, CAPTURE_INFO *cap, int square);
void get_fromto_bitboards(int movei, int color, const BOARD *board, MOVELIST *movelist,
    CAPTURE_INFO cap[], BITBOARD *from, BITBOARD *to);

void get_fromto(BITBOARD fromboard, BITBOARD toboard, int *fromsq, int *tosq);
void get_fromto(BOARD *fromboard, BOARD *toboard, int color, int *fromsq, int *tosq);
int match_jumpto_square(CAPTURE_INFO *cap, int jumpindex, int color, int jumpto_square);
bool is_conversion_move(const BOARD *from, const BOARD *to);


inline void get_fromto_bitboards(BOARD *fromboard, BOARD *toboard, int color, BITBOARD *frombb, BITBOARD *tobb)
{
	*frombb = fromboard->pieces[color] & ~toboard->pieces[color];
	*tobb = ~fromboard->pieces[color] & toboard->pieces[color];
}


inline void get_fromto_bitnums(BITBOARD fromboard, BITBOARD toboard, int *frombitnum, int *tobitnum)
{
    BITBOARD from, to;

    from = fromboard & ~toboard;
    *frombitnum = LSB64(from);
    to = ~fromboard & toboard;
    *tobitnum = LSB64(to);
}


inline void get_fromto_bitnums(BOARD *fromboard, BOARD *toboard, int color, int *frombitnum, int *tobitnum)
{
    BITBOARD from, to;

    from = fromboard->pieces[color] & ~toboard->pieces[color];
    *frombitnum = LSB64(from);
    to = ~fromboard->pieces[color] & toboard->pieces[color];
    *tobitnum = LSB64(to);
}


inline bool is_promotion(BOARD *fromboard, BOARD *toboard, int color)
{
    BITBOARD from, to;

    from = fromboard->pieces[color] & ~toboard->pieces[color];
    to = ~fromboard->pieces[color] & toboard->pieces[color];
	return((from & ~fromboard->king) && (to & toboard->king));
}


}   // namespace egdb_interface
