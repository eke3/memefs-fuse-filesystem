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
#include "utils.h"

#pragma region Globals

extern memefs_superblock_t main_superblock;
extern memefs_superblock_t backup_superblock;
extern memefs_file_entry_t directory[MAX_FILE_ENTRIES];
extern uint16_t main_fat[MAX_FAT_ENTRIES];

/* 

    NOTE: when do we use backup fat? theres no indicator for if its corrupted

*/

extern uint16_t backup_fat[MAX_FAT_ENTRIES];
extern uint8_t user_data[USER_DATA_NUM_BLOCKS * BLOCK_SIZE];
extern int img_fd;

#pragma endregion Globals

#pragma region FUSE Prototypes

// FUSE operations.
static int memefs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);
static int memefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags);
static int memefs_open(const char* path, struct fuse_file_info* fi);
static int memefs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int memefs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi);
static int memefs_unlink(const char *path);
static int memefs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int memefs_truncate(const char* path, off_t new_size, struct fuse_file_info* fi);
static void memefs_destroy(void* private_data);

#pragma endregion FUSE Prototypes

#pragma region Helpers
// Helper functions for loading and unloading data from the filesystem image.
extern int load_image();
extern int unload_image();

// Utility functions
extern double myCeil(double num);
extern void name_to_readable(const char* name, char* readable_name);
extern void name_to_encoded(const char* readable_name, char* encoded_name);
extern int check_legal_name(const char* filename);

#pragma endregion Helpers

#pragma region FUSE Implementations

static struct fuse_operations memefs_oper = {
    .getattr = memefs_getattr,
    .readdir = memefs_readdir,
    .open    = memefs_open,
    .read    = memefs_read,
    .create  = memefs_create,
    .utimens = memefs_utimens,
    .unlink  = memefs_unlink,
    .write = memefs_write,
    .truncate = memefs_truncate,
    .destroy = memefs_destroy,
};

static int memefs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    (void) fi;
    (void) mode;
    char encoded_filename[MAX_ENCODED_FILENAME_LENGTH];
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i, j, name_legal;

    if ((name_legal = check_legal_name(path + 1)) != 0) {
        // File name is not legal.
        return name_legal;
    }

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        // fprintf(stderr, "\n\npath: %s\ncurrent filename: %s\n\n", path+1, readable_filename);
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000)) {
            fprintf(stderr, "\n\n\nFILE ALREADY EXISTS: %s permissions: %d\n\n\n", readable_filename, (int)directory[i].type_permissions);
            // File already exists.
            return -EEXIST;
        }
    }

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (directory[i].type_permissions == 0x0000) {
            fprintf(stderr, "\n\n\nFree file entry at index %d\n\n\n", i);
            // Found free file entry in directory.
            for (j = 0; j < MAX_FAT_ENTRIES; j++) {
                if (main_fat[j] == 0x0000) {
                    fprintf(stderr, "\n\n\nUsing Free file entry at index %d\n\n\n", i);

                    // Found free FAT entry.
                    name_to_encoded(path + 1, encoded_filename);
                    memcpy(directory[i].filename, encoded_filename, 11);
                    directory[i].type_permissions = (uint16_t)(S_IFREG | 0777);
                    directory[i].start_block = (uint16_t)j;
                    directory[i].unused = (uint16_t)0x00;
                    generate_memefs_timestamp(directory[i].bcd_timestamp);
                    directory[i].uid_owner = (uint16_t)getuid();
                    directory[i].gid_owner = (uint16_t)getgid();
                    directory[i].size = (uint32_t)0;
                    main_fat[j] = (uint16_t)0xFFFF;
                    backup_fat[j] = (uint16_t)0xFFFF;
                    name_to_readable(directory[i].filename, readable_filename);
                    fprintf(stderr, "\n\n\n[%d] CREATED FILE: %s with permissions: %d\n\n\n", i, readable_filename, (int)directory[i].type_permissions);
                    if (unload_image() != 0) {
                        fprintf(stderr, "Failed to update image\n");
                    }
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
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        // Root directory or "." or ".."
        stbuf->st_mode = (mode_t)(S_IFDIR | 0755);
        stbuf->st_nlink = (nlink_t)2;
        return 0;
    }

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        fprintf(stderr, "\n[%d]: %s\t%o\n", i, readable_filename, directory[i].type_permissions);

        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(readable_filename) == 0)) {
            // Found file.
            fprintf(stderr, "\n\n\n[%d] FOUND FILE WITH NAME: %sPERMISSIONS: %o\n\n\n", i, readable_filename, directory[i].type_permissions);
            stbuf->st_mode = (mode_t)(S_IFREG | 0777);
            stbuf->st_nlink = (nlink_t)1;
            stbuf->st_uid = (uid_t)directory[i].uid_owner;
            stbuf->st_gid = (gid_t)directory[i].gid_owner;
            stbuf->st_size = (off_t)directory[i].size;
            stbuf->st_mtime = (time_t)directory[i].bcd_timestamp;
            /*
            
            TODO: see if this is necessary
            
            */
            // stbuf->st_blocks = (blkcnt_t)((directory[i].size + 511) / 512);
            return 0;
        }
    }

    return -ENOENT;
}

