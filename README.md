# UNIX-like File System Simulator

This is a simple UNIX-esque file system simulator written in C++. The program mounts a file system residing on a virtual disk (i.e a plain file acting as the disk) and checks if it is consistent. It subsequently reads commands from an input file and executes them by simulating file system operations. The commands supported for this file system are mounting a disk, creating files/directories, deleting files/directories, reading files, writing files, listing the files in the current directory, defragmenting the disk, and changing the current working directory.

### Using the file system
Compile the project and provide it with an input file with commands.
```sh
$ make
$ ./fs <input_file>
```

### Commands supported
These are the command that are supported in the input file

- `M` - Mount the file system residing on the disk (results in the invocation of fs mount)

   Usage: `M <disk name>`  
   Description: Mounts the file system for the given virtual disk (file) and sets the current working directory to root.

- `C` - Create file (results in the invocation of fs create)

   Usage: `C <file name> <file size>`  
   Description: Creates a new file with the provided name in the current working directory.

- `D` - Delete file (results in the invocation of fs delete)

   Usage: `D <file name>`  
   Description: Deletes the specified file from the current working directory.

- `R` - Read file (results in the invocation of fs read)

   Usage: `R <file name> <block number>`  
   Description: Reads the block number-th block from the specified file into the buffer.

- `W` - Write file (results in the invocation of fs write)

   Usage: `W <file name> <block number>`  
   Description: Writes the data in the buffer to the block number-th block of the specified file.

- `B` - Update buffer (results in the invocation of fs buff)

   Usage: `B <new buffer characters>`  
   Description: Updates the buffer with the provided characters. Up to 1024 characters can be provided. If fewer characters are provided, the remaining bytes (at the end of the buffer) are set to 0.

- `L` - List files (results in the invocation of fs ls)

   Usage: `L`  
   Description: Lists all the files/directories in the current working directory, including . and ..

- `E` - Change files size (results in the invocation of fs resize)

   Usage: `E <file name> <new size>`  
   Description: Changes the size of the given file. If the file size is reduced, the extra blocks must be deleted(zeroed out).

- `O` - Defragment the disk (results in the invocation of fs defrag)

   Usage: `O`  
   Description: Defragments the disk, moving used blocks toward the superblock while maintaining the file data. As a result of performing defragmentation, contiguous free blocks can be created.

- `Y` - Change the current working directory (results in the invocation of fs cd)

   Usage: `Y <directory name>`  
   Description: Updates the current working directory to the provided directory. This new directory can be either a subdirectory in the current working directory or the parent of the current working directory

### Design Choices
The file system was designed with modularity and the DRY (Don't Repeat Yourself) principle in mind. A lot of operations were very common and repeated often (especially bit manipulation) so they were separated into common functions/files so they could be used again and again. This was done so that if the code needs to be changed, it is more maintainable and only needs to be changed in one place and doesn't impact the rest of the code. The code is divided into 5 main files: `FileSystem.cc`, `ConsistencyCheck.cc`, `IO.cc`, `InodeHelper.cc`  and `Util.cc`. `FileSystem.cc` contains the main functionality of the program, with the other files being "helper" files. The "helper" files contain commonly used functions that the other files make use of.

###### FileSystem.cc
This file is the entry point to the program. It reads in the command file and parses the commands by splitting up the arguments. This is done with the help of the `Util.cc` file and its `tokenize` function. From these parsed arguments, it determines which file system operation to run. This file contains the main functionality of the file system with functions like `fs_read()`, `fs_mount()`, and `fs_create()` which perform the matching file system operation. The `fs_mount()` function makes use of the `ConsistencyCheck.cc` file to ensure that the disk to be mounted is consistent. All of the other file system operations use the helper files `IO.cc` and `InodeHelper.cc` to perform their specific operation. 

###### ConsistencyCheck.cc
This file handles the consistency checks that must be performed when a disk is to be mounted. It contains the 6 checks that are described in the assignment description. `FileSystem.cc` uses this file in `fs_mount()` when it calls the `check_consistency()` function. It returns the error code of the check that failed. 

###### IO.cc
This file contains helper functions that handle manipulation of the superblock and the disk. It performs various operations on the free block list like allocating a block, freeing a block, and checking if a block is free. It also contains functions that open up the disk and write to a block and read from a block. It contains a helper function for writing the superblock struct back to the disk. In addition, there are functions for moving a file and deleting a file. The other code files use `IO.cc` to perform these common operations.

###### InodeHelper.cc
This file contains helper functions that get information about an inode, and also change data in the inode. Since getting the relevant info from the inode struct involves bit manipulation, this file abstracts that away with helper functions. It contains functions that determine if the inode is in use, if it is a directory, and if the name is set. It also contains functions to get the parent directory, get the inode size, and set the inode size. The other files use this file if they need operations on an inode to be performed.

###### Util.cc
This file contains the `tokenize()` function. It is only used by `FileSystem.cc`. It takes a string and a delimeter and it returns the tokens that are split by the delimeter. It is used to split up the command arguments so that the right file system operation can be invoked.

### Functions + System Calls
| Function                      | System Calls Used                                 |
| ----------------------------- |:-------------------------------------------------:|
| (1) fs_mount                  | `stat()` `open()` `read()` `close()`              |
| (2) fs_create                 | `open()` `write()` `close()`                      |
| (3) fs_delete                 | `open()` `write()` `pwrite()` `close()`           |
| (4) fs_read                   | `open()` `pread()` `close()`                      |
| (5) fs_write                  | `open()` `pwrite()` `close()`                     |
| (6) fs_buff                   | None                                              |
| (7) fs_ls                     | None                                              |
| (8) fs_resize                 | `open()` `write()` `pwrite()` `pread()` `close()` |
| (9) fs_defrag                 | `open()` `write()` `pwrite()` `pread()` `close()` |
| (10) fs_cd                    | None                                              |


### Testing Strategy
Testing was done in an incremental fashion; each feature was tested during and after its implementation. I would implement one of the functions in the file system then I would run the program with a command file that invokes those file system operations. I used GDB and print statements to examine the contents of data structures at certain points in the code, to see if everything was behaving correctly. I had a useful `print_superblock()` function that would print out the contents of the superblock in a human readable fashion so that I could verify that the file operations update the superblock correctly. I had a similar function for the disk as well, so that I could confirm that the file operations also updated the rest of the disk correctly.

After completing the code, I tested my implementation with the provided sample tests. I redirected the stdout of the program to a file called `output` and did the same with the stderr to a file called `error`. I tested with the consistency test cases and ran the `diff` command with the expected stderr and my error to ensure it was the same. Then with the other 4 sample tests, I ran the respective command files and verified the disk and the result disk were the same with the `diff` command, and verified that the stdout and stderr were also the same as the expected ones with the `diff` command as well.

Finally, I used valgrind to check for memory leaks and errors. Valgrind helped me catch memory leaks that I missed when writing the code.

### Sources
The `tokenize()` function provided in Assignment 1 was used. It is located in the `Util.cc` file.

No other external sources, other than class notes and the function mentioned above, were used.