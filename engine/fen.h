#pragma once
#include "engine/board.h"
#include <string>

namespace egdb_interface {
	void print_fen(const BOARD *board, int color, std::string &fenstr);
    int print_fen(const BOARD *board, int color, char *buf);
    int print_fen_header(const BOARD *board, int color, char *buf, char const *line_terminator);
    int parse_fen(const char *buf, BOARD *board, int *ret_color);

}   // namespace egdb_interface
