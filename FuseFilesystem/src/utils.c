#include "utils.h"

#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "define.h"

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

int scrub_path(const char* path, char* clean_path) {
    memset(clean_path, '/', 1);
    memset(clean_path + 1, '\0', MAX_FILENAME_LENGTH);

    if (strcmp(path, "/") == 0) {
        // Root directory.
        return 0;
    }

    char* last_dot = strrchr(path, '.');
    if (last_dot == NULL) {
        // No file extension.
        if (strlen(path + 1) > MAX_FILENAME_LENGTH) {
            // File name too long.
            return -ENAMETOOLONG;
        }
        memcpy(clean_path + 1, path + 1, strlen(path + 1));
        return 0;
    }

    if (strlen(last_dot + 1) > 3) {
        // File extension too long.
        return -ENAMETOOLONG;
    }

    if ((strlen(path) - strlen(last_dot)) > MAX_FILENAME_LENGTH - 4) {
        // File name too long.
        return -ENAMETOOLONG;
    }

    // Format properly.
    memcpy(clean_path + 1, path + 1, strlen(path + 1) - strlen(last_dot));
    memcpy(clean_path + 1 + MAX_FILENAME_LENGTH - strlen(last_dot + 1), last_dot + 1, strlen(last_dot + 1));
    return 0;
}

void name_to_readable(const char* name, char* readable_name) {
    char filename[9];
    char extension[4];

    memset(filename, '\0', 9);
    memset(extension, '\0', 4);
    memset(readable_name, '\0', MAX_FILENAME_LENGTH);

    memcpy(filename, name, 8);
    memcpy(extension, name + 8, 3);

    sprintf(readable_name, "%s.%s", filename, extension);
}

void name_to_encoded(const char* readable_name, char* encoded_name) {
    char filename[9];
    char extension[4];
    int i;

    memset(encoded_name, '\0', MAX_FILENAME_LENGTH);
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
