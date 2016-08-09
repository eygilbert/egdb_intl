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

char const *valuestr(int value)
{
	switch (value) {
	case EGDB_WIN:
		return("win");
	case EGDB_DRAW:
		return("draw");
	case EGDB_LOSS:
		return("loss");
	}
	return("unknown");
}

bool only_single_king_on_single_corner_diag(EGDB_POSITION const& pos)
{
	return
			bitcount64(pos.king & SINGLE_CORNER_DIAG) == 1 &&
			bitcount64((pos.black | pos.white) & ~pos.king & SINGLE_CORNER_DIAG) == 0
	;
}

void query_zwugzwangs_slice(EGDB_DRIVER *handle, SLICE *slice)
{
	int64_t size, index;
	int valb, valw;
	EGDB_POSITION pos;
	char fenbuf[150];

	size = getdatabasesize_slice(slice->getnbm(), slice->getnbk(), slice->getnwm(), slice->getnwk());
	for (index = 0; index < size; ++index) {
		indextoposition_slice(index, &pos, slice->getnbm(), slice->getnbk(), slice->getnwm(), slice->getnwk());

		/* No captures or non-side captures. */
		if (
				canjump((BOARD *)&pos, BLACK) ||
				canjump((BOARD *)&pos, WHITE)
		)
			continue;

		valb = handle->lookup(handle, &pos, EGDB_BLACK, 0);
		valw = handle->lookup(handle, &pos, EGDB_WHITE, 0);

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
	SLICE slice;
	std::clock_t t0;

	t0 = std::clock();
	for (slice.reset(); slice.getnpieces() <= maxpieces; slice.advance()) {

		/* Both sides must have at least 1 man and 1 king. */
		if (!slice.getnbm() || !slice.getnbk() || !slice.getnwm() || !slice.getnwk())
			continue;

		std::printf("\n%.2fsec: db%d-%d%d%d%d\n", TDIFF(t0), slice.getnpieces(), slice.getnbm(), slice.getnbk(), slice.getnwm(), slice.getnwk());
		query_zwugzwangs_slice(handle, &slice);
	}
}


int main(int argc, char *argv[])
{
	const int maxpieces = 6;
	char options[50];
	EGDB_DRIVER *handle;

	init_move_tables();		/* Needed to use the kingsrow move generator. */

	sprintf(options, "maxpieces=%d", maxpieces);
	handle = egdb_open(options, 2000, PATH_WLD, print_msgs);
	if (!handle) {
		std::printf("Cannot open db at %s\n", PATH_WLD);
		return(1);
	}

	query_zugzwangs(handle, maxpieces);
	handle->close(handle);

	return 0;
}

