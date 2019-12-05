
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "InodeHelper.h"
#include "IO.h"


void allocate_block_in_free_list(int block_number, Super_block * super_block) {
    int free_block_list_index = block_number/8;
    int bit_number = 7 - (block_number % 8);

    char * byte = &(super_block->free_block_list[free_block_list_index]);
    *byte |= 1UL << bit_number;
}

void free_block_in_free_list(int block_number, Super_block * super_block) {
    int free_block_list_index = block_number/8;
    int bit_number = 7 - (block_number % 8);

    char * byte = &(super_block->free_block_list[free_block_list_index]);
    *byte &= ~(1UL << bit_number);
}

bool is_block_free(int block_number, Super_block * super_block) {
    int free_block_list_index = block_number/8;
    int bit_number = 7 - (block_number % 8);
    char byte = super_block->free_block_list[free_block_list_index];
    int bit = ((byte >> bit_number) & 1);

    return !bit;
}

void write_superblock_to_disk(std::string disk_name, Super_block * super_block) {
    int fd = open(disk_name.c_str(), O_RDWR);
    int sizeWritten = write(fd, super_block, BLOCK_SIZE);
    if (sizeWritten < BLOCK_SIZE) {
        std::cerr << "Error: Writing superblock back to disk\n";
    }
    close(fd);
}

void write_to_block(int fd, uint8_t buff[BLOCK_SIZE], int block_number) {
    int offset = BLOCK_SIZE*block_number;
    int sizeWritten = pwrite(fd, buff, BLOCK_SIZE, offset);
    if (sizeWritten < BLOCK_SIZE) {
        std::cerr << "Error: Writing to block on disk\n";
    }
}

void read_from_block(int fd, uint8_t buff[BLOCK_SIZE], int block_number) {
    int offset = BLOCK_SIZE*block_number;
    int sizeRead = pread(fd, buff, BLOCK_SIZE, offset);
    if (sizeRead < BLOCK_SIZE) {
        std::cerr << "Error: Reading block from disk\n";
    }
}

void delete_file(Inode * inode, std::string disk_name, Super_block * super_block) {
    uint8_t buff[BLOCK_SIZE] = {0};
    int size = get_inode_size(*inode);
    int fd = open(disk_name.c_str(), O_RDWR);
    for (int i = inode->start_block; i < inode->start_block + size; i++) {
        write_to_block(fd, buff, i);
        free_block_in_free_list(i, super_block);
    }
    close(fd);

    inode->dir_parent = 0;
    inode->start_block = 0;
    inode->used_size = 0;
    for (int i = 0; i < 5; i++) {
        inode->name[i] = 0;
    }
}

// Recursive function
void delete_directory(int directory, std::string disk_name, Super_block * super_block) {
    for (int i = 0; i < 126; i++) {
        Inode * inode = &(super_block->inode[i]);
        if (is_inode_used(*inode) && get_parent_dir(*inode) == directory) {
            if (is_inode_dir(*inode)) {
                delete_directory(i, disk_name, super_block);
            } else {
                delete_file(inode, disk_name, super_block);
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

void copy_file_to_blocks(Inode * inode, std::string disk_name, Super_block * super_block, std::vector<int> destination_blocks) {
    for (auto block: destination_blocks) {
        allocate_block_in_free_list(block, super_block);
    }
    int currentSize = get_inode_size(*inode);
    int destinationIndex = 0;
    int fd = open(disk_name.c_str(), O_RDWR);
    for (int i = inode->start_block; i < inode->start_block + currentSize; i++) {
        uint8_t buff[BLOCK_SIZE] = {0};
        read_from_block(fd, buff, i);
        write_to_block(fd, buff, destination_blocks[destinationIndex]);
        destinationIndex++;
    }
    close(fd);
    inode->start_block = destination_blocks[0];
}