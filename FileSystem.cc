#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "FileSystem.h"
#include "util.h"

// Constants
#define ROOT 127
#define BLOCK_SIZE 1024

// Global variables
Super_block * super_block = NULL;
std::string disk_name = "";
uint8_t current_directory = ROOT;
uint8_t buffer[BLOCK_SIZE] = {0};

bool is_inode_used(Inode inode) {
    return (inode.used_size >> 7) & 1;
}

uint8_t get_inode_size(Inode inode) {
    return (inode.used_size & ~(1UL << 7));
}

bool is_inode_dir(Inode inode) {
    return (inode.dir_parent  >> 7) & 1;
}

uint8_t get_parent_dir(Inode inode) {
    return (inode.dir_parent & ~(1UL << 7));
}

void set_inode_size(Inode * inode, int size) {
    inode->used_size = (uint8_t) size;
    inode->used_size |= 1UL << 7; // set most significant bit
}

bool is_name_set(Inode inode) {
    for (int i = 0; i < 5; i++) {
        if (inode.name[i] != 0) {
            return true;
        }
    }
    return false;
}

void allocate_block_in_free_list(int block_number) {
    int free_block_list_index = block_number/8;
    int block_index = block_number % 8;
    int bit_number = 7 - block_index;

    char * byte = &(super_block->free_block_list[free_block_list_index]);
    *byte |= 1UL << bit_number;
}

void free_block_in_free_list(int block_number) {
    int free_block_list_index = block_number/8;
    int block_index = block_number % 8;
    int bit_number = 7 - block_index;

    char * byte = &(super_block->free_block_list[free_block_list_index]);
    *byte &= ~(1UL << bit_number);
}

// void print_superblock() {
//     int fd2 = open(disk_name.c_str(), O_RDONLY);
//     Super_block * temp_super_block = new Super_block;
//     int sizeRead = read(fd2, temp_super_block, BLOCK_SIZE);

//     for (int i = 0; i < 16; i++) {
//         char byte = temp_super_block->free_block_list[i];
//         for (int j=7; j>=0; j--) {
//             int bit = ((byte >> j) & 1);
//             printf("%d", bit);
//             printf("\n");
//         }
//     }
//     for (int i = 0; i < 126; i++) {
//         Inode inode = temp_super_block->inode[i];
//         std::cout << "Inode " << i << ":\n";
//         printf("Type: %d Dir parent: %d Start Block: %d UsedSize: %d isUsed: %d", is_inode_dir(inode), get_parent_dir(inode), inode.start_block, get_inode_size(inode), is_inode_used(inode));
//         printf(" Name: %s", inode.name);
//         printf("\n");
//     }
//     printf("\n");
//     close(fd2);
// }

void write_superblock_to_disk() {
    int fd = open(disk_name.c_str(), O_RDWR);
    int sizeWritten = write(fd, super_block, BLOCK_SIZE);
    if (sizeWritten < BLOCK_SIZE) {
        std::cerr << "Error: Writing superblock back to disk\n";
    }
    close(fd);
}

void write_to_block(uint8_t buff[BLOCK_SIZE], int block_number) {
    int fd = open(disk_name.c_str(), O_RDWR);
    int offset = BLOCK_SIZE*block_number;
    int sizeWritten = pwrite(fd, buff, BLOCK_SIZE, offset);
    if (sizeWritten < BLOCK_SIZE) {
        std::cerr << "Error: Writing to block on disk\n";
    }
    close(fd);
}

void read_from_block(uint8_t buff[BLOCK_SIZE], int block_number) {
    int fd = open(disk_name.c_str(), O_RDWR);
    int offset = BLOCK_SIZE*block_number;
    int sizeRead = pread(fd, buff, BLOCK_SIZE, offset);
    if (sizeRead < BLOCK_SIZE) {
        std::cerr << "Error: Reading block from disk\n";
    }
    close(fd);
}