static int memefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i;

    if (strcmp(path, "/") != 0) {
        // Not root directory.
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        if (directory[i].type_permissions != 0x0000
            && strlen(directory[i].filename) > 0
            && directory[i].filename[0] != '\0'
            && check_legal_name(readable_filename) == 0) {
            // Found file.
            // Convert to readable name.
            filler(buf, readable_filename, NULL, 0, 0);
        }
    }

    return 0;
}

static int memefs_open(const char* path, struct fuse_file_info* fi) {
    (void) fi;
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i;

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        fprintf(stderr, "\n\npath: %s\nreadable_filename: %s\n permissions: %o\n", path + 1, readable_filename, directory[i].type_permissions);
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(readable_filename) == 0)) {
            // Found file entry.
            fprintf(stderr, "\n\n\nFOUND EXISTING FILE: %s\n\n\n", readable_filename);
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

    
    /* 
    
    TODO: don't think this works
    
    */


    (void) size;
    (void) offset;
    (void) fi;
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i, curr_block, done;
    uint32_t file_size, bytes_read;
    off_t buffer_offset;

    // locate file in directory
    for (i = 0, curr_block = 0xFFFF; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(readable_filename) == 0)) {
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

static int memefs_unlink(const char* path) {
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i;
    uint16_t curr_block, next_block;

    // Find file in directory.
    for (i = 0, curr_block = 0xFFFF; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(readable_filename) == 0)) {
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
        backup_fat[curr_block] = 0x0000;
    }
    directory[i].type_permissions = 0x0000;

    if (unload_image() != 0) {
        fprintf(stderr, "Failed to update image\n");
    }
    return 0;
}

static int memefs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) fi;
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    write_type_t write_type;
    int i;

    write_type = INVALID;
    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(path + 1) == 0)) {
            // Found file.
            if (offset < directory[i].size) {
                write_type = OVERWRITE;
                break;
            } else if (offset == directory[i].size) {
                write_type = APPEND;
                fprintf(stderr, "\n\nAPPEND\n\n");
                break;
            }
        }
    }

    switch (write_type) {
        case OVERWRITE:
            if (overwrite_file(&directory[i], buf, size, offset) != 0) {
                return -EIO;
            }
            break;
        case APPEND:
            if (append_file(&directory[i], buf, size, offset) != 0) {
                return -EIO;
            }
            break;
        // OPTIONAL case Zero Fill Append when offset > file size
        case INVALID:
            return -ENOENT;
    }
    fprintf(stderr, "\n\nOLD SIZE: %d\n\n", (int)directory[i].size);
    directory[i].size = offset + size;
    fprintf(stderr, "\n\nNEW SIZE: %d\n\n", (int)directory[i].size);
            
    generate_memefs_timestamp(directory[i].bcd_timestamp);


    

    
    /*
    
    NOTE: i think some of this is for truncate
    what is truncate :(


    find file entry

    use fat to get to file data blocks
        consider offset. for every 512, jump to next fat reference
        then look at that data block and navigate to offset based on remainder
        start writing. if you hit the end of the block, search for an open data block and continue writing there
            add new data block to fat chain
        track bytes written. add to size? or recount?

    write data at offset, maybe extend file

    allocate new blocks if needed, track by adding to fat

    update size and timestamp in directory entry
    
    */
    if (unload_image() != 0) {
        fprintf(stderr, "Failed to update image\n");
    }
    return 0;
}

