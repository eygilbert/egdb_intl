#include "builddb/indexing.h"
#include "egdb/egdb_intl.h"
#include "egdb/slice.h"
#include "egdb/egdb_search.h"
#include "engine/bitcount.h"
#include "engine/board.h"
#include "engine/fen.h"
#include "engine/lock.h"
#include "engine/move_api.h"
#include "engine/project.h"	// ARRAY_SIZE
#include <algorithm>
#include <cctype>
#include <inttypes.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef USE_MULTI_THREADING
	#include <thread>
#endif

using namespace egdb_interface;

#define FAST_TEST_MULT 1		/* Set to 5 to speed up the tests. */

#define EYG
#ifdef EYG
	#ifdef _MSC_VER
		// drive name where Kingsrow is installed under Windows
		#define C_DRIVE "C:/"
		#define E_DRIVE "E:/"
	#else
		// VirtualBox virtual drive mapping
		#define C_DRIVE "/media/sf_C_DRIVE/"
		#define E_DRIVE "/media/sf_E_DRIVE/"
	#endif
#define DB_RUNLEN C_DRIVE "db_intl/wld_runlen"	/* Need 6 pieces for test. */
#define DB_TUN_V1 E_DRIVE "db_intl/wld_v1"		/* Need 7 pieces for test. */
#define DB_TUN_V2 C_DRIVE "db_intl/wld_v2"		/* Need 8 pieces for test. */
#define DB_MTC E_DRIVE "db_intl/slice32_mtc"	/* Need 8 pieces for test. */
const int maxpieces = 8;
#endif

//#define RH
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

	// locations of the various databaess
	#define DB_RUNLEN	DRIVE_MAPPING KINGSROW_PATH "wld_runlen"	/* Need 6 pieces for test. */
	#define DB_TUN_V1	DRIVE_MAPPING KINGSROW_PATH "wld_tun_v1"	/* Need 7 pieces for test. */
	#define DB_TUN_V2	DRIVE_MAPPING KINGSROW_PATH "wld_database"	/* Need 8 pieces for test. */
	#define DB_MTC		DRIVE_MAPPING KINGSROW_PATH "mtc_database"	/* Need 8 pieces for test. */
	const int maxpieces = 7;
#endif

#define TDIFF(start) (((double)(std::clock() + 1 - start)) / (double)CLOCKS_PER_SEC)

typedef struct {
	EGDB_DRIVER *handle;
	int max_pieces;
	EGDB_TYPE egdb_type;
	bool excludes_some_nonside_captures;	/* If more than 6 pieces. */
	bool excludes_some_sidetomove_colors;	/* If more than 6 pieces. */
} DB_INFO;

namespace egdb_interface {
	extern LOCKT *get_tun_v1_lock();
	extern LOCKT *get_tun_v2_lock();
}


void print_msgs(char const *msg)
{
	std::printf("%s", msg);
}


