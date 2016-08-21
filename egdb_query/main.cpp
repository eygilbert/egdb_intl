#include "egdb/egdb_intl.h"         // EGDB_POSITION
#include "egdb/egdb_intl_ext.h"     // EGDB_DRIVER_V2, Slice, slice_range, position_range_slice
#include "engine/board.h"           // BOARD
#include "engine/fen.h"             // print_fen
#include "engine/move_api.h"        // init_move_tables, canjump
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iostream>

using namespace egdb_interface;

//#define EYG
#ifdef EYG
#define PATH_WLD "C:/db_intl/wld_v2"
#endif

#define RH
#ifdef RH
	#ifdef _MSC_VER
		// drive name where Kingsrow is installed under Windows
		#define DRIVE_MAPPING "C:/"
	#else
		// VirtualBox virtual drive mapping
		#define DRIVE_MAPPING "/media/sf_C_DRIVE/"
	#endif

	// path on DRIVE_MAPPING where Kingsrow is installed
	#define KINGSROW_PATH "Program Files/Kingsrow International/"

	// location of the database
	#define PATH_WLD	DRIVE_MAPPING KINGSROW_PATH "wld_database"
#endif

#define TDIFF(start) (((double)(std::clock() + 1 - start)) / (double)CLOCKS_PER_SEC)


void print_msgs(char const *msg)
{
	std::printf("%s", msg);
}

bool is_full_zugzwang(int score_to_move, int score_not_tomove)
{
	return score_to_move == EGDB_LOSS && score_not_tomove == EGDB_LOSS;
}

bool is_minority_half_zugzwang(int score_to_move, int score_not_tomove)
{
	return score_to_move == EGDB_LOSS && score_not_tomove == EGDB_DRAW;
}

bool is_majority_half_zugzwang(int score_to_move, int score_not_tomove)
{
	return score_to_move == EGDB_DRAW && score_not_tomove == EGDB_LOSS;
}

void query_zugzwangs(EGDB_DRIVER_V2 *handle, int maxpieces)
{
    char fenbuf[150];
    
    std::clock_t const t0 = std::clock();
	auto const slices = slice_range(2, maxpieces + 1);
	std::cout << "Iterating over " << slices.size() << " slices\n";

	for (Slice const& slice : slices) {

		// filter on slice: both sides must have at least 1 man and 1 king
        // with Boost, use instead: for (auto const& slice : slice | boost::adaptors::filtered(slice_condition)) { /* bla */ }
		if (!(slice.nbm() >= 1 && slice.nbk() >= 1 && slice.nwm() >= 1 && slice.nwk() >= 1))
			continue;

		std::printf("\n%.2fsec: ", TDIFF(t0));
		std::cout << slice << '\n';

        // need EGDB_POSITION& instead of EGDB_POSITION const& because none of the lookup functions are const-correct
        for (EGDB_POSITION& pos : position_range_slice{ slice }) {

            // filter on position: no pending captures or threatening captures
            // with Boost, use instead: for (auto const& pos : position_range_slice{slice} | boost::adaptors::filtered(position_condition)) { /* bla */ }
            if (canjump((BOARD *)&pos, BLACK) || canjump((BOARD *)&pos, WHITE))
                continue;

            int valb = handle->lookup(&pos, EGDB_BLACK, 0);
            int valw = handle->lookup(&pos, EGDB_WHITE, 0);

            // zugzwangs from the perspective of black to move
            if (is_full_zugzwang(valb, valw)) {
                print_fen((BOARD *)&pos, EGDB_BLACK, fenbuf);
                std::printf("%s\t full point zugzwang: btm = white win, wtw = black win\n", fenbuf);
            }
            if (is_minority_half_zugzwang(valb, valw)) {
                print_fen((BOARD *)&pos, EGDB_BLACK, fenbuf);
                std::printf("%s\t minority half point zugzwang: btm = white win, wtm = draw\n", fenbuf);
            }
            if (is_majority_half_zugzwang(valb, valw)) {
                print_fen((BOARD *)&pos, EGDB_BLACK, fenbuf);
                std::printf("%s\t majority half point zugzwang: btm = draw, wtm = black win\n", fenbuf);
            }
        }
    }
}


int main()
{
    init_move_tables(); // Needed to use the Kingsrow move generator

    const int maxpieces = 4;
    char options[50];
    std::sprintf(options, "maxpieces=%d", maxpieces);

	EGDB_DRIVER_V2 db(options, 2000, PATH_WLD, print_msgs);
	if (!db.is_open()) {
		std::printf("Could not open db at %s\n", PATH_WLD);
		return(1);
	}

    // find all positions where the side to move can gain 1 or 2 points by not having the move
	query_zugzwangs(&db, maxpieces);
}

