// File:    loaders.c
// Author:  Eric Ekey
// Date:    05/08/2025
// Desc:    Functions for loading and unloading the filesystem image.

#include "loaders.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "define.h"
#include "memefs_file_entry.h"
#include "memefs_superblock.h"

int img_fd; // Filesystem image file descriptor.
memefs_superblock_t main_superblock;
memefs_superblock_t backup_superblock;
memefs_file_entry_t directory[MAX_FILE_ENTRIES];
uint16_t main_fat[MAX_FAT_ENTRIES];
uint16_t backup_fat[MAX_FAT_ENTRIES];
uint8_t user_data[USER_DATA_NUM_BLOCKS * BLOCK_SIZE];

#pragma region Prototypes

// static int load_directory()
// Description: Loads the directory fromm the filesystem image into memory.
// Preconditions: Image exists.
// Postconditions: Directory is loaded into memory.
// Returns: 0 on success, -1 on failure.
static int load_directory();

// static int load_fat()
// Description: Loads the FATs from the filesystem image into memory.
// Preconditions: Image exists.
// Postconditions: FATs are loaded into memory.
// Returns: 0 on success, -1 on failure.
static int load_fat();

// static int load_superblock()
// Description: Loads the superblocks from the filesystem image into memory.
// Preconditions: Image exists.
// Postconditions: Superblocks are loaded into memory.
// Returns: 0 on success, -1 on failure.
static int load_superblock();

// static int load_user_data()
// Description: Loads the user data from the filesystem image into memory.
// Preconditions: Image exists.
// Postconditions: User data is loaded into memory.
// Returns: 0 on success, -1 on failure.
static int load_user_data();

// static int unload_directory()
// Description: Unloads the directory from memory into the filesystem image.
// Preconditions: Directory exists in memory.
// Postconditions: Directory is rewritten on image from memory.
// Returns: 0 on success, -1 on failure.
static int unload_directory();

// static int unload_fat()
// Description: Unloads the FATs from memory into the filesystem image.
// Preconditions: FATs exist in memory.
// Postconditions: FATs are rewritten on image from memory.
// Returns: 0 on success, -1 on failure.
static int unload_fat();

// static int unload_superblock()
// Description: Unloads the superblocks from memory into the filesystem image.
// Preconditions: Superblocks exist in memory.
// Postconditions: Superblocks are rewritten on image from memory.
// Returns: 0 on success, -1 on failure.
static int unload_superblock();

// static int unload_user_data()
// Description: Unloads the user data from memory into the filesystem image.
// Preconditions: User data exists in memory.
// Postconditions: User data is rewritten on image from memory.
// Returns: 0 on success, -1 on failure.
static int unload_user_data();

#pragma endregion Prototypes

#pragma region Implementations

static int load_directory() {
    off_t directory_offset;
    int i;
    
    // Load directory entries from bottom (253) to top (240)
    directory_offset = (off_t)(DIRECTORY_BEGIN * BLOCK_SIZE);
    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (pread(img_fd, &directory[i], FILE_ENTRY_SIZE, directory_offset) != FILE_ENTRY_SIZE) {
            perror("Failed to read directory entry");
            return -1;
        }
        directory_offset += (off_t)FILE_ENTRY_SIZE;
    }

    printf("Successfully loaded directory\n");

    return 0;
}

static int load_fat() {
    off_t fat_offset;
    int i;
    
    // Load main FAT.
    fat_offset = (off_t)(FAT_MAIN_BEGIN * BLOCK_SIZE);
    if (pread(img_fd, &main_fat, BLOCK_SIZE, fat_offset) != sizeof(main_fat)) {
        perror("Failed to read main FAT");
        return -1;
    }

    // Load backup FAT.
    fat_offset = (off_t)(FAT_BACKUP_BEGIN * BLOCK_SIZE);
    if (pread(img_fd, &backup_fat, BLOCK_SIZE, fat_offset) != sizeof(backup_fat)) {
        perror("Failed to read backup FAT");
        return -1;
    }

    // Convert FAT entries from network byte order to host byte order.
    for (i = 0; i < MAX_FAT_ENTRIES; i++) {
        main_fat[i] = ntohs(main_fat[i]);
        backup_fat[i] = ntohs(backup_fat[i]);
    }

    if (MAX_FAT_ENTRIES > 0) {
        printf("First entry in main FAT: 0x%04x\n", main_fat[0]);
        printf("Second entry in main FAT: 0x%04x\n", main_fat[1]);
    }


    printf("Successfully loaded FATs\n");
    return 0;
}

int load_image() {
	if (load_superblock() < 0 || load_directory() < 0) {
    	fprintf(stderr, "Failed to load superblock or directory\n");
    	close(img_fd);
    	return 1;
	}
    if (load_fat() < 0 || load_user_data() < 0) {
        fprintf(stderr, "Failed to load FATs or user data\n");
        close(img_fd);
        return 1;
    }

    main_superblock.cleanly_unmounted = 0xFF;
    backup_superblock.cleanly_unmounted = 0xFF;
    return 0;
}

