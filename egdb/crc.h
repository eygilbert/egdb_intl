#pragma once

unsigned int crc_calc(char *buf, int len);
unsigned int file_crc_calc(FILE *fp, int *abort);
int fname_crc_calc(char *name, unsigned int *crc);