static int memefs_truncate(const char* path, off_t new_size, struct fuse_file_info* fi) {
    (void) path;
    (void) new_size;
    (void) fi;
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int g, h, i, j, k, curr_block, next_block, found;
    int blocks_in_use, blocks_needed, free_fat_blocks;

    if (strcmp(path, "/") == 0) {
        // Can't truncate a directory.
        return -EISDIR;
    }

    // Find file in directory.
    for (i = 0, h = 0, found = 0; i < MAX_FILE_ENTRIES && !found; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(readable_filename) == 0)) {
            // Found file.
            found = 1;
            h = i;
            break;
        }
    }

    if (!found) {
        // File not found.
        return -ENOENT;
    }

    blocks_in_use = (int)myCeil((double)directory[i].size / (double)BLOCK_SIZE);
    blocks_needed = (int)myCeil((double)new_size / (double)BLOCK_SIZE);
    if (blocks_needed == 0) {
        // Even empty files take up a FAT block.
        blocks_needed = 1;
    }

    // Truncate file to smaller size.
    if (blocks_needed < blocks_in_use) {
        // Traverse FAT Chain
        for (g = 1, curr_block = directory[h].start_block; g <= blocks_in_use; g++, curr_block = next_block) {
            next_block = main_fat[curr_block];
            if (g == blocks_needed) {
                // New end of chain block.
                main_fat[curr_block] = 0xFFFF;
                backup_fat[curr_block] = 0xFFFF;
            } else if (g > blocks_needed) {
                // Blocks after new end of chain.
                main_fat[curr_block] = 0x0000;
                backup_fat[curr_block] = 0x0000;
            }
        }
    }

    // Truncate file to larger size.
    if (blocks_needed > blocks_in_use) {
        // Count free FAT blocks.
        for (k = 0, free_fat_blocks = 0; k < MAX_FAT_ENTRIES; k++) {
            if (main_fat[k] == 0x0000) {
                free_fat_blocks++;
            }
        }
        
        // Check if there are enough free FAT blocks.
        if ((blocks_needed - blocks_in_use) > free_fat_blocks) {
            // Not enough free FAT blocks.
            return -ENOSPC;
        }

        // Traverse to last block in use
        for (j = 0, curr_block = directory[h].start_block; j < blocks_in_use; j++) {
            next_block = main_fat[curr_block];
            curr_block = next_block;
        }

        // Find free blocks in the FAT to extend into.
        while (j < blocks_needed) {
            for (i = 0; i < MAX_FAT_ENTRIES; i++) {
                if (main_fat[i] == 0x0000) {
                    // Found free block, make use of it.
                    main_fat[curr_block] = i;
                    backup_fat[curr_block] = i;
                    main_fat[i] = 0xFFFF;
                    backup_fat[i] = 0xFFFF;
                    curr_block = i;
                    j++;
                    break;
                }
            }
        }
    }

    // Update file size.
    directory[h].size = (uint32_t)new_size;
    if (unload_image() != 0) {
        fprintf(stderr, "Failed to update image\n");
    }
    return 0;
}

static void memefs_destroy(void* private_data) {
    (void) private_data;

    if (unload_image() != 0) {
        fprintf(stderr, "Failed to copy filesystem to image\n");
    }
    main_superblock.cleanly_unmounted = 0x00;
    backup_superblock.cleanly_unmounted = 0x00;

    // Close the image file descriptor
    if (img_fd >= 0) {
        close(img_fd);
        img_fd = -1;
    }
}

#pragma endregion FUSE Implementations

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

	return (load_image() ? 1 : fuse_main(argc - 1, argv + 1, &memefs_oper, NULL));
}

