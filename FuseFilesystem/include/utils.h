#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

#include "memefs_file_entry.h"

// Type for write operations.
typedef enum write_type {
    OVERWRITE,
    APPEND,
    INVALID
} write_type_t;

// int append_file(const memefs_file_entry_t*, const char*, size_t)
// Description: Appends data to the end of a file.
// Preconditions: File exists.
// Postconditions: Data is appended to the end of the file.
// Returns: 0 on success, < 0 on failure.
int append_file(const memefs_file_entry_t* file_entry, const char* buf, size_t size);

// int check_legal_name(const char*)
// Description: Checks if a filename is legal.
// Preconditions: None.
// Postconditions: None.
// Returns: 0 on success, < 0 on failure.
int check_legal_name(const char* filename);

// void generate_memefs_timestamp(uint8_t*)
// Description: Generates a timestamp in BCD format.
// Preconditions: None.
// Postconditions: BCD timestamp is generated.
// Returns: None.
void generate_memefs_timestamp(uint8_t bcd_time[8]);

// time_t memefs_bcd_to_time(const uint8_t*)
// Description: Converts a BCD timestamp to a time_t.
// Preconditions: BCD timestamp is provided.
// Postconditions: time_t timestamp is generated.
// Returns: time_t timestamp.
time_t memefs_bcd_to_time(const uint8_t bcd_time[8]);

// double myCeil(double)
// Description: Returns the smallest integer greater than or equal to num.
// Preconditions: None.
// Postconditions: None.
// Returns: Smallest integer greater than or equal to num.
double myCeil(double num);

// void name_to_encoded(const char*, char*)
// Description: Converts a readable filename to a memefs encoded filename.
// Preconditions: Readable filename is provided.
// Postconditions: Encoded filename is generated.
// Returns: None.
void name_to_encoded(const char* readable_name, char* encoded_name);

// void name_to_readable(const char*, char*)
// Description: Converts a memefs encoded filename to a readable filename.
// Preconditions: Encoded filename is provided.
// Postconditions: Readable filename is generated.
// Returns: None.
void name_to_readable(const char* name, char* readable_name);

// int overwrite_file(memefs_file_entry_t*, const char*, size_t)
// Description: Overwrites a file.
// Preconditions: File exists.
// Postconditions: File data overwritten.
// Returns: 0 on success, < 0 on failure.
int overwrite_file(memefs_file_entry_t* file_entry, const char* buf, size_t size);

#endif // UTILS_H