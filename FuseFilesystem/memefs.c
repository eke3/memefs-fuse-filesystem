// File:    
// Author:  Eric Ekey
// Date:    04/27/2025
// Desc:    

#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "include/memefs_structs.h"

#define BLOCK_SIZE 512
#define DIRECTORY_BEGIN 253
#define DIRECTORY_NUM_BLOCKS 14
#define FAT_BACKUP_BEGIN 239
#define FAT_MAIN_BEGIN 254
#define FILE_ENTRY_SIZE 32
#define MAX_FILE_ENTRIES 224
#define SIGNATURE "?MEMEFS++CMSC421"
#define SUPERBLOCK_BACKUP_BEGIN 0
#define SUPERBLOCK_MAIN_BEGIN 255



static memefs_superblock_t main_superblock;
static memefs_superblock_t backup_superblock;
static memefs_file_entry_t directory[MAX_FILE_ENTRIES];
static uint16_t main_fat[256];
static uint16_t backup_fat[256];

// File descriptor for the filesystem image.
static int img_fd;

// FUSE operations.
static int memefs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);
static int memefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags);
static int memefs_open(const char* path, struct fuse_file_info* fi);
static int memefs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);

int load_superblock();
int load_directory();
int load_fat();
int load_user_data();

static struct fuse_operations memefs_oper = {
    .getattr = memefs_getattr,
    .readdir = memefs_readdir,
    .open    = memefs_open,
    .read    = memefs_read,
};

int load_fat() {
    /* 
    
    TODO: Investigate funky numbers here.

    */
    // off_t fat_offset;
    
    // // Load main FAT.
    // fat_offset = (off_t)(FAT_MAIN_BEGIN * BLOCK_SIZE);
    // if (pread(img_fd, &main_fat, BLOCK_SIZE, fat_offset) != sizeof(main_fat)) {
    //     perror("Failed to read main FAT");
    //     return -1;
    // }

    // // Load backup FAT.
    // fat_offset = (off_t)(FAT_BACKUP_BEGIN * BLOCK_SIZE);
    // if (pread(img_fd, &backup_fat, BLOCK_SIZE, fat_offset) != sizeof(backup_fat)) {
    //     perror("Failed to read backup FAT");
    //     return -1;
    // }

    // printf("First values of the main FAT:\n");
    // for (int i = 0; i < 256; i++) {
    //     printf("%d: %d\n", i, main_fat[i]);
    // }

    // printf("Successfully loaded FATs\n");
    // return 0;
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
    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (pread(img_fd, &directory[i], FILE_ENTRY_SIZE, directory_offset) != FILE_ENTRY_SIZE) {
            perror("Failed to read directory entry");
            return -1;
        }
        directory_offset -= (off_t)FILE_ENTRY_SIZE;
    }
    printf("Successfully loaded directory\n");
    return 0;
}


static int memefs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void) fi;
    int i;

    memset(stbuf, 0, sizeof(struct stat));
    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (strcmp(directory[i].filename, path) == 0) {
            // Found file entry.
            /*

            TODO: set stbuf->st_mode

            */
            stbuf->st_uid = (uid_t)directory[i].uid_owner;
            stbuf->st_gid = (gid_t)directory[i].gid_owner;
            stbuf->st_size = (off_t)directory[i].size;
            stbuf->st_mtime = (time_t)directory[i].bcd_timestamp;
            return 0;
        }
    }
    return -1;
}

static int memefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    (void) path; 
    (void) offset;
    (void) fi;
    (void) flags;
    int i ;

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (directory[i].type_permissions != 0x0000) {
            // Found file entry.
            filler(buf, directory[i].filename, NULL, 0, 0);
        }
    }
    return 0;
}

static int memefs_open(const char* path, struct fuse_file_info* fi) {
    int i;

    for (i=0; i < MAX_FILE_ENTRIES; i++) {
        if ((strcmp(directory[i].filename, path) == 0) && (directory[i].type_permissions != 0x0000)) {
            // Found file entry.
            fi->fh = i;
            return 0;
        }
    }
    return -ENOENT;
}

static int memefs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {

    // locate file in fat table?

    // get start block

    // add data at start block to buffer

    // go to next block

    // append data to buffer

    // continue

    // return number of bytes in the buffer

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

    if (load_fat() < 0) {
        fprintf(stderr, "Failed to load FATs\n");
        close(img_fd);
        return 1;
    }

	return fuse_main(argc - 1, argv + 1, &memefs_oper, NULL);
}

