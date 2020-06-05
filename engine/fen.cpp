#include "engine/fen.h"
#include "engine/board.h"
#include "engine/bool.h"
#include <cctype>
#include <cstdio>
#include <cstring>

namespace egdb_interface {

void print_fen(const BOARD *board, int color, std::string &fenstr)
{
    int bitnum, square0;
    BITBOARD bb, sq;

    fenstr = color == BLACK ? "B" : "W";

    /* Add the white pieces. */
    fenstr += ":W";
    for (bb = board->white; bb; ) {
        sq = get_lsb(bb);
        bitnum = LSB64(sq);
        square0 = bitnum_to_square0(bitnum);
        if (sq & board->king)
			fenstr += "K";
		fenstr += std::to_string(square0 + 1);
        bb = clear_lsb(bb);
        if (bb)
			fenstr += ",";
    }

    /* Add the black pieces. */
	fenstr += ":B";
	for (bb = board->black; bb; ) {
		sq = get_lsb(bb);
		bitnum = LSB64(sq);
		square0 = bitnum_to_square0(bitnum);
		if (sq & board->king)
			fenstr += "K";
		fenstr += std::to_string(square0 + 1);
		bb = clear_lsb(bb);
		if (bb)
			fenstr += ",";
	}
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
