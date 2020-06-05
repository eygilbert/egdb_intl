#pragma once

#include "Engine/project.h"

namespace egdb_interface {

#define MAXSQUARE_BICOEF 50
#define MAXPIECES_BICOEF 8

extern unsigned int bicoef[MAXSQUARE_BICOEF + 1][MAXPIECES_BICOEF + 1];
extern bool did_initbicoef;

/*
 * Return the number of ways to choose k objects from a set of n objects.
 */
inline unsigned int choose(int n, int k)
{
	assert(did_initbicoef);
	return(bicoef[n][k]);
}

void initbicoef(void);
}	// namespace egdb_interface
