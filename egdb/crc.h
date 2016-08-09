#pragma once
#include <cstdio>

namespace egdb_interface {

unsigned int crc_calc(char const *buf, int len);
unsigned int file_crc_calc(FILE *fp, int *abort);
int fname_crc_calc(char const *name, unsigned int *crc);
}	// namespace egdb_interface
