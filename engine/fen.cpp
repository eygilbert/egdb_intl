#include "fen.h"

// engine
#include "board.h"
#include "bool.h"
#include "project.h"

#include <cctype>
#include <cstdio>
#include <cstring>

int print_fen(BOARD *board, int color, char *buf)
{
	int len, bitnum, square0;
	BITBOARD mask, sq;

	len = sprintf(buf, "%c", color == BLACK ? 'B' : 'W');

	/* Print the white men and kings. */
	len += sprintf(buf + len, ":W");
	for (mask = board->white; mask; ) {

		/* Peel off the lowest set bit in mask. */
		sq = mask & -(int64)mask;
		bitnum = LSB64(sq);
		square0 = bitnum_to_square0(bitnum);
		if (sq & board->king)
			len += sprintf(buf + len, "K%d", square0 + 1);
		else
			len += sprintf(buf + len, "%d", square0 + 1);
		mask = mask & (mask - 1);
		if (mask)
			len += sprintf(buf + len, ",");
	}

	/* Print the black men and kings. */
	len += sprintf(buf + len, ":B");
	for (mask = board->black; mask; ) {

		/* Peel off the lowest set bit in mask. */
		sq = mask & -(int64)mask;
		bitnum = LSB64(sq);
		square0 = bitnum_to_square0(bitnum);
		if (sq & board->king)
			len += sprintf(buf + len, "K%d", square0 + 1);
		else
			len += sprintf(buf + len, "%d", square0 + 1);
		mask = mask & (mask - 1);
		if (mask)
			len += sprintf(buf + len, ",");
	}
	return(len);
}


int print_fen_with_newline(BOARD *board, int color, char *buf)
{
	int len;

	len = print_fen(board, color, buf);
	len += sprintf(buf + len, "\n");
	return(len);
}


int print_fen_header(BOARD *board, int color, char *buf, char *line_terminator)
{
	int len;

	len = sprintf(buf, "[FEN \"");
	len += print_fen(board, color, buf + len);
	len += sprintf(buf + len, "\"]%s", line_terminator);
	return(len);
}


/*
 * Return non-zero if the text in buf does not seem to be a fen string.
 */
int parse_fen(char *buf, BOARD *board, int *ret_color)
{
	int square, square2, s;
	int color;
	int king;
	char *lastp;

	while (isspace(*buf))
		++buf;
	if (*buf == '"')
		++buf;

	while (isspace(*buf))
		++buf;

	/* Get the color. */
	if (toupper(*buf) == 'B')
		*ret_color = BLACK;
	else if (toupper(*buf) == 'W')
		*ret_color = WHITE;
	else {
		return(1);
	}

	memset(board, 0, sizeof(BOARD));

	/* Skip the colon. */
	buf += 2;

	lastp = buf;
	while (*buf) {
		king = 0;
		if (*buf == '"') {
			++buf;
			continue;
		}
		if (toupper(*buf) == 'W') {
			color = WHITE;
			++buf;
			continue;
		}
		if (toupper(*buf) == 'B') {
			color = BLACK;
			++buf;
			continue;
		}
		if (toupper(*buf) == 'K') {
			king = 1;
			++buf;
		}
		for (square = 0; isdigit(*buf); ++buf)
			square = 10 * square + (*buf - '0');

		square2 = square;
		if (*buf == ',' || *buf == ':' || *buf == '.' || isspace(*buf))
			++buf;

		else if (*buf == '-' && isdigit(buf[1])) {
			++buf;
			for (square2 = 0; isdigit(*buf); ++buf)
				square2 = 10 * square2 + (*buf - '0');
			if (*buf == ',' || *buf == ':' || *buf == '.' || isspace(*buf))
				++buf;
		}

		if (square && square <= square2) {
			for (s = square; s <= square2; ++s) {
				if (color == WHITE)
					board->white |= square0_to_bitboard(s - 1);
				else
					board->black |= square0_to_bitboard(s - 1);
				if (king)
					board->king |= square0_to_bitboard(s - 1);
			}
		}
		if (lastp == buf)
			break;
		lastp = buf;
	}
	return(0);
}


