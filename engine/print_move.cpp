#include <stdio.h>
#include "project.h"
#include "board.h"
#include "bitcount.h"
#include "move_api.h"
#include "move.h"
#include "print_move.h"

using namespace egdb_interface;


/*
 * Write the move and return the number of characters put into the buffer.
 */
int print_move(BOARD *last_board, BOARD *new_board, int color, char *buf)
{
	int i, len, move_index, count;
	int fromsq0, tosq0;
	char separator;
	BITBOARD test_from, test_to;
	BITBOARD from, to;
	MOVELIST movelist;
	CAPTURE_INFO cap[MAXMOVES];

	/* Find this move in the kr movelist. */
	build_movelist_path(last_board, color, &movelist, cap);

	/* Find the move in the list. */
	for (move_index = 0; move_index < movelist.count; ++move_index) {
		if (memcmp(new_board, movelist.board + move_index, sizeof(BOARD)) == 0)
			break;
	}

	if (move_index >= movelist.count) {
		buf[0] = 0;
		return(0);					/* Illegal move. */
	}

	/* Find the number of moves that match this from and to square. */
	get_fromto_bitboards(move_index, color, last_board, &movelist, cap, &from, &to);
	count = 0;
	for (i = 0; i < movelist.count; ++i) {
		get_fromto_bitboards(i, color, last_board, &movelist, cap, &test_from, &test_to);
		if (test_from == from && test_to == to)
			++count;
	}

	if (count == 0) {
		buf[0] = 0;
		return(0);
	}

	if (count > 1) {
		/* Need to use the full move notation. */
		len = 0;
		for (i = 0; i < cap[move_index].capture_count; ++i) {
			if (color == WHITE)
				get_fromto(cap[move_index].path[i].white, cap[move_index].path[i + 1].white, &fromsq0, &tosq0);
			else
				get_fromto(cap[move_index].path[i].black, cap[move_index].path[i + 1].black, &fromsq0, &tosq0);

			if (i == 0)
				len += sprintf(buf + len, "%dx%d", 1 + fromsq0, 1 + tosq0);
			else
				len += sprintf(buf + len, "x%d", 1 + tosq0);
		}
	}
	else {
		if (cap[0].capture_count)
			separator = 'x';
		else
			separator = '-';
		len = sprintf(buf, "%d%c%d", bitboard_to_square(from), separator, bitboard_to_square(to));
	}

	return(len);
}


/*
 * Write the move and return the move string.
 * If no move then an empty string is returned.
 */
std::string print_move(BOARD *last_board, BOARD *new_board, int color)
{
	int i, len, move_index, count;
	int fromsq0, tosq0;
	char separator;
	BITBOARD test_from, test_to;
	BITBOARD from, to;
	MOVELIST movelist;
	std::string move;
	CAPTURE_INFO cap[MAXMOVES];

	/* Find this move in the kr movelist. */
	build_movelist_path(last_board, color, &movelist, cap);

	/* Find the move in the list. */
	for (move_index = 0; move_index < movelist.count; ++move_index) {
		if (memcmp(new_board, movelist.board + move_index, sizeof(BOARD)) == 0)
			break;
	}

	if (move_index >= movelist.count)
		return(move);					/* Illegal move. */

	/* Find the number of moves that match this from and to square. */
	get_fromto_bitboards(move_index, color, last_board, &movelist, cap, &from, &to);
	count = 0;
	for (i = 0; i < movelist.count; ++i) {
		get_fromto_bitboards(i, color, last_board, &movelist, cap, &test_from, &test_to);
		if (test_from == from && test_to == to)
			++count;
	}

	if (count == 0)
		return(move);

	if (count > 1) {
		/* Need to use the full move notation. */
		len = 0;
		for (i = 0; i < cap[move_index].capture_count; ++i) {
			if (color == WHITE)
				get_fromto(cap[move_index].path[i].white, cap[move_index].path[i + 1].white, &fromsq0, &tosq0);
			else
				get_fromto(cap[move_index].path[i].black, cap[move_index].path[i + 1].black, &fromsq0, &tosq0);

			if (i == 0)
				move += std::to_string(1 + fromsq0) + 'x' + std::to_string(1 + tosq0);
			else
				move += 'x' + std::to_string(1 + tosq0);
		}
	}
	else {
		if (cap[0].capture_count)
			separator = 'x';
		else
			separator = '-';
		move = std::to_string(bitboard_to_square(from)) + separator + std::to_string(bitboard_to_square(to));
	}

	return(move);
}


/*
 * Take a list of GAMEPOS positions and write a space-separated list of pdn moves.
 * Return true if there were no legal moves anywhere in the poslist.
 */
bool poslist_to_moves(std::vector<egdb_interface::GAMEPOS> &poslist, std::string &moves)
{
	size_t i;
	std::string move;

	moves.clear();
	for (i = 0; i < poslist.size() - 1; ++i) {
		move = print_move(&poslist[i].board, &poslist[i + 1].board, poslist[i].color);
		if (move.size() == 0)
			return(true);

		if (i > 0)
			moves += ' ';

		moves += move;
	}
	return(false);
}


/*
 * Given an old board and a new board, write a move string
 * of the form "9-13".
 */
const char *movestr(char *buf, BOARD *blast, BOARD *bnew)
{
	int i, color;
	char separator;
	int tosq, fromsq;
	int deltab, deltaw;
	BITBOARD deltamask;

	deltab = bitcount64(bnew->black) - bitcount64(blast->black);
	deltaw = bitcount64(bnew->white) - bitcount64(blast->white);
	if (deltab < 0) {
		/* It was a white jump move. */
		deltamask = bnew->white ^ blast->white;
		separator = 'x';
		color = WHITE;
	}
	else if (deltaw < 0) {
		/* It was a black jump move. */
		deltamask = bnew->black ^ blast->black;
		separator = 'x';
		color = BLACK;
	}
	else {
		if (deltaw > 0 || deltab > 0)
			return("???");				/* Something is really wrong. */

		/* It was a nonjump move. Assume first it was a black move. */
		separator = '-';
		deltamask = bnew->black ^ blast->black;
		if (!deltamask)
			/* It was a white move. */
			deltamask = bnew->white ^ blast->white;
	}
	if (!deltamask) {
		/* jumping around and returning to original square. */
		BITBOARD from, to;
		MOVELIST movelist;
		CAPTURE_INFO jump_info[MAXMOVES];

		build_jump_list<true>(blast, &movelist, color, jump_info);
		if (!movelist.count)
			return("???");

		for (i = 0; i < movelist.count; ++i) {
			if (!memcmp(bnew, movelist.board + i, sizeof(movelist.board[0]))) {

				/* found the move. */
				get_fromto_bitboards(i, color, blast, &movelist, jump_info, &from, &to);
				fromsq = bitboard_to_square(from);
				tosq = bitboard_to_square(to);
				sprintf(buf, "%dx%d", fromsq, tosq);
				return(buf);
			}
		}
		return("???");
	}
	for (i = 0; i < NUMSQUARES; ++i)
		if (square0_to_bitboard(i) & deltamask)
			if (square0_to_bitboard(i) & deltamask & (bnew->white | bnew->black))
				tosq = i;
			else
				fromsq = i;
	sprintf(buf, "%d%c%d", fromsq + 1, separator, tosq + 1);
	return(buf);
}

