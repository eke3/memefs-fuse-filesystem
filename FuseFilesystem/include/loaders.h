#ifndef LOADERS_H
#define LOADERS_H

// int load_image()
// Description: Loads the filesystem image into memory.
// Preconditions: Filesystem image exists.
// Postconditions: Filesystem image is loaded into memory.
// Returns: 0 on success, 1 on failure.
int load_image();

// int unload_image()
// Description: Unloads the filesystem image from memory.
// Preconditions: Filesystem image is loaded into memory.
// Postconditions: Filesystem image is rewritten from memory.
// Returns: 0 on success, 1 on failure.
int unload_image();

#endif // LOADERS_H