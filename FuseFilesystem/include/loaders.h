#ifndef LOADERS_H
#define LOADERS_H

// Helper functions for loading data from the filesystem image.
int load_superblock();
int load_directory();
int load_fat();
int load_user_data();

#endif // LOADERS_H