static int load_superblock() {
    off_t superblock_offset;
    
    // Load main superblock.
    superblock_offset = (off_t)(SUPERBLOCK_MAIN_BEGIN * BLOCK_SIZE);
    if (pread(img_fd, &main_superblock, BLOCK_SIZE, superblock_offset) != sizeof(memefs_superblock_t)) {
        perror("Failed to read main superblock");
        return -1;
    }
    if (strncmp(main_superblock.signature, SIGNATURE, strlen(SIGNATURE)) != 0) {
        fprintf(stderr, "Invalid filesystem signature from main superblock\n%s\n", main_superblock.signature);
        return -1;
    }
    fprintf(stderr, "main superblock signature: %s\n", main_superblock.signature);

    // Load backup superblock.
    superblock_offset = (off_t)(SUPERBLOCK_BACKUP_BEGIN * BLOCK_SIZE);
    if (pread(img_fd, &backup_superblock, BLOCK_SIZE, superblock_offset) != sizeof(memefs_superblock_t)) {
        perror("Failed to read backup superblock");
        return -1;
    }
    if (strncmp(backup_superblock.signature, SIGNATURE, strlen(SIGNATURE)) != 0) {
        fprintf(stderr, "Invalid filesystem signature from backup superblock\n");
        return -1;
    }

    memset(main_superblock.reserved1, 0x00, sizeof(main_superblock.reserved1));
    memset(backup_superblock.reserved1, 0x00, sizeof(backup_superblock.reserved1));
    memset(main_superblock.unused, 0x00, sizeof(main_superblock.unused));
    memset(backup_superblock.unused, 0x00, sizeof(backup_superblock.unused));
    main_superblock.cleanly_unmounted = 0x00;
    backup_superblock.cleanly_unmounted = 0x00;
    printf("Successfully loaded superblocks\n");
    return 0;
}

static int load_user_data() {
    off_t data_offset;

    data_offset = (off_t)(USER_DATA_BEGIN * BLOCK_SIZE);
    if (pread(img_fd, &user_data, USER_DATA_NUM_BLOCKS * BLOCK_SIZE, data_offset) != (USER_DATA_NUM_BLOCKS * BLOCK_SIZE)) {
        perror("Failed to read user data");
        return -1;
    }

    printf("Successfully loaded user data\n");
    return 0;
}

static int unload_directory() {
    off_t directory_offset;
    int i;

    // Unload directory entries from bottom (253) to top (240)
    directory_offset = (off_t)(DIRECTORY_BEGIN * BLOCK_SIZE);
    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (pwrite(img_fd, &directory[i], FILE_ENTRY_SIZE, directory_offset) != FILE_ENTRY_SIZE) {
            perror("Failed to write directory entry");
            return -1;
        }
        directory_offset += (off_t)FILE_ENTRY_SIZE;
    }

    // int i;
    // char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    // for (i = 0; i < MAX_FILE_ENTRIES; i++) {
    //     name_to_readable(directory[i].filename, readable_filename);
    //     printf("File %d: %s\n", i, readable_filename);
    // }

    // printf("Successfully unloaded directory\n");
    return 0;
}

static int unload_fat() { 
    off_t fat_offset;
    int i;
    
    // Convert FAT entries from host byte order to network byte order.
    for (i = 0; i < MAX_FAT_ENTRIES; i++) {
        main_fat[i] = htons(main_fat[i]);
        backup_fat[i] = htons(backup_fat[i]);
    }

    // Load main FAT.
    fat_offset = (off_t)(FAT_MAIN_BEGIN * BLOCK_SIZE);
    if (pwrite(img_fd, &main_fat, BLOCK_SIZE, fat_offset) != sizeof(main_fat)) {
        perror("Failed to write main FAT");
        return -1;
    }

    // Load backup FAT.
    fat_offset = (off_t)(FAT_BACKUP_BEGIN * BLOCK_SIZE);
    if (pwrite(img_fd, &backup_fat, BLOCK_SIZE, fat_offset) != sizeof(backup_fat)) {
        perror("Failed to write backup FAT");
        return -1;
    }

    // Convert local FAT entries from network byte order back to host byte order.
    for (i = 0; i < MAX_FAT_ENTRIES; i++) {
        main_fat[i] = ntohs(main_fat[i]);
        backup_fat[i] = ntohs(backup_fat[i]);
    }

    printf("Successfully unloaded FAT blocks\n");
    return 0;
}

int unload_image() {
    if (unload_user_data() < 0 || unload_fat() < 0 || unload_directory() < 0 || unload_superblock() < 0) {
        return -1;
    }
    return 0;
}

static int unload_superblock() {
    off_t superblock_offset;
    
    // Load main superblock.
    superblock_offset = (off_t)(SUPERBLOCK_MAIN_BEGIN * BLOCK_SIZE);
    if (pwrite(img_fd, &main_superblock, BLOCK_SIZE, superblock_offset) != sizeof(memefs_superblock_t)) {
        perror("Failed to write main superblock");
        return -1;
    }

    // Load backup superblock.
    superblock_offset = (off_t)(SUPERBLOCK_BACKUP_BEGIN * BLOCK_SIZE);
    if (pwrite(img_fd, &backup_superblock, BLOCK_SIZE, superblock_offset) != sizeof(memefs_superblock_t)) {
        perror("Failed to write backup superblock");
        return -1;
    }

    printf("Successfully unloaded superblocks\n");
    return 0;
}

static int unload_user_data() {
    off_t data_offset;

    data_offset = (off_t)(USER_DATA_BEGIN * BLOCK_SIZE);
    if (pwrite(img_fd, &user_data, USER_DATA_NUM_BLOCKS * BLOCK_SIZE, data_offset) != (USER_DATA_NUM_BLOCKS * BLOCK_SIZE)) {
        perror("Failed to write user data");
        return -1;
    }

    printf("Successfully unloaded user data\n");
    return 0;
}

#pragma endregion Implementations