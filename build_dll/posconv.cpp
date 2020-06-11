#include "engine/board.h"
#include "../egdb_intl_dll.h"
#include "build_dll/posconv.h"


egdb_dll::Position board_to_dllpos(egdb_interface::BOARD &board)
{
	egdb_dll::Position pos;
	pos.black = board.black;
	pos.white = board.white;
	pos.king = board.king;
	return(pos);
}


egdb_interface::BOARD dllpos_to_board(egdb_dll::Position &pos)
{
	egdb_interface::BOARD board;
	board.black = pos.black;
	board.white = pos.white;
	board.king = pos.king;
	return(board);
}
