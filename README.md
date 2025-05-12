# CMSC 421 Project 4

## Introduction
**Platforms** - Linux

MEMEfs is a FUSE-based user-space filesystem that stores files using 32-byte data blocks and a simple block allocation strategy. It supports creating, reading, writing, and deleting files. File names are restricted to 8 character names and 3 character extensions. The filesystem directory has space for 224 files. The user data section has 220 blocks for storing user data. The filesystem uses a FAT to access user data. This project is a memefs FUSE filesystem implementation. It supports read and write operations on files stored within a disk image. The primary goal of this project is to gain experience with filesystem operations and secondary memory by managing files stored within a disk image file.

### Supported FUSE Operations
* `create` – Creates a new file in the filesystem
* `destroy` – Unloads the file image from memory to the filesystem image
* `getattr` – Retrieves file metadata such as size, permissions, and last modification time
* `open` – Opens a file and validates its existence
* `read` – Reads data from a file, respecting file size and bounds
* `readdir` – Lists files in the root directory of the filesystem
* `unlink` – Deletes a file
* `write` – Writes data to a file, supporting overwrites, appends, and partial writes

### Extra Credit Features
* `truncate` – Changes the size of a file


## Contact
* **Contributor** - Eric Ekey
* **Student ID** - XR06051
* **Email** - eekey1@umbc.edu


## Installation and Setup
### Setup

Install up-to-date dependencies for building this program:

**Debian**
~~~bash
sudo apt update
sudo apt install fuse3 libfuse3-3 libfuse3-dev make
~~~

### Build and Compile
Navigate to the `FuseFilesystem/` directory, write the filesystem image, and compile the memefs implementation using the Makefile:
~~~bash
cd FuseFilesystem/
make create_memefs_img create_dir all
~~~
Mount the filesystem using the provided Makefile:
~~~bash
make mount_memefs
~~~
OR you can mount the filesystem and view internal logging using the provided Makefile:
~~~bash
make debug
~~~
You can view the state of the bytes in the filesystem using the debugger script:
~~~bash
./memefs_debugger.sh myfilesystem.img
~~~
You can unmount the filesystem using the provided Makefile:
~~~bash
make unmount_memefs
~~~

## Testing Strategy
* Create files using `touch` and verify with `ls`
* Write data using `echo >` and read with `cat` to confirm contents
* Append data using `echo >>` and verify the full contents
* Delete files using `rm` and ensure they are removed from directory listings
* Use internal logging to monitor operation success and failures
* Use the provided bash script `memefs_debugger.sh` to inspect raw disk image data for consistency.

## Troubleshooting
### Known Issues
* None

## References
### External Resources
* [Programiz](https://www.programiz.com/)
* [GeeksForGeeks](https://www.geeksforgeeks.org/)
* [cppreference](https://en.cppreference.com/) 
* [tutorialspoint](https://www.tutorialspoint.com/)
* [man7](https://www.man7.org/)



