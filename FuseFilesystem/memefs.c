// File:    memefs.c
// Author:  Eric Ekey
// Date:    04/27/2025
// Desc:    

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include "memefs_file_entry.h"
#include "memefs_superblock.h"
#include "define.h"

static memefs_superblock_t main_superblock;
static memefs_superblock_t backup_superblock;
static memefs_file_entry_t directory[MAX_FILE_ENTRIES];
static uint16_t main_fat[256];
static uint16_t backup_fat[256];
static uint8_t user_data[USER_DATA_NUM_BLOCKS * BLOCK_SIZE];

// File descriptor for the filesystem image.
static int img_fd;

// FUSE operations.
static int memefs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);
static int memefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags);
static int memefs_open(const char* path, struct fuse_file_info* fi);
static int memefs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int memefs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi);
static int memefs_unlink(const char *path);

// Helper functions for loading data from the filesystem image.
int load_superblock();
int load_directory();
int load_fat();
int load_user_data();
void generate_memefs_timestamp(uint8_t bcd_time[8]);
uint8_t to_bcd(u_int8_t num);

static struct fuse_operations memefs_oper = {
    .getattr = memefs_getattr,
    .readdir = memefs_readdir,
    .open    = memefs_open,
    .read    = memefs_read,
    .create  = memefs_create,
    .utimens = memefs_utimens,
    .unlink  = memefs_unlink,
};

int load_user_data() {
    off_t data_offset;

    data_offset = (off_t)(USER_DATA_BEGIN * BLOCK_SIZE);
    if (pread(img_fd, &user_data, USER_DATA_NUM_BLOCKS * BLOCK_SIZE, data_offset) != (USER_DATA_NUM_BLOCKS * BLOCK_SIZE)) {
        perror("Failed to read user data");
        return -1;
    }

    printf("Successfully loaded user data\n");
    return 0;
}

int load_fat() {
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
    for (i = 0; i < 256; i++) {
        main_fat[i] = ntohs(main_fat[i]);
        backup_fat[i] = ntohs(backup_fat[i]);
    }

    printf("Successfully loaded FATs\n");
    return 0;
}

int load_superblock() {
    off_t superblock_offset;
    
    // Load main superblock.
    superblock_offset = (off_t)(SUPERBLOCK_MAIN_BEGIN * BLOCK_SIZE);
    if (pread(img_fd, &main_superblock, BLOCK_SIZE, superblock_offset) != sizeof(memefs_superblock_t)) {
        perror("Failed to read main superblock");
        return -1;
    }
    if (strcmp(main_superblock.signature, SIGNATURE) != 0) {
        fprintf(stderr, "Invalid filesystem signature from main superblock\n");
        return -1;
    }

    // Load backup superblock.
    superblock_offset = (off_t)(SUPERBLOCK_BACKUP_BEGIN * BLOCK_SIZE);
    if (pread(img_fd, &backup_superblock, BLOCK_SIZE, superblock_offset) != sizeof(memefs_superblock_t)) {
        perror("Failed to read backup superblock");
        return -1;
    }
    if (strcmp(backup_superblock.signature, SIGNATURE) != 0) {
        fprintf(stderr, "Invalid filesystem signature from backup superblock\n");
        return -1;
    }

    printf("Successfully loaded superblocks\n");
    return 0;
}

int load_directory() {
    int i;
    off_t directory_offset;
    
    // Load directory entries from bottom (253) to top (240)
    directory_offset = (off_t)(DIRECTORY_BEGIN * BLOCK_SIZE);
    for (i = MAX_FILE_ENTRIES - 1; i >= 0; i--) {
        if (pread(img_fd, &directory[i], FILE_ENTRY_SIZE, directory_offset) != FILE_ENTRY_SIZE) {
            perror("Failed to read directory entry");
            return -1;
        }
        directory_offset -= (off_t)FILE_ENTRY_SIZE;
    }

    printf("Successfully loaded directory\n");
    return 0;
}

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

static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    (void) mode;
    int i, j;

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if ((strcmp(directory[i].filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000)) {
            // File already exists.
            return -EEXIST;
        }
    }

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (directory[i].type_permissions == 0x0000) {
            // Found free file entry in directory.
            for (j = 0; j < 256; j++) {
                if (main_fat[j] == 0x0000) {
                    // Found free FAT entry.
                    strcpy(directory[i].filename, path + 1);
                    directory[i].type_permissions = mode; // TODO: should i make sure this sets correct permissions instead of all?
                    directory[i].start_block = j;
                    generate_memefs_timestamp(directory[i].bcd_timestamp);
                    directory[i].uid_owner = 0; // TODO: should these owners be set to current user or root?
                    directory[i].gid_owner = 0;
                    directory[i].size = 0;
                    main_fat[j] = 0xFFFF;
                    return 0;
                }
            }
        }
    }

    return -ENOSPC;
}

