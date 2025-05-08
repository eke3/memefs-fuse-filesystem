#ifndef MEMEFS_FILE_ENTRY_H
#define MEMEFS_FILE_ENTRY_H

#include <stdint.h>

#include "define.h"

// Struct representing a file entry in the directory.
typedef struct memefs_file_entry {
    uint16_t type_permissions; // File type and permissions
    uint16_t start_block;      // Starting block number
    char filename[MAX_ENCODED_FILENAME_LENGTH];         // Filename
    uint8_t unused;            // Unused byte
    uint8_t bcd_timestamp[8];  // Timestamp in BCD format
    uint32_t size;             // File size
    uint16_t uid_owner;        // User ID of owner
    uint16_t gid_owner;        // Group ID of owner
} __attribute__((packed)) memefs_file_entry_t;

#endif // MEMEFS_FILE_ENTRY_H