std::vector<int> get_contiguous_blocks(int size, int start_block = 1, int end_block = 128) {
    // Find the first set of contiguous blocks that can be allocated to the file by scanning
    // data blocks from 1 to 127.
    std::vector<int> contiguous_blocks;
    int block_number = start_block;
    while (block_number < end_block) {
        int free_block_list_index = block_number/8;
        int bit_number = 7 - (block_number % 8);
        char byte = super_block->free_block_list[free_block_list_index];

        int bit = ((byte >> bit_number) & 1);
        if (bit == 0) {
            contiguous_blocks.push_back(block_number);
        } else if (bit == 1) {
            contiguous_blocks.clear();
        }
        if ((int)contiguous_blocks.size() == size) {
            return contiguous_blocks;
        }
        block_number++;
    }
    contiguous_blocks.clear();
    return contiguous_blocks;
}

void copy_file_to_blocks(Inode * inode, std::vector<int> destination_blocks) {
    for (auto block: destination_blocks) {
        allocate_block_in_free_list(block);
    }
    //TODO: IF PERFORMANCE IS BAD MIGHT HAVE 2 change
    int currentSize = get_inode_size(*inode);
    int destinationIndex = 0;
    for (int i = inode->start_block; i < inode->start_block + currentSize; i++) {
        uint8_t buff[BLOCK_SIZE] = {0};
        read_from_block(buff, i);
        write_to_block(buff, destination_blocks[destinationIndex]);
        destinationIndex++;
    }
    inode->start_block = destination_blocks[0];
}

// Blocks that are marked free in the free-space list cannot be allocated to any file. Similarly, blocks
// marked in use in the free-space list must be allocated to exactly one file.
bool consistency_check_1(Super_block * temp_super_block) {
    std::set<int> free_blocks;
    std::set<int> used_blocks;
    int block_number = 0;
    for (int i = 0; i < 16; i++) {
        char byte = temp_super_block->free_block_list[i];
        for (int j=7; j>=0; j--) {
            if (block_number == 0) {// this is the superblock
                block_number++;
                continue;
            }
            int bit = ((byte >> j) & 1);
            if (bit == 0) {
                free_blocks.insert(block_number);
            } else if (bit == 1) { // In use
                used_blocks.insert(block_number);
            }
            block_number++;
        }
    }

    std::vector<int> inode_used_blocks;
    for (int i = 0; i < 126; i ++) {
        Inode inode = temp_super_block->inode[i];
        if (is_inode_used(inode)) {
            int fileSize = get_inode_size(inode);
            for (int j = inode.start_block; j < inode.start_block + fileSize; j++) {
                if (free_blocks.find(j) != free_blocks.end()) {
                    return false;
                }
                inode_used_blocks.push_back(j);
            }
        }
    }

    for (auto f: used_blocks) {
        if ( f != 0 && std::count(inode_used_blocks.begin(), inode_used_blocks.end(), f) != 1) {
            return false;
        }
    }

    return true;
}

// The name of every file/directory must be unique in each directory
bool consistency_check_2(Super_block * temp_super_block) {
    std::map<uint8_t, std::vector<Inode>> directory;

    for (int i = 0; i < 126; i++) {
        Inode inode = temp_super_block->inode[i];
        uint8_t parent_dir = get_parent_dir(inode);

        if (is_name_set(inode)) {
            auto it = directory.find(parent_dir);
            if (it != directory.end()) {
                std::vector<Inode> directoryContents = it->second;
                for (size_t i = 0; i < directoryContents.size(); i++) {
                    if (strncmp(directoryContents[0].name, inode.name, 5) == 0) {
                        return false;
                    }
                }
                directoryContents.push_back(inode);
            } else {
                std::vector<Inode> directoryContents;
                directoryContents.push_back(inode);
                directory.insert({parent_dir, directoryContents});
            }
        }
    }
    return true;
}

