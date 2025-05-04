#include "define.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

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
int check_legal_name(const char* path) {
    int i, j;

    printf("strlen %d\n", strlen(path + 1));
    if (strlen(path + 1) >= MAX_READABLE_FILENAME_LENGTH) {
        // File name too long.
        printf("catch path too long\n");
        return -ENAMETOOLONG;
    }

    // Check filename for illegal characters or too long.
    for (i = 0; ((path + 1)[i] != '.') && ((path + 1)[i] != '\0'); i++) {
        if (i > 7) {
            // Too many characters before '.'
            printf("catch too many chars before dot\n");
            return -ENAMETOOLONG;
        }

        if (!isalnum((path + 1)[i]) && ((path + 1)[i] != '^') && ((path + 1)[i] != '-') && ((path + 1)[i] != '_') && ((path + 1)[i] != '=') && ((path + 1)[i] != '|')) {
            // Invalid character in file name.
            printf("catch bad char in filename\n");
            return -EINVAL;
        }
    }

    if ((path + 1)[i] == '\0') {
        // No '.' in filename.
        printf("catch no dot\n");
        return -EINVAL;
    }

    // Check extension for illegal characters or too long.
    for (j = 0; (path + 1 + i + 1)[j] != '\0'; j++) {
        if (j > 2) {
            // Too many characters after '.'
            printf("catch too many chars after dot\n");
            return -ENAMETOOLONG;
        }

        if (!isalnum((path + 1 + i + 1)[j]) && ((path + 1 + i + 1)[j] != '^') && ((path + 1 + i + 1)[j] != '-') && ((path + 1 + i + 1)[j] != '_') && ((path + 1 + i + 1)[j] != '=') && ((path + 1 + i + 1)[j] != '|')) {
            // Invalid character in file extension.
            printf("catch bad char in extension\n");
            return -EINVAL;
        }
    }

    return 0;
}

int main() {

    // char name[11] = "filenam\0md";
    // char readable_name[MAX_READABLE_FILENAME_LENGTH];
    // name_to_readable(name, readable_name);
    // printf("%s\n", readable_name);

    // char encoded_name[MAX_ENCODED_FILENAME_LENGTH];
    // name_to_encoded(readable_name, encoded_name);
    // for (int i = 0; i < 11; i++) {
    //     printf("%c", (encoded_name[i] == '\0') ? '0' : encoded_name[i]);
    // }


    char* path = "/nametoolo.ng";
    char* path2 = "/ext.toolong";
    char* path3 = "/pathtoolong.txt";
    char* path4 = "/bad(name.txt";
    char* path5 = "/bade.x(t";
    char* path6 = "/valid.txt";
    char* path7 = "/nodot";
    char* path8 = "/goodname.md"; 
    char* path9 = "/longnamew.txt";
    char* path10 = "/longnamewithdot.txt";
    printf("%d\n", check_legal_name(path));
    printf("%d\n", check_legal_name(path2));
    printf("%d\n", check_legal_name(path3));
    printf("%d\n", check_legal_name(path4));
    printf("%d\n", check_legal_name(path5));
    printf("%d\n", check_legal_name(path6));
    printf("%d\n", check_legal_name(path7));
    printf("%d\n", check_legal_name(path8));
    printf("%d\n", check_legal_name(path9));
    printf("%d\n", check_legal_name(path10));


    return 0;
}