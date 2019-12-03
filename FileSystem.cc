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

#define ROOT 127
#define BLOCK_SIZE 1024

Super_block * super_block = NULL;
std::string disk_name = "";
uint8_t current_directory = ROOT;

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

void write_to_block(char buff[BLOCK_SIZE], int block_number) {
    int fd = open(disk_name.c_str(), O_RDWR);
    int offset = BLOCK_SIZE*block_number;
    int sizeWritten = pwrite(fd, buff, BLOCK_SIZE, offset);
    if (sizeWritten < BLOCK_SIZE) {
        std::cerr << "Error: Writing to block on disk\n";
    }
    close(fd);
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
        // Find the first set of contiguous blocks that can be allocated to the file by scanning
        // data blocks from 1 to 127.
        int block_number = 0;
        for (int i = 0; i < 16; i++) {
            char byte = super_block->free_block_list[i];
            for (int j=7; j>=0; j--) {
                if (block_number == 0) {// this is the superblock
                    block_number++;
                    continue;
                }
                int bit = ((byte >> j) & 1);
                if (bit == 0) {
                    contiguous_blocks.push_back(block_number);
                } else if (bit == 1) {
                    contiguous_blocks.clear();
                }
                if ((int)contiguous_blocks.size() == size) {
                    break;
                }
                block_number++;
            }

            if ((int)contiguous_blocks.size() == size) {
                break;
            }
        }

        if ((int)contiguous_blocks.size() != size) {
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
    available_inode->used_size = (uint8_t) size;
    available_inode->used_size |= 1UL << 7; // set most significant bit
    strncpy(available_inode->name, name, 5);

    write_superblock_to_disk();
}

void delete_directory(char name[5], uint8_t directory) {
    
}

void delete_file(Inode * inode) {
    char buff[BLOCK_SIZE] = {0};
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

void fs_delete(char name[5]) {
    Inode * inode = NULL;
    for (int i = 0; i < 126; i++) {
        inode = &(super_block->inode[i]);
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
        delete_directory(name, current_directory);
    } else {
        delete_file(inode);
    }

    write_superblock_to_disk();
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
        } else if (isValid && !isMounted) {
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
        } else if (isValid && !isMounted) {
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
        } else if (stoi(arguments[1]) < 0 || stoi(arguments[1]) > 127) {
            isValid = false;
        } else if (isValid && !isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        }
        
    } else if (command.compare("W") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 0 || stoi(arguments[1]) > 127) {
            isValid = false;
        } else if (isValid && !isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        }
        
    } else if (command.compare("B") == 0) {
        if (arguments.size() < 1) {
            isValid = false;
        } else if (isValid && !isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        }
        
    } else if (command.compare("L") == 0) {
        if (arguments.size() != 0) {
            isValid = false;
        } else if (isValid && !isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        }
        
    } else if (command.compare("E") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 1 || stoi(arguments[1]) > 127) {
            isValid = false;
        } else if (isValid && !isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        }
        
    } else if (command.compare("O") == 0) {
        if (arguments.size() != 0) {
            isValid = false;
        } else if (isValid && !isMounted) {
            std::cerr << "Error: No file system is mounted\n";
        }
        
    } else if (command.compare("Y") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (isValid && !isMounted) {
            std::cerr << "Error: No file system is mounted\n";
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
        std::vector<std::string> arguments = tokenize(command, " ");
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