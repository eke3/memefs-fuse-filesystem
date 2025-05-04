#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

// Helper functions for fuse operations
void generate_memefs_timestamp(uint8_t bcd_time[8]);
uint8_t to_bcd(uint8_t num);
double myCeil(double num);
int check_legal_name(const char* filename);
void name_to_readable(const char* name, char* readable_name);
void name_to_encoded(const char* readable_name, char* encoded_name);

// TODO: parsers for file names

#endif // UTILS_H