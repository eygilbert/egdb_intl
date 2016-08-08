#pragma once

namespace egdb_interface {

#define MAXSQUARE_BICOEF 50
#define MAXPIECES_BICOEF 8

extern unsigned int bicoef[MAXSQUARE_BICOEF + 1][MAXPIECES_BICOEF + 1];

void initbicoef(void);
}	// namespace egdb_interface
