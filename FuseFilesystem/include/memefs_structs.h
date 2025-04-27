#ifndef MEMEFS_STRUCTS_H
#define MEMEFS_STRUCTS_H

#include <stdint.h>

// Structure representing a file entry in the directory.
typedef struct memefs_file_entry {
    uint16_t type_permissions; // File type and permissions
    uint16_t start_block;      // Starting block number
    char filename[11];         // Filename
    uint8_t unused;            // Unused byte
    uint8_t bcd_timestamp[8];  // Timestamp in BCD format
    uint32_t size;             // File size
    uint16_t uid_owner;        // User ID of owner
    uint16_t gid_owner;        // Group ID of owner
} __attribute__((packed)) memefs_file_entry_t;

// Structure representing the superblock metadata for the filesystem.
typedef struct memefs_superblock {
    char signature[16];        // Filesystem signature
    uint8_t cleanly_unmounted; // Flag for unmounted state
    uint8_t reseerved1[3];     // Reserved bytes
    uint32_t fs_version;       // Filesystem version
    uint8_t fs_ctime[8];       // Creation timestamp in BCD format
    uint16_t main_fat;         // Starting block for main FAT
    uint16_t main_fat_size;    // Size of the main FAT
    uint16_t backup_fat;       // Starting block for backup FAT
    uint16_t backup_fat_size;  // Size of the backup FAT
    uint16_t directory_start;  // Starting block for directory
    uint16_t directory_size;   // Directory size in blocks
    uint16_t num_user_blocks;  // Number of user data blocks
    uint16_t first_user_block; // First user data block
    char volume_label[16];     // Volume label
    uint8_t unused[448];       // Unused space for alignment
} __attribute__((packed)) memefs_superblock_t;

#endif // MEMEFS_STRUCTS_H