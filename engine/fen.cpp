#include "engine/fen.h"
#include "engine/board.h"
#include "engine/bool.h"
#include <cctype>
#include <cstdio>
#include <cstring>

namespace egdb_interface {

int consecutive_squares(int start, BITBOARD sidebb, BITBOARD &newsidebb, BITBOARD king, bool is_king)
{
	int bitnum, square0, count;
	BITBOARD sq;
	bool is_nextking;
	
	count = 1;
	newsidebb = sidebb;
	while (newsidebb) {
		sq = get_lsb(newsidebb);
		bitnum = LSB64(sq);
		square0 = bitnum_to_square0(bitnum);
		if (square0 != start + count)
			break;
		is_nextking = sq & king;
		if (is_king ^ is_nextking)
			break;
		newsidebb = clear_lsb(newsidebb);
		++count;
	}
	return(count);
}


void print_fen_side(BITBOARD sidebb, BITBOARD king, std::string &fenstr)
{
	int bitnum, square0, count;
	BITBOARD sq, newsidebb;

	while (sidebb) {
		sq = get_lsb(sidebb);
		bitnum = LSB64(sq);
		square0 = bitnum_to_square0(bitnum);
		if (sq & king)
			fenstr += "K";
		fenstr += std::to_string(square0 + 1);
		sidebb = clear_lsb(sidebb);
		count = consecutive_squares(square0, sidebb, newsidebb, king, sq & king);
		if (count >= 3) {
			fenstr += "-" + std::to_string(square0 + count);
			sidebb = newsidebb;
		}
		if (sidebb)
			fenstr += ",";
	}
}


void print_fen(const BOARD *board, int color, std::string &fenstr)
{
	fenstr = color == BLACK ? "B" : "W";

	/* Add the white pieces. */
	fenstr += ":W";
	print_fen_side(board->white, board->king, fenstr);

	/* Add the black pieces. */
	fenstr += ":B";
	print_fen_side(board->black, board->king, fenstr);
}


int print_fen(const BOARD *board, int color, char *buf)
{
	std::string fenstr;
	print_fen(board, color, fenstr);
	strcpy(buf, fenstr.c_str());
	return((int)fenstr.size());
}


int print_fen_header(const BOARD *board, int color, char *buf, char const *line_terminator)
{
	int len;

	len = std::sprintf(buf, "[FEN \"");
	len += print_fen(board, color, buf + len);
	len += std::sprintf(buf + len, "\"]%s", line_terminator);
	return(len);
}


/*
 * Return non-zero if the text in buf does not seem to be a fen setup.
 */
int parse_fen(const char *buf, BOARD *board, int *ret_color)
{
	int square, square2, s;
	int color = BLACK;	// prevent using unitialized memory
	int king;
	const char *lastp;

	while (std::isspace(*buf))
		++buf;
	if (*buf == '"')
		++buf;

	/* Get the color. */
	if (std::toupper(*buf) == 'B')
		*ret_color = BLACK;
	else if (std::toupper(*buf) == 'W')
		*ret_color = WHITE;
	else
		return(1);

	std::memset(board, 0, sizeof(BOARD));
	++buf;

	if (*buf != ':')
		return(1);

	++buf;
	lastp = buf;
	while (*buf) {
		king = 0;
		if (*buf == '"') {
			++buf;
			continue;
		}
		if (std::toupper(*buf) == 'W') {
			color = WHITE;
			++buf;
			continue;
		}
		if (std::toupper(*buf) == 'B') {
			color = BLACK;
			++buf;
			continue;
		}
		if (std::toupper(*buf) == 'K') {
			king = 1;
			++buf;
		}
		for (square = 0; std::isdigit(*buf); ++buf)
			square = 10 * square + (*buf - '0');

		square2 = square;
		if (*buf == ',' || *buf == ':')
			++buf;

		else if (*buf == '-' && std::isdigit(buf[1])) {
			++buf;
			for (square2 = 0; std::isdigit(*buf); ++buf)
				square2 = 10 * square2 + (*buf - '0');
			if (*buf == ',' || *buf == ':')
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

}   // namespace egdb_interface
