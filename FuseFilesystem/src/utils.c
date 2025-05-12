// File:    utils.c
// Author:  Eric Ekey
// Date:    05/08/2025
// Desc:    Utility functions for memefs filesystem.

#include "utils.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "define.h"

extern uint16_t main_fat[MAX_FAT_ENTRIES];
extern uint16_t backup_fat[MAX_FAT_ENTRIES];
extern uint8_t user_data[USER_DATA_NUM_BLOCKS * BLOCK_SIZE];

// static void clear_fat_chain(const memefs_file_entry_t*)
// Description: Clears the FAT chain for a given file entry.
// Preconditions: File entry exists.
// Postconditions: FAT chain is cleared.
// Returns: None.
static void clear_fat_chain(const memefs_file_entry_t* file_entry);

// static int from_bcd(uint8_t)
// Description: Converts a byte from BCD format to decimal.
// Preconditions: None.
// Postconditions: None.
// Returns: Decimal value.
static int from_bcd(uint8_t bcd);

// static uint8_t to_bcd(uint8_t)
// Description: Converts a byte from decimal to BCD format.
// Preconditions: None.
// Postconditions: None.
// Returns: BCD value.
static uint8_t to_bcd(uint8_t num);

int append_file(const memefs_file_entry_t* file_entry, const char* buf, size_t size) {
    uint16_t last_block_index;
    int space_to_write;
    int append_start_index;
    int curr_blk_index;
    int i;
    off_t buffer_offset;
    bool is_first_block;
    int free_fat_blocks;

    for (i = 0, free_fat_blocks = 0; i < MAX_FAT_ENTRIES; i++) {
        if (main_fat[i] == 0x0000) {
            free_fat_blocks++;
        }
    }

    last_block_index = file_entry->start_block;
    while (main_fat[last_block_index] != 0xFFFF) {
        last_block_index = main_fat[last_block_index];
    }
    append_start_index = (last_block_index * BLOCK_SIZE) + (file_entry->size % BLOCK_SIZE);
    space_to_write = ((int)size < (BLOCK_SIZE - ((int)file_entry->size % BLOCK_SIZE))) ? size : (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE));

    is_first_block = true;
    buffer_offset = 0;
    while ((int)size > 0) {
        if (free_fat_blocks == 0) {
            // No more free FAT blocks. Disk is full, cannot continue writing.
            return -ENOSPC;
        }

        if (is_first_block) {
            space_to_write = ((int)size < (BLOCK_SIZE - ((int)file_entry->size % BLOCK_SIZE))) ? size : (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE));
            is_first_block = false;
        } else {
            space_to_write = ((int)size < BLOCK_SIZE) ? size : BLOCK_SIZE;
        }

        // fill the space in current block        
        memset(user_data + append_start_index, '\0', space_to_write);
        memcpy(user_data + append_start_index, buf + buffer_offset, space_to_write);
        size -= space_to_write;
        buffer_offset += space_to_write;

        if ((int)size > 0) {
            // add a new fat block to the chain, updating last_block_index
            for (curr_blk_index = 0; curr_blk_index < MAX_FAT_ENTRIES; curr_blk_index++) {
                if (main_fat[curr_blk_index] == 0x0000) {
                    main_fat[last_block_index] = curr_blk_index;
                    backup_fat[last_block_index] = curr_blk_index;
                    main_fat[curr_blk_index] = 0xFFFF;
                    backup_fat[curr_blk_index] = 0xFFFF;
                    last_block_index = curr_blk_index;
                    free_fat_blocks--;
                    break;
                }
            }

            // reset append_start_index to (last_block_index * BLOCK_SIZE)
            append_start_index = last_block_index * BLOCK_SIZE;
        }
    }

    return 0;
}

int check_legal_name(const char* filename) {
    int i, j;

    if (strlen(filename) >= MAX_READABLE_FILENAME_LENGTH) {
        // File name too long.
        return -ENAMETOOLONG;
    }

    // Check filename for illegal characters or too long.
    for (i = 0; ((filename)[i] != '.') && ((filename)[i] != '\0'); i++) {
        if (i > 7) {
            // Too many characters before '.'
            return -ENAMETOOLONG;
        }

        if (!isalnum((filename)[i]) 
            && ((filename)[i] != '^') 
            && ((filename)[i] != '-') 
            && ((filename)[i] != '_') 
            && ((filename)[i] != '=') 
            && ((filename)[i] != '|')) {
            // Invalid character in file name.
            return -EINVAL;
        }
    }

    if ((filename)[i] == '\0') {
        // No '.' in filename.
        return -EINVAL;
    }

    // Check extension for illegal characters or too long.
    for (j = 0; (filename + i + 1)[j] != '\0'; j++) {
        if (j > 2) {
            // Too many characters after '.'
            return -ENAMETOOLONG;
        }

        if (!isalnum((filename + i + 1)[j]) 
            && ((filename + i + 1)[j] != '^') 
            && ((filename + i + 1)[j] != '-') 
            && ((filename + i + 1)[j] != '_') 
            && ((filename + i + 1)[j] != '=') 
            && ((filename + i + 1)[j] != '|')) {
            // Invalid character in file extension.
            return -EINVAL;
        }
    }

    return 0;
}

