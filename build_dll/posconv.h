#pragma once

#include "engine\board.h"
#include "../egdb_intl_dll.h"


egdb_dll::Position board_to_dllpos(egdb_interface::BOARD &board);
egdb_interface::BOARD dllpos_to_board(egdb_dll::Position &pos);
