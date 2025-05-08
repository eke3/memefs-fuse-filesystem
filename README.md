# CMSC 421 Project 4


## Introduction
**Platforms** - Linux


This project is [...]. It supports [...]. The primary goals of this project include [...].


### Features
*
*
*

### Extra Credit Features
*
*
*

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
You can view the state of the bytes in the filesystem using the debugger script:
~~~bash
./memefs_debugger.sh myfilesystem.img
~~~
You can unmount the filesystem using the provided Makefile:
```bash
make unmount_memefs
```


## Testing Strategy
[...]

### Supported FUSE Operations
*
*
*

## Troubleshooting
### Known Issues
* Undiagnosed crash when user data becomes very long, approaching space limitations.


## References
### External Resources
* [Programiz](https://www.programiz.com/)
* [GeeksForGeeks](https://www.geeksforgeeks.org/)
* [cppreference](https://en.cppreference.com/) 
* [tutorialspoint](https://www.tutorialspoint.com/)
* [man7](https://www.man7.org/)