static void clear_fat_chain(const memefs_file_entry_t* file_entry) {
    uint16_t fat_start_index, curr_blk_index, next_blk_index;

    fat_start_index = file_entry->start_block;
    for (curr_blk_index = fat_start_index, next_blk_index = main_fat[fat_start_index]; curr_blk_index != 0xFFFF; curr_blk_index = next_blk_index) {
        next_blk_index = main_fat[curr_blk_index];
        main_fat[curr_blk_index] = 0x0000;
        backup_fat[curr_blk_index] = 0x0000;
    }
    main_fat[fat_start_index] = 0xFFFF;
    backup_fat[fat_start_index] = 0xFFFF;
}

static int from_bcd(uint8_t bcd) {
    // Convert a single BCD byte to a binary integer (e.g., 0x42 -> 42)
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void generate_memefs_timestamp(uint8_t bcd_time[8]) {
	time_t now = time(NULL);
	struct tm utc;
	gmtime_r(&now, &utc); // UTC time (MEMEfs uses UTC, not localtime)

	int full_year = utc.tm_year + 1900;
	bcd_time[0] = to_bcd(full_year / 100); 	// Century
	bcd_time[1] = to_bcd(full_year % 100); 	// Year within century
	bcd_time[2] = to_bcd(utc.tm_mon + 1);  	// Month (0-based in tm)
	bcd_time[3] = to_bcd(utc.tm_mday);     	// Day
	bcd_time[4] = to_bcd(utc.tm_hour);     	// Hour
	bcd_time[5] = to_bcd(utc.tm_min);      	// Minute
	bcd_time[6] = to_bcd(utc.tm_sec);      	// Second
	bcd_time[7] = 0x00;                         	// Unused (reserved)
}

time_t memefs_bcd_to_time(const uint8_t bcd_time[8]) {
    struct tm utc_tm;

    utc_tm.tm_year = from_bcd(bcd_time[0]) * 100 + from_bcd(bcd_time[1]) - 1900;
    utc_tm.tm_mon  = from_bcd(bcd_time[2]) - 1;
    utc_tm.tm_mday = from_bcd(bcd_time[3]);
    utc_tm.tm_hour = from_bcd(bcd_time[4]);
    utc_tm.tm_min  = from_bcd(bcd_time[5]);
    utc_tm.tm_sec  = from_bcd(bcd_time[6]);
    utc_tm.tm_isdst = 0;

    return timegm(&utc_tm);
}

double myCeil(double num) {
    int whole_num;

    whole_num = (int)num;
    if (num - whole_num > 0) {
        // Number is greater than the whole part.
        return whole_num + 1;
    }

    return whole_num;
}

void name_to_encoded(const char* readable_name, char* encoded_name) {
    char filename[9];
    char extension[4];
    int i;

    memset(encoded_name, '\0', MAX_ENCODED_FILENAME_LENGTH);
    memset(filename, '\0', 9);
    memset(extension, '\0', 4);

    // Get index of '.'
    for (i = 0; i < 8; i++) {
        if (readable_name[i] == '.') {
            break;
        }
    }

    memcpy(filename, readable_name, i);
    memcpy(extension, readable_name + i + 1, 3);

    memcpy(encoded_name, filename, 8);
    memcpy(encoded_name + 8, extension, 3);
}

void name_to_readable(const char* name, char* readable_name) {
    char filename[9];
    char extension[4];

    memset(filename, '\0', 9);
    memset(extension, '\0', 4);
    memset(readable_name, '\0', MAX_READABLE_FILENAME_LENGTH);

    memcpy(filename, name, 8);
    strcpy(extension, name + 8);

    snprintf(readable_name, MAX_READABLE_FILENAME_LENGTH, "%s.%s", filename, extension);
}

int overwrite_file(memefs_file_entry_t* file_entry, const char* buf, size_t size) {
    clear_fat_chain(file_entry);
    file_entry->size = 0;
    return append_file(file_entry, buf, size);
}

static uint8_t to_bcd(uint8_t num) {
	if (num > 99) return 0xFF;
	return ((num / 10) << 4) | (num % 10);
}


