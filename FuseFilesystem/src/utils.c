#include "utils.h"

#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#include "define.h"

extern uint16_t main_fat[MAX_FAT_ENTRIES];
extern uint16_t backup_fat[MAX_FAT_ENTRIES];
extern uint8_t user_data[USER_DATA_NUM_BLOCKS * BLOCK_SIZE];

uint8_t to_bcd(uint8_t num) {
	if (num > 99) return 0xFF;
	return ((num / 10) << 4) | (num % 10);
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

double myCeil(double num) {
    int whole_num;

    whole_num = (int)num;
    if (num - whole_num > 0) {
        // Number is greater than the whole part.
        return whole_num + 1;
    }
    return whole_num;
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

int overwrite_file(const memefs_file_entry_t* file_entry, const char* buf, size_t size, off_t offset) {


    

}

int append_file(const memefs_file_entry_t* file_entry, const char* buf, size_t size) {
    uint16_t fat_start_index = file_entry->start_block; // starting block INDEX
    uint16_t last_block_index = fat_start_index;
    int space_to_write;
    int append_start_index;
    int curr_blk_index;
    int i;
    off_t buffer_offset;
    size_t total_bytes_available;
    bool is_first_block;

    // check if there is enough space
    // count empty fat blocks
    total_bytes_available = (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE));
    for (i = 0; i < MAX_FAT_ENTRIES; i++) {
        if (main_fat[i] == 0x0000) {
            total_bytes_available += BLOCK_SIZE;
        }
    }

    // check if there is enough space on the disk to compleete the write
    printf("Total bytes available: %d\n", total_bytes_available);
    printf("Size: %zu\n", size);
    if (size > total_bytes_available) {
        return -ENOSPC;
    }



    printf("\n\n\nBuffer: %s\n\n\n", buf);
    printf("Size: %zu\n", size);



    while (main_fat[last_block_index] != 0xFFFF) {
        last_block_index = main_fat[last_block_index];
    }
    // now i have the index of the last fat block used. go to the START of the corresponding data block, which is this index * 512. Then add whatever is leftover from size % 512 to get the FIRST index of the data block to write to.
    append_start_index = (last_block_index * BLOCK_SIZE) + (file_entry->size % BLOCK_SIZE);
    // start at data[append_start_index]
    space_to_write = ((int)size < (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE))) ? size : (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE));

    is_first_block = true;
    buffer_offset = 0;
    while ((int)size > 0) {

        if (is_first_block) {
            space_to_write = ((int)size < (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE))) ? size : (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE));
            is_first_block = false;
        } else {
            space_to_write = ((int)size < BLOCK_SIZE) ? size : BLOCK_SIZE;
        }
        // fill the space in current block
        printf("string at buf+buffer_offset: %.*s\n", space_to_write, buf + buffer_offset);
        
        printf("space_to_write: %d\n", space_to_write);
        memset(user_data + append_start_index, '\0', space_to_write);
        memcpy(user_data + append_start_index, buf + buffer_offset, space_to_write);
        printf("size before decrment: %d\n", (int)size);

        size -= space_to_write;
        printf("size after decrement: %d\n", (int)size);

        buffer_offset += space_to_write;

        if ((int)size > 0) {
            // add a new fat block to the chain, updating last_block_index
            for (curr_blk_index = 0; curr_blk_index < MAX_FAT_ENTRIES; curr_blk_index++) {
                if (main_fat[curr_blk_index] == 0x0000) {
                    main_fat[last_block_index] = curr_blk_index;
                    main_fat[curr_blk_index] = 0xFFFF;
                    last_block_index = curr_blk_index;
                    break;
                }
            }

        
        // reset append_start_index to (last_block_index * BLOCK_SIZE)
        append_start_index = last_block_index * BLOCK_SIZE;
        // reduce size by space_to_write
        // printf("size before decrment: %d\n", (int)size);
        
        // size -= space_to_write;
        // printf("size after decrement: %d\n", (int)size);
        // move buffer_offset by space_to_write
        // buffer_offset += space_to_write;
        // reset space_to_write to 512
        // space_to_write = ((int)size < (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE))) ? size : (BLOCK_SIZE - (file_entry->size % BLOCK_SIZE));
        }



    }


    return 0;
}
