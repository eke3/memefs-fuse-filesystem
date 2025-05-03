#include "define.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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



int main() {

    char name[11] = "filenam\0md";
    char readable_name[MAX_FILENAME_LENGTH];
    name_to_readable(name, readable_name);
    printf("%s\n", readable_name);

    char encoded_name[MAX_FILENAME_LENGTH];
    name_to_encoded(readable_name, encoded_name);
    for (int i = 0; i < 11; i++) {
        printf("%c", (encoded_name[i] == '\0') ? '0' : encoded_name[i]);
    }




    return 0;
}