static int memefs_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi) {
    (void) path;
    (void) tv;
    (void) fi;
    return 0;
}

static int memefs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void) fi;
    int i;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        // Root directory or "." or ".."
        stbuf->st_mode = (mode_t)(S_IFDIR | 0755);
        stbuf->st_nlink = (nlink_t)2;
        return 0;
    }

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (strcmp(directory[i].filename, path + 1) == 0) {
            // Found file.
            stbuf->st_mode = (mode_t)(S_IFREG | 0777);
            stbuf->st_nlink = (nlink_t)1;
            stbuf->st_uid = (uid_t)directory[i].uid_owner;
            stbuf->st_gid = (gid_t)directory[i].gid_owner;
            stbuf->st_size = (off_t)directory[i].size;
            stbuf->st_mtime = (time_t)directory[i].bcd_timestamp;
            return 0;
        }
    }

    return -ENOENT;
}

static int memefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;
    int i;

    if (strcmp(path, "/") != 0) {
        // Not root directory.
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    // Add fake file for testing.
    filler(buf, "bullshitfile", NULL, 0, 0);

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (directory[i].type_permissions != 0x0000
            && strlen(directory[i].filename) > 0
            && directory[i].filename[0] != '\0') {
            // Found file.
            filler(buf, directory[i].filename, NULL, 0, 0);
        }
    }

    return 0;
}

static int memefs_open(const char* path, struct fuse_file_info* fi) {
    (void) fi;
    int i;

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if ((strcmp(directory[i].filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000)) {
            // Found file entry.
            return 0;
        }
    }
    
    if (strcmp(path, "/") == 0) {
        // Found root directory.
        return 0;
    }

    return -ENOENT;
}

static int memefs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) size;
    (void) offset;
    (void) fi;
    int i, curr_block, done;
    uint32_t file_size, bytes_read;
    off_t buffer_offset;

    // locate file in directory
    for (i = 0, curr_block = 0xFFFF; i < MAX_FILE_ENTRIES; i++) {
        if ((strcmp(directory[i].filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000)) {
            // Found file entry.
            curr_block = directory[i].start_block;
            file_size = directory[i].size;
            break;
        }
    }

    if (curr_block == 0xFFFF) {
        // File not found.
        return -ENOENT;
    }

    // read file data
    for (done = 0, buffer_offset = 0, bytes_read = 0; !done; curr_block = main_fat[curr_block]) {
        // for current, loop one byte at a time copying block to buffer
        for (i = 0; i < BLOCK_SIZE; i++) {
            if (bytes_read == file_size) {
                done = 1;
                break;
            }
            memcpy(buf + buffer_offset, &user_data[curr_block * BLOCK_SIZE], 1);
            buffer_offset++;
            bytes_read++;
        }

        // check if reached end of file
        if (main_fat[curr_block] == 0xFFFF) {
            done = 1;
        }
    }

    // points to start block in fat 

    // add data at start block to buffer

    // go to next block

    // append data to buffer

    // continue until 0xffff

    // return number of bytes in the buffer 

    return bytes_read;
}

static int memefs_unlink(const char *path) {
    int i;
    uint16_t curr_block, next_block;

    //find file in directory
    for (i = 0, curr_block = 0xFFFF; i < MAX_FILE_ENTRIES; i++) {
        if ((strcmp(directory[i].filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000)) {
            // Found file entry.
            curr_block = directory[i].start_block;
            break;
        }
    }

    if (curr_block == 0xFFFF) {
        // File not found.
        return -ENOENT;
    }

    // Unlink file from FAT.
    for (next_block = 0xFFFF; curr_block != 0xFFFF; curr_block = next_block) {
        next_block = main_fat[curr_block];
        main_fat[curr_block] = 0x0000;
    }
    directory[i].type_permissions = 0x0000;

    return 0;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
    	fprintf(stderr, "Usage: %s <filesystem image> <mount point>\n", argv[0]);
    	return 1;
	}

	// Open filesystem image
	img_fd = open(argv[1], O_RDWR);
	if (img_fd < 0) {
    	perror("Failed to open filesystem image");
    	return 1;
	}

	// HINT: Define helper functions: load_superblock and load_directory
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

	return fuse_main(argc - 1, argv + 1, &memefs_oper, NULL);
}

