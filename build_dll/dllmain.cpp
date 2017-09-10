#include "engine/bitcount.h"
#include "engine/reverse.h"
#include "engine/bicoef.h"
#include "builddb/indexing.h"
#include <Windows.h>

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		egdb_interface::init_bitcount();
		egdb_interface::init_reverse();
		egdb_interface::initbicoef();
		egdb_interface::build_man_index_base();
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}


