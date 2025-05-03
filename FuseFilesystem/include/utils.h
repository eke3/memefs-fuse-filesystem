#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

// Helper functions for fuse operations
void generate_memefs_timestamp(uint8_t bcd_time[8]);
uint8_t to_bcd(uint8_t num);
double myCeil(double num);
int is_illegal_filename(char* filename);
int scrub_path(const char* path, char* clean_path);

// TODO: parsers for file names

#endif // UTILS_H