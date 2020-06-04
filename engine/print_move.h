#pragma once
#include <string>
#include <vector>
#include "project.h"
#include "board.h"

int print_move(egdb_interface::BOARD *last_board, egdb_interface::BOARD *new_board, int color, char *buf);
std::string print_move(egdb_interface::BOARD *last_board, egdb_interface::BOARD *new_board, int color);
bool poslist_to_moves(std::vector<egdb_interface::GAMEPOS> &poslist, std::string &moves);
const char *movestr(char *buf, egdb_interface::BOARD *blast, egdb_interface::BOARD *bnew);
