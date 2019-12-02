#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <unordered_map>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "FileSystem.h"
#include "util.h"

Super_block super_block;

bool is_inode_used(Inode inode) {
    return (inode.used_size >> 7) & 1;
}

uint8_t get_inode_size(Inode inode) {
    return (inode.used_size & ~(1UL << 7));
}

// Blocks that are marked free in the free-space list cannot be allocated to any file. Similarly, blocks
// marked in use in the free-space list must be allocated to exactly one file.
bool consistency_check_1(Super_block * temp_super_block) {
    std::set<uint8_t> free_blocks;
    std::set<uint8_t> used_blocks;
    uint8_t block_number = 0;
    for (int i = 0; i < 16; i++) {
        char byte = temp_super_block->free_block_list[i];
        for (int i=7; i>=0; i--) {
            int bit = ((byte >> i) & 1);
            if (bit == 0) {
                free_blocks.insert(block_number);
            } else if (bit == 1) { // In use
                used_blocks.insert(block_number);
            }
            block_number++;
        }
    }

    std::unordered_multimap<uint8_t, bool> inode_used_blocks;
    for (int i = 0; i < 126; i ++) {
        Inode inode = temp_super_block->inode[i];
        if (is_inode_used(inode)) {
            uint8_t fileSize = get_inode_size(inode);
            for (uint8_t i = inode.start_block; i < inode.start_block + fileSize; i++) {
                if (free_blocks.find(i) != free_blocks.end()) {
                    return false;
                }
                inode_used_blocks.insert(std::pair<uint8_t, bool>(i, true));
            }
        }
    }

    for (auto f: used_blocks) {
        if ( f != 0 && inode_used_blocks.count(f) != 1) {
            return false;
        }
    }

    return true;
}

int check_consistency(Super_block * temp_super_block) {
    int errorCode = 0;

    if (!consistency_check_1(temp_super_block)) {
        errorCode = 1;
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
    int sizeRead = read(fd, temp_super_block, 1024);
    if (sizeRead < 1024) {
        std::cerr << "Error: Reading superblock during mount was not successful\n";
        delete temp_super_block;
        close(fd);
    }

    int errorCode = check_consistency(temp_super_block);

    if (errorCode != 0) {
        std::cout << "Error: File system in " << new_disk_name << " is inconsistent";
        std::cout << " (error code: " << errorCode << ")\n";
        close(fd);
        delete temp_super_block;
    }

    delete temp_super_block;
    close(fd);
}

bool runCommand(std::vector<std::string> arguments) {
    // Separate out the command and the arguments
    std::string command = arguments[0];
    arguments.erase(arguments.begin());
    bool isValid = true;

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
        }
    } else if (command.compare("D") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        }
        
    } else if (command.compare("R") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 0 || stoi(arguments[1]) > 127) {
            isValid = false;
        }
        
    } else if (command.compare("W") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 0 || stoi(arguments[1]) > 127) {
            isValid = false;
        }
        
    } else if (command.compare("B") == 0) {
        if (arguments.size() < 1) {
            isValid = false;
        }
        
    } else if (command.compare("L") == 0) {
        if (arguments.size() != 0) {
            isValid = false;
        }
        
    } else if (command.compare("E") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
        } else if (stoi(arguments[1]) < 1 || stoi(arguments[1]) > 127) {
            isValid = false;
        }
        
    } else if (command.compare("O") == 0) {
        if (arguments.size() != 0) {
            isValid = false;
        }
        
    } else if (command.compare("Y") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        } else if (arguments[0].size() > 5) {
            isValid = false;
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

    command_file.close();

    return 0;
}