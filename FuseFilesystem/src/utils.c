#include "utils.h"

#include <time.h>
#include <string.h>

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

void format_filename(char* path, char formatted_filename[11]) {
	char* dot = strchr(path, '.');

    // Initialize formatted_filename to all '\0'
    memset(formatted_filename, '\0', 11);

    if (strcmp(path, "/") == 0) {
        // Root directory.
        return;
    }

    if (strlen(path + 1) > (MAX_FILENAME_LENGTH + strlen('.'))) {
        // File name too long.
        path = NULL;
        return;
    }
    
	if (dot == NULL) {
        // No dot. Not too long. Copy whole filename
		strncpy(formatted_filename, path + 1, strlen(path + 1));
	} else {
        // Has dot. Not too long. Parse filename
		int len_pre = dot - path;
        int len_post = strlen(dot + 1);

		if (len_pre > 8) {
            // Too many characters before the dot. invalid
            path = NULL;
            return;
        }
        if (len_post > 3) {
            // Too many characters after the dot. invalid
            path = NULL;
            return;
        }

        // Appropriate amount of characters before and after the dot
        // Format appropriately
        strncpy(formatted_filename, path + 1, len_pre);
        strncpy(formatted_filename + len_pre, dot + 1, len_post);
    }

    // Copy new string to path
    strncpy(path, formatted_filename, MAX_FILENAME_LENGTH);
}