void verify(DB_INFO *db1, DB_INFO *db2, Slice const& slice, int64_t max_lookups)
{
	int64_t size, index, incr;
	int value1, value2;
	bool black_ok, white_ok;
	EGDB_POSITION pos;

	std::printf("Verifying db%d%d%d%d\n", slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
	size = getdatabasesize_slice(slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
	if (max_lookups < 1)
		incr = 1;
	else
		incr = (std::max)(size / max_lookups, (int64_t)1);

	/* If npieces >= 7, some slices do not have positions for both colors. */
	black_ok = true;
	white_ok = true;

	if (slice.npieces() >= 7) {
		if (db1->excludes_some_sidetomove_colors) {
			indextoposition_slice(0, &pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
			value1 = egdb_lookup(db1->handle, &pos, BLACK, 0);
			if (value1 == EGDB_UNKNOWN)
				black_ok = false;
			value1 = egdb_lookup(db1->handle, &pos, WHITE, 0);
			if (value1 == EGDB_UNKNOWN)
				white_ok = false;
		}
		if (db2->excludes_some_sidetomove_colors) {
			indextoposition_slice(0, &pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
			value2 = egdb_lookup(db2->handle, &pos, BLACK, 0);
			if (value2 == EGDB_UNKNOWN)
				black_ok = false;
			value2 = egdb_lookup(db2->handle, &pos, WHITE, 0);
			if (value2 == EGDB_UNKNOWN)
				white_ok = false;
		}
		if (black_ok == false && white_ok == false) {
			std::printf("Between db1 and db2, neither black or white can be probed. Shouldn't be possible.\n");
			std::exit(1);
		}
	}

	for (index = 0; index < size; index += incr) {
		indextoposition_slice(index, &pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
		if (slice.npieces() >= 7) {
			if (db1->excludes_some_nonside_captures || db2->excludes_some_nonside_captures) {
				if (canjump((BOARD *)&pos, BLACK))
					continue;

				if (canjump((BOARD *)&pos, WHITE))
					continue;
			}
		}
		if (black_ok && !canjump((BOARD *)&pos, BLACK)) {
			value1 = egdb_lookup(db1->handle, &pos, BLACK, 0);
			value2 = egdb_lookup(db2->handle, &pos, BLACK, 0);
			if (value1 != value2) {
				std::printf("Verify error..%" PRId64 "color BLACK, v1 %d, v2 %d\n", index, value1, value2);
				std::exit(1);
			}
		}

		if (white_ok && !canjump((BOARD *)&pos, WHITE)) {
			value1 = egdb_lookup(db1->handle, &pos, WHITE, 0);
			value2 = egdb_lookup(db2->handle, &pos, WHITE, 0);
			if (value1 != value2) {
				std::printf("Verify error.%" PRId64 "color WHITE, v1 %d, v2 %d\n", index, value1, value2);
				std::exit(1);
			}
		}
	}
}


void self_verify(EGDB_INFO *db, Slice const& slice, int64_t max_lookups)
{
	int64_t size, index, incr;
	int value1, value2;
	bool black_ok, white_ok;
	EGDB_POSITION pos;

	std::printf("self verifying db%d%d%d%d\n", slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
	size = getdatabasesize_slice(slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
	if (max_lookups < 1)
		incr = 1;
	else
		incr = (std::max)(size / max_lookups, (int64_t)1);

	/* If npieces >= 7, some slices do not have positions for both colors. */
	black_ok = true;
	white_ok = true;
	if (slice.npieces() >= 7) {
		if (db->egdb_excludes_some_nonside_caps) {
			indextoposition_slice(0, &pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
			value1 = egdb_lookup(db->handle, &pos, BLACK, 0);
			if (value1 == EGDB_UNKNOWN)
				black_ok = false;
			value1 = egdb_lookup(db->handle, &pos, WHITE, 0);
			if (value1 == EGDB_UNKNOWN)
				white_ok = false;
		}
		if (black_ok == false && white_ok == false) {
			std::printf("Neither black or white can be probed. Shouldn't be possible.\n");
			std::exit(1);
		}
	}

	for (index = 0; index < size; index += incr) {
		indextoposition_slice(index, &pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
		if (db->requires_nonside_capture_test((BOARD *)&pos)) {
			if (canjump((BOARD *)&pos, BLACK))
				continue;

			if (canjump((BOARD *)&pos, WHITE))
				continue;
		}
		if (black_ok && !canjump((BOARD *)&pos, BLACK)) {
			value1 = egdb_lookup(db->handle, &pos, BLACK, 0);
			value2 = db->lookup_with_search((BOARD *)&pos, BLACK, 64, true);
			if (value1 != value2 && value2 != EGDB_UNKNOWN) {
				std::printf("Verify error.%" PRId64 "color BLACK, v1 %d, v2 %d\n", index, value1, value2);
				std::exit(1);
			}
		}

		if (white_ok && !canjump((BOARD *)&pos, WHITE)) {
			value1 = egdb_lookup(db->handle, &pos, WHITE, 0);
			value2 = db->lookup_with_search((BOARD *)&pos, WHITE, 64, true);
			if (value1 != value2 && value2 != EGDB_UNKNOWN) {
				std::printf("Verify error.%" PRId64 "color WHITE, v1 %d, v2 %d\n", index, value1, value2);
				std::exit(1);
			}
		}
	}
}


void verify_indexing(Slice const& slice, int64_t max_tests)
{
	int64_t size, index, return_index, incr;
	EGDB_POSITION pos;

	size = getdatabasesize_slice(slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
	if (max_tests < 1)
		incr = 1;
	else
		incr = (std::max)(size / max_tests, (int64_t)1);

	for (index = 0; index < size; index += incr) {
		indextoposition_slice(index, &pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
		return_index = position_to_index_slice(&pos, slice.nbm(), slice.nbk(), slice.nwm(), slice.nwk());
		if (return_index != index) {
			std::printf("index verify error%" PRId64 "return index%" PRId64 "\n", index, return_index);
			std::exit(1);
		}
	}
}


int identify(char const *dir, DB_INFO *db)
{
	int status;

	status = egdb_identify(dir, &db->egdb_type, &db->max_pieces);
	if (status) {
		std::printf("db not found at %s\n", dir);
		return(1);
	}
	if (db->egdb_type == EGDB_WLD_TUN_V2) {
		db->excludes_some_nonside_captures = true;
		db->excludes_some_sidetomove_colors = true;
	}
	else {
		db->excludes_some_nonside_captures = false;
		db->excludes_some_sidetomove_colors = false;
	}
	return(0);
}


bool is_conversion_move(BOARD *from, BOARD *to, int color)
{
	uint64_t from_bits, to_bits, moved_bits;

	/* It is a conversion move if its a capture. */
	from_bits = from->black | from->white;
	to_bits = to->black | to->white;
	if (bitcount64(from_bits) > bitcount64(to_bits))
		return(true);

	/* Return true if there is a man move. */
	moved_bits = from_bits & ~to_bits;
	if (moved_bits & from->king)
		return(false);
	else
		return(true);
}


void test_best_mtc_successor(EGDB_INFO *wld, EGDB_DRIVER *mtc, BOARD *pos, int color)
{
	int fromsq, tosq;
	int i, value, mtc_value, parent_value, parent_mtc_value, best_movei, best_mtc_value;
	MOVELIST movelist;
	char fenbuf[120];

	parent_value = wld->lookup_with_search(pos, color, MAXREPDEPTH, false);
	if (parent_value == EGDB_WIN)
		best_mtc_value = 1000;
	else if (parent_value == EGDB_LOSS)
		best_mtc_value = 0;
	else if (parent_value == EGDB_UNKNOWN)
		return;
	else {
		std::printf("bad wld value, %d, in test_best_mtc_successor\n", parent_value);
		return;
	}
	parent_mtc_value = egdb_lookup(mtc, (EGDB_POSITION *)pos, color, 0);

	build_movelist(pos, color, &movelist);
	best_movei = 0;
	for (i = 0; i < movelist.count; ++i) {

		/* We're not going to take a conversion move if in a loss, because that's a terrible
		 * move. We already know that the parent position is at least 10 ply from a conversion by some path,
		 * and being in a loss we want to delay a conversion as long as possible,
		 * so there is a better choice than a conversion move.
		 */
		if (parent_value == EGDB_LOSS) {
			if (is_conversion_move(pos, movelist.board + i, color))
				continue;
		}
		value = wld->lookup_with_search((movelist.board + i), OTHER_COLOR(color), MAXREPDEPTH, false);
		if (value == EGDB_UNKNOWN)
			return;
		if (parent_value == EGDB_WIN)
			if (value != EGDB_LOSS)
				continue;
		mtc_value = egdb_lookup(mtc, (EGDB_POSITION *)(movelist.board + i), OTHER_COLOR(color), 0);
		if (mtc_value != MTC_LESS_THAN_THRESHOLD) {
			if (parent_value == EGDB_WIN) {
				if (value == EGDB_LOSS) {
					if (mtc_value < best_mtc_value) {
						best_mtc_value = mtc_value;
						best_movei = i;
					}
				}
			}
			else {
				if (parent_value == EGDB_LOSS) {
					if (value == EGDB_WIN) {
						if (mtc_value > best_mtc_value) {
							best_mtc_value = mtc_value;
							best_movei = i;
						}
					}
				}
			}
		}
	}
	get_fromto(pos, movelist.board + best_movei, color, &fromsq, &tosq);
	if (parent_mtc_value >= 12) {
		if (best_mtc_value == parent_mtc_value || best_mtc_value == parent_mtc_value - 2) {
			test_best_mtc_successor(wld, mtc, movelist.board + best_movei, OTHER_COLOR(color));
		}
		else {
			print_fen(movelist.board + best_movei, OTHER_COLOR(color), fenbuf);
			std::printf("move %d-%d, parent mtc value %d, best successor mtc value %d\n",
					1 + fromsq, 1 + tosq, parent_mtc_value, best_mtc_value);
			std::exit(1);
		}
	}
}


/*
 * Read a file of the highest MTC positions in each db subdivision.
 * Lookup the MTC value of that position, and then descend into one of the
 * best successor positions. Keep going until there are no successors with
 * MTC values >= 12. Make sure that each position has a successor with either
 * the same MTC value or a value that is smaller by 2. MTC values are always even
 * because the db stores the real MTC value divided by 2.
 */
void mtc_test()
{
	int color, status, max_pieces, value, max_pieces_1side, max_9pc_kings, max_8pc_kings_1side;
	FILE *fp;
	EGDB_TYPE type;
	char const *mtc_stats_filename = "../egdb_test/10x10 HighMtc.txt"; // consistent with out-of-tree CMake builds
	char linebuf[120];
	BOARD pos;
	EGDB_INFO wld;

	std::printf("\nMTC test.\n");
	status = egdb_identify(DB_MTC, &type, &max_pieces);
	if (status) {
		std::printf("MTC db not found at %s\n", DB_MTC);
		std::exit(1);
	}
	if (type != EGDB_MTC_RUNLEN) {
		std::printf("Wrong db type, not MTC.\n");
		std::exit(1);
	}
	if (max_pieces != 8) {
		std::printf("Need 8 pieces MTC for test, only found %d.\n", max_pieces);
		std::exit(1);
	}

	/* Open the endgame db drivers. */
	wld.handle = egdb_open("maxpieces=8", 3000, DB_TUN_V2, print_msgs);
	if (!wld.handle) {
		std::printf("Cannot open tun v2 db\n");
		std::exit(1);
	}
	egdb_get_pieces(wld.handle, &max_pieces, &max_pieces_1side, &max_9pc_kings, &max_8pc_kings_1side);
	wld.dbpieces = max_pieces;
	wld.dbpieces_1side = max_pieces_1side;
	wld.db8_kings_1side = max_8pc_kings_1side;
	wld.egdb_excludes_some_nonside_caps = true;

	EGDB_DRIVER* mtc = egdb_open("maxpieces=8", 100, DB_MTC, print_msgs);
	if (!mtc) {
		std::printf("Cannot open MTC db\n");
		std::exit(1);
	}
	fp = std::fopen(mtc_stats_filename, "r");
	if (!fp) {
		std::printf("Cannot open %s for reading\n", mtc_stats_filename);
		std::exit(1);
	}

	for (int count = 0; ; ) {
		if (!std::fgets(linebuf, ARRAY_SIZE(linebuf), fp))
			break;
		if (parse_fen(linebuf, &pos, &color))
			continue;

		/* For 7 and 8 pieces, only test every 10th position, to speed up the test. */
		++count;
		if ((count % (10 * FAST_TEST_MULT)) && bitcount64(pos.black | pos.white) >= 7)
			continue;

		print_fen(&pos, color, linebuf);
		std::printf("%s ", linebuf);
		value = egdb_lookup(mtc, (EGDB_POSITION *)&pos, color, 0);
		if (value >= 12) {
			test_best_mtc_successor(&wld, mtc, &pos, color);
		}

		std::printf("mtc %d\n", value);
	}
	egdb_close(mtc);
	egdb_close(wld.handle);
}


void open_options_test(void)
{
	int value1, value2;
	EGDB_POSITION pos;

	{
		/* Verify that no slices with more than 2 kings on a side are loaded. */
		std::printf("\nTesting open options.\n");
		EGDB_DRIVER *db = egdb_open("maxpieces = 8; maxkings_1side_8pcs = 2", 500, DB_TUN_V2, print_msgs);
		if (!db) {
			std::printf("Cannot open db at %s\n", DB_TUN_V2);
			std::exit(1);
		}
		indextoposition_slice(0, &pos, 2, 2, 2, 2);
		value1 = egdb_lookup(db, &pos, BLACK, 0);
		if (value1 != EGDB_WIN && value1 != EGDB_DRAW && value1 != EGDB_LOSS) {
			std::printf("bad value returned from lookup() in options test.\n");
			std::exit(1);
		}
		indextoposition_slice(0, &pos, 1, 3, 2, 2);
		value1 = egdb_lookup(db, &pos, BLACK, 0);
		value2 = egdb_lookup(db, &pos, WHITE, 0);
		if (value1 != EGDB_SUBDB_UNAVAILABLE || value2 != EGDB_SUBDB_UNAVAILABLE) {
			std::printf("bad value returned from lookup() in options test.\n");
			std::exit(1);
		}
		egdb_close(db);
	}

	{
		/* Verify that no slices with more than 6 pieces are loaded. */
		EGDB_DRIVER *db = egdb_open("maxpieces = 6", 500, DB_TUN_V2, print_msgs);
		if (!db) {
			std::printf("Cannot open db at %s\n", DB_TUN_V2);
			std::exit(1);
		}
		indextoposition_slice(0, &pos, 2, 1, 3, 0);
		value1 = egdb_lookup(db, &pos, BLACK, 0);
		if (value1 != EGDB_WIN && value1 != EGDB_DRAW && value1 != EGDB_LOSS) {
			std::printf("bad value returned from lookup() in options test.\n");
			std::exit(1);
		}
		indextoposition_slice(0, &pos, 4, 0, 3, 0);
		value1 = egdb_lookup(db, &pos, BLACK, 0);
		value2 = egdb_lookup(db, &pos, WHITE, 0);
		if (value1 != EGDB_SUBDB_UNAVAILABLE || value2 != EGDB_SUBDB_UNAVAILABLE) {
			std::printf("bad value returned from lookup() in options test.\n");
			std::exit(1);
		}
		egdb_close(db);
	}
}


void crc_verify_test(char const *dbpath)
{
	int abort;
	EGDB_VERIFY_MSGS verify_msgs = {
		"crc failed",
		"ok",
		"errors",
		"no errors"
	};

	std::printf("\nTesting crc verify\n");
	EGDB_DRIVER* db = egdb_open("maxpieces=6", 100, dbpath, print_msgs);
	if (!db) {
		std::printf("Cannot open db at %s\n", dbpath);
		std::exit(1);
	}
	abort = 0;
	egdb_verify(db, print_msgs, &abort, &verify_msgs);
	egdb_close(db);
}

void parallel_read(EGDB_DRIVER *db, int &value)
{
	EGDB_POSITION pos;

	indextoposition_slice(0, &pos, 2, 1, 2, 1);
	value = egdb_lookup(db, &pos, EGDB_BLACK, 0);
}

#ifdef USE_MULTI_THREADING

	void test_mutual_exclusion(char const *dbpath, LOCKT *lock)
	{
		int value;

		std::printf("\nTesting mutual exclusion, %s\n", dbpath);

		// Open with minimum cache memory, so that all lookups will have to go to disk.
		EGDB_DRIVER* db = egdb_open("maxpieces=6", 0, dbpath, print_msgs);
		if (!db) {
			std::printf("Cannot open db at %s\n", dbpath);
			std::exit(1);
		}

		value = EGDB_UNKNOWN;
		take_lock(*lock);
		std::thread threadobj(parallel_read, db, std::ref(value));
		std::chrono::milliseconds delay_time(3000);
		std::this_thread::sleep_for(delay_time);
		if (value != EGDB_UNKNOWN) {
			printf("Lock did not enforce mutual exclusion.\n");
			std::exit(1);
		}
		release_lock(*lock);
		std::this_thread::sleep_for(delay_time);
		if (value == EGDB_UNKNOWN) {
			printf("Releasking lock did not disable mutual exclusion.\n");
			std::exit(1);
		}
		threadobj.detach();
		egdb_close(db);
	}
	
#endif

int main(int argc, char *argv[])
{
	DB_INFO db1, db2;
	clock_t t0;

	init_move_tables();

	if (identify(DB_TUN_V2, &db1))
		return(1);
	if (identify(DB_TUN_V1, &db2))
		return(1);

	/* Open the endgame db drivers. */
	char opt[50];
	std::sprintf(opt, "maxpieces=%d", maxpieces);
	db1.handle = egdb_open(opt, 2000, DB_TUN_V2, print_msgs);
	if (!db1.handle) {
		std::printf("Cannot open tun v2 db\n");
		return(1);
	}
	db2.handle = egdb_open(opt, 2000, DB_TUN_V1, print_msgs);
	if (!db2.handle) {
		std::printf("Cannot open tun v1 db\n");
		return(1);
	}

	/* Do a quick verification of the indexing functions. */
	t0 = std::clock();
	std::printf("\nTesting indexing round trip\n");
	for (Slice first(2), last(10); first != last; first.increment()) {
		verify_indexing(first, 100000);
	}
	std::printf("%.2fsec: index test completed\n", TDIFF(t0));

	std::printf("\nVerifying WLD Tunstall v1 against Tunstall v2 (up to 7 pieces).\n");
	t0 = std::clock();
	for (Slice first(2), last(maxpieces + 1); first != last; first.increment()) {
		std::printf("%.2fsec: ", TDIFF(t0));
		verify(&db1, &db2, first, 50000 / FAST_TEST_MULT);
	}

	/* Close db2, leave db1 open for the next test. */
	egdb_close(db2.handle);

	std::printf("\nVerifying WLD runlen against WLD Tunstall v2 (up to 6 pieces).\n");
	if (identify(DB_RUNLEN, &db2))
		return(1);

	db2.handle = egdb_open("maxpieces=6", 2000, DB_RUNLEN, print_msgs);
	if (!db2.handle) {
		std::printf("Cannot open runlen db\n");
		return(1);
	}

	t0 = std::clock();
	for (Slice first(2), last(7); first != last; first.increment()) {
		std::printf("%.2fsec: ", TDIFF(t0));
		verify(&db1, &db2, first, 50000 / FAST_TEST_MULT);
	}

	egdb_close(db1.handle);
	egdb_close(db2.handle);

	std::printf("\nSelf-verify Tunstall v2 test.\n");
	if (identify(DB_TUN_V2, &db1))
		return(1);

	db1.handle = egdb_open("maxpieces=8", 3000, DB_TUN_V2, print_msgs);
	if (!db1.handle) {
		std::printf("Cannot open tun v2 db\n");
		return(1);
	}

	EGDB_INFO db;
	db.dbpieces = db1.max_pieces;
	db.handle = db1.handle;
	db.dbpieces_1side = 5;
	db.db8_kings_1side = 5;
	db.egdb_excludes_some_nonside_caps = true;
	t0 = std::clock();
	for (Slice first(2), last(9); first != last; first.increment()) {
		std::printf("%.2fsec: ", TDIFF(t0));
		self_verify(&db, first, 10000 / FAST_TEST_MULT);
	}
	egdb_close(db.handle);

	mtc_test();
	open_options_test();

	crc_verify_test(DB_TUN_V1);
	crc_verify_test(DB_TUN_V2);

#ifdef USE_MULTI_THREADING
	test_mutual_exclusion(DB_TUN_V1, get_tun_v1_lock());
	test_mutual_exclusion(DB_TUN_V2, get_tun_v2_lock());
#endif
}