// If the state of an inode is free, all bits in this inode must be zero. Otherwise, the name attribute stored
// in the inode must have at least one bit that is not zero.
bool consistency_check_3(Super_block * temp_super_block) {
    for (int i = 0; i < 126; i++) {
        Inode inode = temp_super_block->inode[i];

        if (is_inode_used(inode)) {
            for (int i = 0; i < 5; i++) {
                if (inode.name[i] != 0) {
                    return true;
                }
            }
            return false;
        } else {//inode free
            if (inode.dir_parent != 0 || inode.start_block != 0 || inode.used_size != 0) {
                return false;
            }
            for (int i = 0; i < 5; i++) {
                if (inode.name[i] != 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

// The start block of every inode that is marked as a file must have a value between 1 and 127 inclusive
bool consistency_check_4(Super_block * temp_super_block) {
    for (int i = 0; i < 126; i++) {
        Inode inode = temp_super_block->inode[i];
        if (is_inode_used(inode) && !is_inode_dir(inode) && (inode.start_block < 1 || inode.start_block > 127)) {
            return false;
        }
    }
    return true;
}

// The size and start block of an inode that is marked as a directory must be zero.
bool consistency_check_5(Super_block * temp_super_block) {
    for (int i = 0; i < 126; i++) {
        Inode inode = temp_super_block->inode[i];
        if (is_inode_used(inode) && is_inode_dir(inode) && (inode.start_block != 0 || get_inode_size(inode) != 0)) {
            return false;
        }
    }
    return true;
}

// For every inode, the index of its parent inode cannot be 126. Moreover, if the index of the parent inode
// is between 0 and 125 inclusive, then the parent inode must be in use and marked as a directory.
bool consistency_check_6(Super_block * temp_super_block) {
    for (int i = 0; i < 126; i++) {
        Inode inode = temp_super_block->inode[i];
        if (is_inode_used(inode)) {
            if (inode.dir_parent == 126) {
                return false;
            } else if (inode.dir_parent >= 0 && inode.dir_parent <= 125) {
                Inode parent_inode = temp_super_block->inode[inode.dir_parent];
                if (!is_inode_used(parent_inode) || !is_inode_dir(parent_inode)) {
                    return false;
                }
            }
        }
    }
    return true;
}

int check_consistency(Super_block * temp_super_block) {
    int errorCode = 0;

    if (!consistency_check_1(temp_super_block)) {
        errorCode = 1;
    } else if (!consistency_check_2(temp_super_block)) {
        errorCode = 2;
    } else if (!consistency_check_3(temp_super_block)) {
        errorCode = 3;
    } else if (!consistency_check_4(temp_super_block)) {
        errorCode = 4;
    } else if (!consistency_check_5(temp_super_block)) {
        errorCode = 5;
    } else if (!consistency_check_6(temp_super_block)) {
        errorCode = 6;
    }

    return errorCode;
}

void fs_mount(char *new_disk_name) {
    struct stat sb;

    if (stat(new_disk_name, &sb) != 0) {
        std::cerr << "Error: Cannot find disk " << new_disk_name << std::endl;
        return;
    }

    int fd = open(new_disk_name, O_RDONLY);
    if (fd < 0) {
        printf("Error\n");
        return;
    }

    // Read the superblock
    Super_block * temp_super_block = new Super_block;
    int sizeRead = read(fd, temp_super_block, BLOCK_SIZE);
    if (sizeRead < BLOCK_SIZE) {
        std::cerr << "Error: Reading superblock during mount was not successful\n";
        delete temp_super_block;
        close(fd);
        return;
    }

    int errorCode = check_consistency(temp_super_block);

    if (errorCode == 0) {
        super_block = temp_super_block;
        disk_name = new_disk_name;
        current_directory = ROOT;
    } else {
        std::cout << "Error: File system in " << new_disk_name << " is inconsistent";
        std::cout << " (error code: " << errorCode << ")\n";
        delete temp_super_block;
    }

    close(fd);
    // print_superblock();
}

void fs_create(char name[5], int size) {
    // Need to find first available inode
    Inode * available_inode = NULL;
    for (int i = 0; i < 126; i++) {
        available_inode = &(super_block->inode[i]);
        if (!is_inode_used(*available_inode)) {
            break;
        } else {
            available_inode = NULL;
        }
    }

    // No available inodes were found
    if (available_inode == NULL) {
        std::cerr << "Error: Superblock in disk " << disk_name;
        std::cerr << " is full, cannot create " << name << std::endl;
        return;
    }

    // "." and ".." are reserved and can't be used for a file/directory
    if (strncmp(name, ".", 5) == 0 || strncmp(name, "..", 5) == 0) {
        std::cerr << "Error: File or directory " << name;
        std::cerr << " already exists\n";
        return;
    }

    // New file/directory needs to have unique name within current working directory
    for (int i = 0; i < 126; i++) {
        Inode inode = super_block->inode[i];
        if (is_inode_used(inode) && get_parent_dir(inode) == current_directory && strncmp(inode.name, name, 5) == 0) {
            std::cerr << "Error: File or directory " << name;
            std::cerr << " already exists\n";
            return;
        }
    }

    std::vector<int> contiguous_blocks;
    // If it's a file, we have to allocate space
    if (size != 0) {
        contiguous_blocks = get_contiguous_blocks(size);

        if (contiguous_blocks.empty()) {
            std::cerr << "Error: Cannot allocate " << size << " on " << disk_name << std::endl;
            return;
        }

        for (auto block: contiguous_blocks) {
            allocate_block_in_free_list(block);
        }
    }

    available_inode->dir_parent = current_directory;
    if (size == 0) {// set bit
        available_inode->dir_parent |= 1UL << 7;
        available_inode->start_block = 0;
    } else {// clear bit
        available_inode->dir_parent &= ~(1UL << 7);
        available_inode->start_block = contiguous_blocks[0];
    }

    set_inode_size(available_inode, size);
    strncpy(available_inode->name, name, 5);

    write_superblock_to_disk();
}

void delete_file(Inode * inode) {
    uint8_t buff[BLOCK_SIZE] = {0};
    int size = get_inode_size(*inode);
    for (int i = inode->start_block; i < inode->start_block + size; i++) {
        write_to_block(buff, i);
        free_block_in_free_list(i);
    }

    inode->dir_parent = 0;
    inode->start_block = 0;
    inode->used_size = 0;
    for (int i = 0; i < 5; i++) {
        inode->name[i] = 0;
    }
}

// Recursive function
void delete_directory(int directory) {
    for (int i = 0; i < 126; i++) {
        Inode * inode = &(super_block->inode[i]);
        if (is_inode_used(*inode) && get_parent_dir(*inode) == directory) {
            if (is_inode_dir(*inode)) {
                delete_directory(i);
            } else {
                delete_file(inode);
            }
        }
    }

    super_block->inode[directory].dir_parent = 0;
    super_block->inode[directory].start_block = 0;
    super_block->inode[directory].used_size = 0;
    for (int i = 0; i < 5; i++) {
        super_block->inode[directory].name[i] = 0;
    }
}

void fs_delete(char name[5]) {
    Inode * inode = NULL;
    int inodeIndex;
    for (inodeIndex = 0; inodeIndex < 126; inodeIndex++) {
        inode = &(super_block->inode[inodeIndex]);
        if (is_inode_used(*inode) && get_parent_dir(*inode) == current_directory && strncmp(inode->name, name, 5) == 0) {
            break;
        }
        inode = NULL;
    }

    if (inode == NULL) {
        std::cerr << "Error: File or directory " << name << " does not exist\n";
        return;
    }

    if (is_inode_dir(*inode)) {
        delete_directory(inodeIndex);
    } else {
        delete_file(inode);
    }

    write_superblock_to_disk();
}

void fs_read(char name[5], int block_num) {
    Inode * inode = NULL;
    for (int i = 0; i < 126; i++) {
        inode = &(super_block->inode[i]);
        if (is_inode_used(*inode) && !is_inode_dir(*inode) && get_parent_dir(*inode) == current_directory && strncmp(inode->name, name, 5) == 0) {
            break;
        }
        inode = NULL;
    }

    if (inode == NULL) {
        std::cerr << "Error: File " << name << " does not exist\n";
        return;
    }

    if (block_num < 0 || block_num >= get_inode_size(*inode)) {
        std::cerr << "Error: " << name << " does not have block " << block_num << std::endl;
        return;
    }

    read_from_block(buffer, inode->start_block + block_num);
}

void fs_write(char name[5], int block_num) {
    Inode * inode = NULL;
    for (int i = 0; i < 126; i++) {
        inode = &(super_block->inode[i]);
        if (is_inode_used(*inode) && !is_inode_dir(*inode) && get_parent_dir(*inode) == current_directory && strncmp(inode->name, name, 5) == 0) {
            break;
        }
        inode = NULL;
    }

    if (inode == NULL) {
        std::cerr << "Error: File " << name << " does not exist\n";
        return;
    }

    if (block_num < 0 || block_num >= get_inode_size(*inode)) {
        std::cerr << "Error: " << name << " does not have block " << block_num << std::endl;
        return;
    }

    write_to_block(buffer, inode->start_block + block_num);
}

void fs_buff(uint8_t buff[1024], int size) {
    //Flush the buffer
    for (int i = 0; i < BLOCK_SIZE; i ++) {
        buffer[i] = 0;
    }
    memcpy(buffer, buff, size);
}

void fs_resize(char name[5], int new_size) {
    Inode * inode = NULL;
    for (int i = 0; i < 126; i++) {
        inode = &(super_block->inode[i]);
        if (is_inode_used(*inode) &&
                    !is_inode_dir(*inode) &&
                    get_parent_dir(*inode) == current_directory &&
                    strncmp(name, inode->name, 5) == 0) {
            break;
        }
        inode = NULL;
    }

    if (inode == NULL) {
        std::cerr << "Error: File " << name << " does not exist\n";
        return;
    }

    int current_size = get_inode_size(*inode);
    if (new_size < current_size) {
        uint8_t buff[BLOCK_SIZE] = {0};
        for (int i = inode->start_block + new_size; i < inode->start_block + current_size; i++) {
            write_to_block(buff, i);
            free_block_in_free_list(i);
        }
    } else if (new_size > current_size) {
        std::vector<int> contiguous_blocks = get_contiguous_blocks(new_size - current_size, inode->start_block + current_size, inode->start_block + new_size);

        //Not enough blocks in the next blocks
        if (contiguous_blocks.empty()) {
            for (int i = inode->start_block; i < inode->start_block + current_size; i++) {
                free_block_in_free_list(i);
            }
            contiguous_blocks = get_contiguous_blocks(new_size);
            if (contiguous_blocks.empty()) {
                for (int i = inode->start_block; i < inode->start_block + current_size; i++) {
                    allocate_block_in_free_list(i);
                }
                std::cerr << "Error: File " << name << " cannot expand to size " << new_size << std::endl;
                return;
            } else {
                copy_file_to_blocks(inode, contiguous_blocks);
            }
        } else {// Enough blocks available
            uint8_t buff[BLOCK_SIZE] = {0};
            for (auto block: contiguous_blocks) {
                write_to_block(buff, block);
                allocate_block_in_free_list(block);
            }
        }
    } else {
        return;
    }

    set_inode_size(inode, new_size);
    write_superblock_to_disk();
}

void fs_cd(char name[5]) {
    if (strncmp(name, ".", 5) == 0) {
        // Stay at the current directory
        return;
    } else if (strncmp(name, "..", 5) == 0) {
        if (current_directory != ROOT) {
            current_directory = get_parent_dir(super_block->inode[current_directory]);
        }
        return;
    }

    bool directory_found = false;
    int inodeIndex;
    for (inodeIndex = 0; inodeIndex < 126; inodeIndex++) {
        Inode inode = super_block->inode[inodeIndex];
        if (is_inode_used(inode) &&
                    is_inode_dir(inode) &&
                    get_parent_dir(inode) == current_directory &&
                    strncmp(name, inode.name, 5) == 0) {
            directory_found = true;
            break;
        }
    }

    if (directory_found) {
        current_directory = inodeIndex;
    } else {
        std::cerr << "Error: Directory " << name << " does not exist\n";
    }
}

bool runCommand(std::vector<std::string> arguments) {
    // Separate out the command and the arguments
    std::string command = arguments[0];
    arguments.erase(arguments.begin());
    bool isValid = true;
    bool isMounted = super_block != NULL;

    if (command.compare("M") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        } else {
            char * cstr = &(arguments[0][0]);
            fs_mount(cstr);
        }
    } else if (command.compare("C") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 0 || stoi(arguments[1]) > 127) {
            isValid = false;
        } else if (!isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        } else {
            char * cstr = &(arguments[0][0]);
            fs_create(cstr, stoi(arguments[1]));
        }
    } else if (command.compare("D") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (!isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        } else {
            char * cstr = &(arguments[0][0]);
            fs_delete(cstr);
        }
    } else if (command.compare("R") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 0 || stoi(arguments[1]) > 126) {
            isValid = false;
        } else if (!isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        } else {
            char * cstr = &(arguments[0][0]);
            fs_read(cstr, stoi(arguments[1]));
        }
    } else if (command.compare("W") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 0 || stoi(arguments[1]) > 126) {
            isValid = false;
        } else if (!isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        } else {
            char * cstr = &(arguments[0][0]);
            fs_write(cstr, stoi(arguments[1]));
        }
    } else if (command.compare("B") == 0) {
        if (arguments.size() < 1) {
            isValid = false;
        } else {
            std::string message = arguments[0];
            message.erase(0, message.find_first_not_of(" "));

            if (message.size() > BLOCK_SIZE || message.size() < 1) {
                isValid = false;
            } else if (!isMounted) {
                std::cerr << "Error: No file system is mounted\n";
            } else {
                uint8_t * buff = (uint8_t *) &(message[0]);
                fs_buff(buff, message.size());
            }
        }
    } else if (command.compare("L") == 0) {
        if (arguments.size() != 0) {
            isValid = false;
        } else if (!isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        }
        
    } else if (command.compare("E") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 1 || stoi(arguments[1]) > 127) {
            isValid = false;
        } else if (!isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        } else {
            char * cstr = &(arguments[0][0]);
            fs_resize(cstr, stoi(arguments[1]));
        }
    } else if (command.compare("O") == 0) {
        if (arguments.size() != 0) {
            isValid = false;
        } else if (!isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        }
        
    } else if (command.compare("Y") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (!isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        } else {
            char * cstr = &(arguments[0][0]);
            fs_cd(cstr);
        }
    } else {
        return false;
    }

    return isValid;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Please provide a command file.\n";
        return 0;
    } else if (argc > 2) {
        std::cerr << "Too many arguments. Please provide one command file.\n";
        return 0;
    }

    std::string command_file_name = argv[1];
    std::ifstream command_file (command_file_name);

    if (!command_file.is_open()){
        std::cerr << "Unable to open the command file.\n";
        return 0;
    }

    std::string command;
    int line_number = 0;
    while (getline(command_file, command)) {
        line_number++;
        std::vector<std::string> arguments;
        if (command.at(0) == 'B') {
            char * command_cstr = const_cast<char*> (command.c_str());
            char * command_first_arg = strsep(&command_cstr, " ");
            arguments.push_back(std::string(command_first_arg));
            if (command_cstr != NULL) {
                arguments.push_back(std::string(command_cstr));
            }
        } else {
            arguments = tokenize(command, " ");
        }
        if (runCommand(arguments) == false) {
            std::cerr << "Command Error: " << command_file_name << ", " << line_number << std::endl;
        }
    }

    if (super_block != NULL) {
        delete super_block;
    }

    command_file.close();
    // print_superblock();

    return 0;
}