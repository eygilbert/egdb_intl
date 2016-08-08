#pragma once
#include "engine/board.h"

using namespace egdb_interface;

int print_fen(BOARD *board, int color, char *buf);
int print_fen_with_newline(BOARD *board, int color, char *buf);
int print_fen_header(BOARD *board, int color, char *buf, char const *line_terminator);
int parse_fen(char *buf, BOARD *board, int *ret_color);

