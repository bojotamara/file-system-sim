#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "FileSystem.h"
#include "ConsistencyCheck.h"
#include "InodeHelper.h"
#include "IO.h"
#include "Util.h"

// Global variables
Super_block * super_block = NULL;
std::string disk_name = "";
uint8_t current_directory = ROOT;
uint8_t buffer[BLOCK_SIZE] = {0};

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

std::vector<int> get_contiguous_blocks(int size, int start_block = 1, int end_block = 128) {
    // Find the first set of contiguous blocks that can be allocated to the file by scanning
    // data blocks from 1 to 127.
    std::vector<int> contiguous_blocks;
    int block_number = start_block;
    while (block_number < end_block) {
        if (is_block_free(block_number, super_block)) {
            contiguous_blocks.push_back(block_number);
        } else {
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
        std::cerr << "Error: File system in " << new_disk_name << " is inconsistent";
        std::cerr << " (error code: " << errorCode << ")\n";
        delete temp_super_block;
    }

    close(fd);
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
            allocate_block_in_free_list(block, super_block);
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

    write_superblock_to_disk(disk_name, super_block);
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
        delete_directory(inodeIndex, disk_name, super_block);
    } else {
        delete_file(inode, disk_name, super_block);
    }

    write_superblock_to_disk(disk_name, super_block);
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
    int fd = open(disk_name.c_str(), O_RDWR);
    read_from_block(fd, buffer, inode->start_block + block_num);
    close(fd);
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
    int fd = open(disk_name.c_str(), O_RDWR);
    write_to_block(fd, buffer, inode->start_block + block_num);
    close(fd);
}

void fs_buff(uint8_t buff[1024], int size) {
    //Flush the buffer
    for (int i = 0; i < BLOCK_SIZE; i ++) {
        buffer[i] = 0;
    }
    memcpy(buffer, buff, size);
}

void fs_ls() {
    // Maps directory to its children
    std::map<uint8_t, std::vector<uint8_t>> directory;

    for (int i = 0; i < 126; i++) {
        Inode * inode = &(super_block->inode[i]);
        uint8_t parent_dir = get_parent_dir(*inode);

        if (is_inode_used(*inode)) {
            if (is_inode_dir(*inode)) {
                auto it = directory.find(i);
                if (it == directory.end()) { // directory is not in the map yet
                    std::vector<uint8_t> directoryContents;
                    directory.insert({i, directoryContents});
                }
            }

            auto it = directory.find(parent_dir);
            if (it != directory.end()) {
                // directory in map
                it->second.push_back(i);
            } else {
                // directory not in map
                std::vector<uint8_t> directoryContents;
                directoryContents.push_back(i);
                directory.insert({parent_dir, directoryContents});
            }

        }
    }

    std::vector<uint8_t> current_contents;
    std::vector<uint8_t> parent_contents;

    auto it_current = directory.find(current_directory);
    if (it_current != directory.end()) {
        current_contents =  it_current->second;
    }

    if (current_directory == ROOT) {
        parent_contents = current_contents;
    } else {
        uint8_t parent_dir = get_parent_dir(super_block->inode[current_directory]);
        auto it_parent = directory.find(parent_dir);
        if (it_parent != directory.end()) {
            parent_contents =  it_parent->second;
        }
    }

    printf("%-5s %3d\n", ".", (int) current_contents.size() + 2);
    printf("%-5s %3d\n", "..", (int) parent_contents.size() + 2);

    for (auto inode_index: current_contents) {
        Inode * inode = &(super_block->inode[inode_index]);
        if (is_inode_dir(*inode)) {
            int num_children = 0;
            auto it = directory.find(inode_index);
            if (it != directory.end()) {
                num_children = it->second.size();
            }
            printf("%-5.5s %3d\n", inode->name, num_children + 2);
        } else {
            printf("%-5.5s %3d KB\n", inode->name, get_inode_size(*inode));
        }
    }

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
        int fd = open(disk_name.c_str(), O_RDWR);
        for (int i = inode->start_block + new_size; i < inode->start_block + current_size; i++) {
            write_to_block(fd, buff, i);
            free_block_in_free_list(i, super_block);
        }
        close(fd);
    } else if (new_size > current_size) {
        std::vector<int> contiguous_blocks = get_contiguous_blocks(new_size - current_size, inode->start_block + current_size, inode->start_block + new_size);

        //Not enough blocks in the next blocks
        if (contiguous_blocks.empty()) {
            for (int i = inode->start_block; i < inode->start_block + current_size; i++) {
                free_block_in_free_list(i, super_block);
            }
            contiguous_blocks = get_contiguous_blocks(new_size);
            if (contiguous_blocks.empty()) {
                for (int i = inode->start_block; i < inode->start_block + current_size; i++) {
                    allocate_block_in_free_list(i, super_block);
                }
                std::cerr << "Error: File " << name << " cannot expand to size " << new_size << std::endl;
                return;
            } else {
                copy_file_to_blocks(inode, disk_name, super_block, contiguous_blocks);
            }
        } else {// Enough blocks available
            uint8_t buff[BLOCK_SIZE] = {0};
            int fd = open(disk_name.c_str(), O_RDWR);
            for (auto block: contiguous_blocks) {
                write_to_block(fd, buff, block);
                allocate_block_in_free_list(block, super_block);
            }
            close(fd);
        }
    } else {
        return;
    }

    set_inode_size(inode, new_size);
    write_superblock_to_disk(disk_name, super_block);
}

void fs_defrag() {
    std::map<uint8_t, Inode*> sortedInodes;
    for (int i = 0; i < 126; i++) {
        Inode * inode = &(super_block->inode[i]);
        if (is_inode_used(*inode)) {
            sortedInodes.insert({inode->start_block, inode});
        }
    }

    int fd = 0;
    if (!sortedInodes.empty()) {
        fd = open(disk_name.c_str(), O_RDWR);
    }

    for (auto f: sortedInodes) {
        Inode * inode = f.second;
        uint8_t new_start_block = f.first;
        while (new_start_block > 0) {
            if (!is_block_free(new_start_block - 1, super_block)) {
                break;
            }
            new_start_block--;
        }
        // Can't move this file left
        if (new_start_block == f.first) {
            continue;
        }

        int size = get_inode_size(*inode);
        for (int i = 0; i < size; i++) {
            int old_block = inode->start_block + i;
            int new_block = new_start_block + i;

            uint8_t buff[BLOCK_SIZE] = {0};
            read_from_block(fd, buff, old_block);
            write_to_block(fd, buff, new_block);
            uint8_t clear[BLOCK_SIZE] = {0};
            write_to_block(fd, clear, old_block);

            free_block_in_free_list(old_block, super_block);
            allocate_block_in_free_list(new_block, super_block);
        }

        inode->start_block = new_start_block;
    }

    if (!sortedInodes.empty()) {
        close(fd);
        write_superblock_to_disk(disk_name, super_block);
    }
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
        } else {
            fs_ls();
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
        } else {
            fs_defrag();
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