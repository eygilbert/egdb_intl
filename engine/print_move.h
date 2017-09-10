#pragma once

#include "project.h"
#include "board.h"

int print_move(egdb_interface::BOARD *last_board, egdb_interface::BOARD *new_board, int color, char *buf);
char *movestr(char *buf, egdb_interface::BOARD *blast, egdb_interface::BOARD *bnew);
