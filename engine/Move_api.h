#pragma once

#define SINGLE_CORNER_DIAG (S5 | S10 | S14 | S19 | S23 | S28 | S32 | S37 | S41 | S46)


typedef struct {
	int count;					/* The number of moves in this list. */
	BOARD board[MAXMOVES];		/* How the board looks after each move. */
} MOVELIST;

typedef struct {
	char capture_count;
	BOARD path[MAXPIECES_JUMPED];
} CAPTURE_INFO;

typedef struct {
	int is_fen;
	int color;
	BOARD board;
} GAMEPOS;



void init_move_tables();
int build_nonjump_list(BOARD *board, MOVELIST *movelist, int color);
int build_man_nonjump_list(BOARD *board, MOVELIST *movelist, int color);
int build_king_nonjump_list(BOARD *board, MOVELIST *movelist, int color);
int canjump(BOARD *board, int color);

template <bool save_capture_info>
int build_jump_list(BOARD *board, MOVELIST *movelist, int color, CAPTURE_INFO cap[MAXMOVES] = NULL);

int has_a_king_move(BOARD *p, int color);
BITBOARD black_safe_moveto(BOARD *board);
BITBOARD white_safe_moveto(BOARD *board);
int build_movelist(BOARD *board, int color, MOVELIST *movelist);
int build_movelist_path(BOARD *board, int color, MOVELIST *movelist, CAPTURE_INFO *path);
void get_jumped_square(int jumpi, int movei, int color, CAPTURE_INFO cap[], BITBOARD *jumped);
int has_jumped_square(int jumpcount, int movei, int color, CAPTURE_INFO cap[], BITBOARD jumped);
int has_landed_square(int jumpcount, int movei, int color, CAPTURE_INFO *cap, int square);
void get_from_to(int jumpcount, int movei, int color, BOARD *board, MOVELIST *movelist,
				 CAPTURE_INFO cap[], BITBOARD *from, BITBOARD *to);

void get_fromto(BITBOARD fromboard, BITBOARD toboard, int *fromsq, int *tosq);
void get_fromto(BOARD *fromboard, BOARD *toboard, int color, int *fromsq, int *tosq);
int match_jumpto_square(CAPTURE_INFO *cap, int jumpindex, int color, int jumpto_square, int jumpcount);


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


