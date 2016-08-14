#include "egdb/egdb_intl.h"         // EGDB_DRIVER, EGDB_POSITION
#include "egdb/egdb_intl_ext.h"     // Slice, slice_range, position_range_slice
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

bool is_full_zugzwang(int left, int right)
{
	return left == EGDB_LOSS && right == EGDB_LOSS;
}

bool is_minority_half_zugzwang(int left, int right)
{
	return left == EGDB_LOSS && right == EGDB_DRAW;
}

bool is_majority_half_zugzwang(int left, int right)
{
	return left == EGDB_DRAW && right == EGDB_LOSS;
}

void query_zugzwangs(EGDB_DRIVER *handle, int maxpieces)
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

            int valb = handle->lookup(handle, &pos, EGDB_BLACK, 0);
            int valw = handle->lookup(handle, &pos, EGDB_WHITE, 0);

            if (is_full_zugzwang(valw, valb)) {
                print_fen((BOARD *)&pos, EGDB_WHITE, fenbuf);
                std::printf("%s\t full point zugzwang: wtm = black win, btw = white win\n", fenbuf);
            }
            if (is_minority_half_zugzwang(valw, valb)) {
                print_fen((BOARD *)&pos, EGDB_WHITE, fenbuf);
                std::printf("%s\t minority half point zugzwang: wtm = black win, btm = draw\n", fenbuf);
            }
            if (is_majority_half_zugzwang(valw, valb)) {
                print_fen((BOARD *)&pos, EGDB_WHITE, fenbuf);
                std::printf("%s\t majority half point zugzwang: wtm = draw, btm = white win\n", fenbuf);
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

	EGDB_DRIVER *handle = egdb_open(options, 2000, PATH_WLD, print_msgs);
	if (!handle) {
		std::printf("Cannot open db at %s\n", PATH_WLD);
		return(1);
	}

    // find all positions where the side to move can gain 1 or 2 points by not having the move
	query_zugzwangs(handle, maxpieces);
	
    handle->close(handle);
}

