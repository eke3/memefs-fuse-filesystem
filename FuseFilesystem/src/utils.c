#include "utils.h"

#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "define.h"
#include "memefs_file_entry.h"

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

int append_file(const memefs_file_entry_t* file_entry, const char* buf, size_t size, off_t offset) {



}
