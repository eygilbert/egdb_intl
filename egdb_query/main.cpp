#include "builddb/indexing.h"
#include "egdb/egdb_intl.h"
#include "egdb/slice.h"
#include "engine/board.h"
#include "engine/fen.h"
#include "engine/move_api.h"
#include "engine/reverse.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>

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

void query_zwugzwangs_slice(EGDB_DRIVER *handle, Slice const& slice)
{
	EGDB_POSITION pos;
	char fenbuf[150];

	int64_t const size = getdatabasesize_slice(slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
	for (int64_t index = 0; index < size; ++index) {
		indextoposition_slice(index, &pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());

		/* No captures or non-side captures. */
		if (
				canjump((BOARD *)&pos, BLACK) ||
				canjump((BOARD *)&pos, WHITE)
		)
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


void query_zugzwangs(EGDB_DRIVER *handle, int maxpieces)
{
	std::clock_t const t0 = std::clock();
	auto const slices = slice_range(2, maxpieces + 1);
	std::cout << "Checking " << slices.size() << " slices\n";
	for (Slice const& slice : slices) {
		/* Both sides must have at least 1 man and 1 king. */
		if (!slice.nbm() || !slice.nbk() || !slice.nwm() || !slice.nwk())
			continue;

		std::printf("\n%.2fsec: ", TDIFF(t0));
		std::cout << slice << '\n';
		query_zwugzwangs_slice(handle, slice);
	}
}


int main()
{
	const int maxpieces = 4;
	char options[50];

	init_move_tables();		// Needed to use the kingsrow move generator.

	std::sprintf(options, "maxpieces=%d", maxpieces);
	EGDB_DRIVER *handle = egdb_open(options, 2000, PATH_WLD, print_msgs);
	if (!handle) {
		std::printf("Cannot open db at %s\n", PATH_WLD);
		return(1);
	}

	query_zugzwangs(handle, maxpieces);
	handle->close(handle);
}

