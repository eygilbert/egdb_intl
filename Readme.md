## Overview 

The DLL gives access to the endgame databases from a number of languages. See the file `egdb_intl_dll.h` for definitions of types and constants. If you're accessing the DLL from a C++ program, you should #include that header file in your source files.

This branch is a modification to the "dll" branch. It modifies the draughts move generator to work exactly like Gerard Benning's move generator, so that he can use the dll in his VBA program that finds "sharp" draughts positions.

See files in the VBA_test directory for access from VBA.

## Functions
For the functions below that return an int value, unless otherwise stated a negative value indicates an error. See error values in `egdb_intl_dll.h`.

Some of the DLL function below are similar to the C++ driver functions described in the `master` repository branch. You may find more info on those function in the master branch docs.
#### extern "C" int __stdcall egdb_open(char *options, int cache_mb, const char *directory, const char *filename);
Opens a database for access. The function return value is a handle that can be passed to other access functions.
Options, cache_mb, and directory are identical to the C++ version of this function (see Master branch).
Filename is the name of a logging file. The driver will write status and error messages to this file. Give an empty string ("") for no file.
#### extern "C" int __stdcall egdb_close(int handle);
Closes the driver and releases all associated resources. 
#### extern "C" int __stdcall egdb_identify(const char *directory, int *egdb_type, int *max_pieces);
Checks if an endgame database exists in `directory`. If so, the return value of the function is 0, the database type is returned in `egdb_type` and the maximum number of pieces is returned in `max_pieces`.
#### extern "C" int __stdcall egdb_lookup(int handle, Position *pos, int color, int cl);
Returns the value of a position in the database. `pos` is the position in bitboard format.
#### extern "C" int __stdcall egdb_lookup_fen(int handle, char *fen, int cl);
Returns the value of a position in the database. `fen` is the position in FEN notation. Example: "B:WK3,K9,49:B2,7,13,37".
#### extern "C" int __stdcall egdb_lookup_fen_with_search(int handle, char *fen);
Does a search and returns the value of a position in a WLD database. The WLD database may exclude captures and other positions to make the compression work better, but these values can be found using a search.
`fen` is the position in FEN notation. Example: "B:WK3,K9,49:B2,7,13,37".
#### extern "C" int __stdcall egdb_lookup_with_search(int handle, Position *pos, int color);
Does a search and returns the value of a position in a WLD database. The WLD database does not have values for all positions meeting the max_pieces constraints, but these values can be found using a search.
`pos` is the position in bitboard format.
#### extern "C" int __stdcall egdb_lookup_distance(int handle_wld, int handle_dist, const char *fen, int distances[maxmoves], char moves[maxmoves][20]);
Returns distance values in plies and their corresponding best-moves from a distance to win (DTW) or distance to conversion (MTC) database. Values in the `distances` and  `moves` arrays are ordered as best moves first. For example, if the position is a win, then move[0] is the move leading to the shortest distance to win, and distances[0] is that distance in plies. The function requires handles to both a WLD db and a distance db (DTW or MTC).
The return value the number of elements returned in `distances` and `moves`.
#### extern "C" int __stdcall get_movelist(Position *pos, int color, Position movelist[maxmoves]);
Returns in the array `movelist` a list of successor positions reachable by legal moves from the start position given by `pos` and `color`. The function return value is the number of moves in `movelist`.
#### extern "C" int16_t __stdcall is_capture(Position *pos, int color);
The function returns non-zero if the position has capture moves.
#### extern "C" int64_t __stdcall getdatabasesize_slice(int nbm, int nbk, int nwm, int nwk);
Returns the number of positions in a database subset defined by number of black men, black kings, white men, and white kings.
#### extern "C" void __stdcall indextoposition(int64_t index, Position *pos, int nbm, int nbk, int nwm, int nwk);
Converts a database subset index into a position. The range of index is 0 through size-1. The subset is defined by number of black men, black kings, white men, and white kings.
#### extern "C" int64_t __stdcall positiontoindex(Position *pos, int nbm, int nbk, int nwm, int nwk);
Converts a position into an index.
#### extern "C" int32_t __stdcall sharp_status(int handle, Position *pos, int color, Position *sharp_move_pos);
Returns the sharp status of the position defined by `pos` and `color`.

Returns EGDB_SHARP_STAT_TRUE if the position has exactly one move that wins. In this case the successor position is returned in `sharp_move_pos`.

Returns EGDB_SHARP_STAT_FALSE if the position is not sharp.

Returns EGDB_SHARP_STAT_UNKNOWN if the sharp status cannot be determined. This could happen if the function egdb_lookup_with_search(), which is used internally by sharp_status(), got a return value of EGDB_UNKNOWN.
#### extern "C" int __stdcall move_string(Position *last_pos, Position *new_pos, int color, char *move);
Given a draughts move that changes the position from `last_pos` to `new_pos`, this function returns the PDN character string representation of the move in `move`.
#### extern "C" int __stdcall sharp_capture_path(Position *last_board, Position *new_board, int color, char *landed, char *captured);
Returns the landed and captured squares of a capture move described by last_board, new_board, and color. The return value from the function is the number of pieces captured. landed[0] is the 'from' square, landed[1] is the landed square after the first jump, ... captured[0] is the first jumped square, ... If the return value is 0, it means that either there is not a capture move, or there is more than one capture move between the positions given.
#### extern "C" int __stdcall positiontofen(Position *pos, int color, char *fen);
Converts a draughts position given in bitboard format to FEN character string format. The return value of the function is the number of characters written to `fen`.
#### extern "C" int __stdcall fentoposition(char *fen, Position *pos, int *color);
Converts a draughts position given in FEN character string format to bitboard format.
## License

    Original work Copyright (C) 2007-2021 Ed Gilbert

Distributed under the Boost Software License, Version 1.0.
(See accompanying file [LICENSE_1_0.txt](LICENSE_1_0.txt) or copy at
[http://www.boost.org/LICENSE_1_0.txt](http://www.boost.org/LICENSE_1_0.txt))
