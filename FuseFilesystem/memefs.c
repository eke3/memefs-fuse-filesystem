// File:    memefs.c
// Author:  Eric Ekey
// Date:    04/27/2025
// Desc:    Implementation of the memefs filesystem.

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "define.h"
#include "loaders.h"
#include "memefs_file_entry.h"
#include "memefs_superblock.h"
#include "utils.h"

#pragma region Globals

extern int img_fd;
extern memefs_superblock_t main_superblock;
extern memefs_superblock_t backup_superblock;
extern memefs_file_entry_t directory[MAX_FILE_ENTRIES];
extern uint16_t main_fat[MAX_FAT_ENTRIES];
extern uint16_t backup_fat[MAX_FAT_ENTRIES];
extern uint8_t user_data[USER_DATA_NUM_BLOCKS * BLOCK_SIZE];

#pragma endregion Globals

#pragma region FUSE Prototypes

// FUSE operations.
static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static void memefs_destroy(void* private_data);
static int memefs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);
static int memefs_open(const char* path, struct fuse_file_info* fi);
static int memefs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
static int memefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags);
static int memefs_truncate(const char* path, off_t new_size, struct fuse_file_info* fi);
static int memefs_unlink(const char *path);
static int memefs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi);
static int memefs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

#pragma endregion FUSE Prototypes

#pragma region FUSE Implementations

// FUSE operations.
static const struct fuse_operations memefs_oper = {
    .create   = memefs_create,
    .destroy  = memefs_destroy,
    .getattr  = memefs_getattr,
    .open     = memefs_open,
    .read     = memefs_read,
    .readdir  = memefs_readdir,
    .truncate = memefs_truncate,
    .unlink   = memefs_unlink,
    .utimens  = memefs_utimens,
    .write    = memefs_write,
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
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000)) {
            // File already exists.
            return -EEXIST;
        }
    }

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        if (directory[i].type_permissions == 0x0000) {
            // Found free file entry in directory.
            for (j = 0; j < MAX_FAT_ENTRIES; j++) {
                if (main_fat[j] == 0x0000) {
                    // Found free FAT entry.
                    name_to_encoded(path + 1, encoded_filename);
                    memcpy(directory[i].filename, encoded_filename, 11);
                    directory[i].type_permissions = (uint16_t)(S_IFREG | 0644);
                    directory[i].start_block = (uint16_t)j;
                    directory[i].unused = (uint16_t)0x00;
                    generate_memefs_timestamp(directory[i].bcd_timestamp);
                    directory[i].uid_owner = (uint16_t)getuid();
                    directory[i].gid_owner = (uint16_t)getgid();
                    directory[i].size = (uint32_t)0;
                    main_fat[j] = (uint16_t)0xFFFF;
                    backup_fat[j] = (uint16_t)0xFFFF;
                    if (unload_image() != 0) {
                        fprintf(stderr, "Failed to update image after create()\n");
                        return -EIO;
                    }
                    return 0;
                }
            }
        }
    }

    return -ENOSPC;
}

static void memefs_destroy(void* private_data) {
    (void) private_data;

    main_superblock.cleanly_unmounted = 0x00;
    backup_superblock.cleanly_unmounted = 0x00;
    if (unload_image() != 0) {
        fprintf(stderr, "Failed to update image after destroy()\n");
    }

    // Close the image file descriptor
    if (img_fd >= 0) {
        close(img_fd);
        img_fd = -1;
    }
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
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(readable_filename) == 0)) {
            // Found file.
            stbuf->st_mode = (mode_t)(S_IFREG | 0644);
            stbuf->st_nlink = (nlink_t)1;
            stbuf->st_uid = (uid_t)directory[i].uid_owner;
            stbuf->st_gid = (gid_t)directory[i].gid_owner;
            stbuf->st_size = (off_t)directory[i].size;
            stbuf->st_mtime = memefs_bcd_to_time(directory[i].bcd_timestamp);
            stbuf->st_blocks = (blkcnt_t)((directory[i].size + 511) / BLOCK_SIZE);
            return 0;
        }
    }

    return -ENOENT;
}

static int memefs_open(const char* path, struct fuse_file_info* fi) {
    (void) fi;
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i;

    for (i = 0; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(readable_filename) == 0)) {
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
    (void) offset;
    (void) fi;
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i, curr_block, bytes_read;
    uint32_t file_size;
    size_t bytes_to_read;
    off_t buffer_offset;

    // Locate file in directory
    for (i = 0, curr_block = -1; i < MAX_FILE_ENTRIES; i++) {
        name_to_readable(directory[i].filename, readable_filename);
        if ((strcmp(readable_filename, path + 1) == 0) && (directory[i].type_permissions != 0x0000) && (check_legal_name(readable_filename) == 0)) {
            // Found file entry.
            curr_block = directory[i].start_block;
            file_size = directory[i].size;
            break;
        }
    }

    if (curr_block == -1) {
        // File not found.
        return -ENOENT;
    }

    // Adjust size if reading beyond EOF
    size = (size_t)MIN(size, file_size);
    bytes_to_read = 0;
    bytes_read = 0;
    buffer_offset = 0;

    // Copy data from FAT into buffer.
    while (((int)size > 0) && (curr_block != 0xFFFF)) {
        bytes_to_read = MIN(BLOCK_SIZE, size);
        memcpy(buf + buffer_offset, &user_data[curr_block * BLOCK_SIZE], bytes_to_read);
        buffer_offset += bytes_to_read;
        size -= bytes_to_read;
        bytes_read += bytes_to_read;
        curr_block = main_fat[curr_block];
    }

    return bytes_read;
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
    generate_memefs_timestamp(directory[h].bcd_timestamp);
    directory[h].size = (uint32_t)new_size;
    if (unload_image() != 0) {
        fprintf(stderr, "Failed to unload image after truncate()\n");
        return -EIO;
    }
    return 0;
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
        fprintf(stderr, "Failed to unload image after unlink()\n");
        return -EIO;
    }
    return 0;
}

static int memefs_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi) {
    (void) path;
    (void) tv;
    (void) fi;
    return 0;
}

static int memefs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) fi;
    char readable_filename[MAX_READABLE_FILENAME_LENGTH];
    int i;
    write_type_t write_type;

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
                break;
            }
        }
    }

    switch (write_type) {
        case OVERWRITE:
            if (overwrite_file(&directory[i], buf, size) != 0) {
                return -ENOSPC;
            }            
            break;
        case APPEND:
            if (append_file(&directory[i], buf, size) != 0) {
                return -ENOSPC;
            }
            break;
        case INVALID:
            return -ENOENT;
    }
            
    generate_memefs_timestamp(directory[i].bcd_timestamp);
    if (unload_image() != 0) {
        fprintf(stderr, "Failed to unload image after write()\n");
        return -EIO;
    }
    return (int)size;
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

