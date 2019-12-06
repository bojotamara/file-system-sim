
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "InodeHelper.h"
#include "IO.h"

/**
 * @brief Allocate the block in the superblock's free list by setting its bit to 1
 * 
 * @param block_number - The block index to allocate
 * @param super_block - The super block with the free list to change
 */
void allocate_block_in_free_list(int block_number, Super_block * super_block) {
    int free_block_list_index = block_number/8;
    int bit_number = 7 - (block_number % 8);

    char * byte = &(super_block->free_block_list[free_block_list_index]);
    *byte |= 1UL << bit_number;
}

/**
 * @brief Free the block in the superblock's free list by setting its bit to 0
 * 
 * @param block_number - The block index to free
 * @param super_block - The super block with the free list to change
 */
void free_block_in_free_list(int block_number, Super_block * super_block) {
    int free_block_list_index = block_number/8;
    int bit_number = 7 - (block_number % 8);

    char * byte = &(super_block->free_block_list[free_block_list_index]);
    *byte &= ~(1UL << bit_number);
}

/**
 * @brief Determine if the provided block is free in the superblock's free list
 * 
 * @param block_number - The block index to check
 * @param super_block - The super block with the free list to check
 * @return True if the block is free (the bit is 0). False if it is allocated (the bit is 1).
 */
bool is_block_free(int block_number, Super_block * super_block) {
    int free_block_list_index = block_number/8;
    int bit_number = 7 - (block_number % 8);
    char byte = super_block->free_block_list[free_block_list_index];
    int bit = ((byte >> bit_number) & 1);

    return !bit;
}

/**
 * @brief Write the provided superblock to the provided disk. The superblock should be the
 * first block on the disk.
 * 
 * @param disk_name - The disk to write the superblock to
 * @param super_block - The super block
 */
void write_superblock_to_disk(std::string disk_name, Super_block * super_block) {
    int fd = open(disk_name.c_str(), O_RDWR);
    int sizeWritten = write(fd, super_block, BLOCK_SIZE);
    if (sizeWritten < BLOCK_SIZE) {
        std::cerr << "Error: Writing superblock back to disk\n";
    }
    close(fd);
}

/**
 * @brief Write the buffer array to the block on the disk provided by the file descriptor fd.
 * The disk needs to be opened before the call of this function.
 * 
 * @param fd - The file descriptor of the disk to write to
 * @param buff - The contents to write to the block
 * @param block_number - The index of the block to write to
 */
void write_to_block(int fd, uint8_t buff[BLOCK_SIZE], int block_number) {
    int offset = BLOCK_SIZE*block_number;
    int sizeWritten = pwrite(fd, buff, BLOCK_SIZE, offset);
    if (sizeWritten < BLOCK_SIZE) {
        std::cerr << "Error: Writing to block on disk\n";
    }
}

/**
 * @brief Read the block on the disk provided by the file descriptor fd into the provided buffer array.
 * The disk needs to be opened before the call of this function.
 * 
 * @param fd - The file descriptor of the disk to read from
 * @param buff - The array to read the block into
 * @param block_number - The index of the block to read from
 */
void read_from_block(int fd, uint8_t buff[BLOCK_SIZE], int block_number) {
    int offset = BLOCK_SIZE*block_number;
    int sizeRead = pread(fd, buff, BLOCK_SIZE, offset);
    if (sizeRead < BLOCK_SIZE) {
        std::cerr << "Error: Reading block from disk\n";
    }
}

/**
 * @brief Delete the file represented by the inode. Clears the inode bits, frees the block in the
 * free block list, and zeros out the contents on the disk
 * 
 * @param inode - The inode representing the file to delete
 * @param disk_name - The disk to delete the file from
 * @param super_block - The super block to update
 */
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

/**
 * @brief Delete the directory. A recursive function that deletes all the files within a directory
 * and deletes the directories within itself by calling this function again. Clears the inode bits as well.
 * 
 * @param directory - The directory to delete - represented by the index of the directory in the inode list
 * @param disk_name - The disk to delete the directory from
 * @param super_block - The super block to update
 */
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

/**
 * @brief Move the file represented by the inode to the provided destination blocks. Updates the superblock's
 * free list, copies the old block on disk to the new one, and zeroes out the old block.
 * 
 * @param inode - The inode representing the file to move
 * @param disk_name - The disk to update
 * @param super_block - The super block to update
 * @param destination_blocks - The blocks to move the file to
 */
void move_file_to_blocks(Inode * inode, std::string disk_name, Super_block * super_block, std::vector<int> destination_blocks) {
    for (auto block: destination_blocks) {
        allocate_block_in_free_list(block, super_block);
    }
    int currentSize = get_inode_size(*inode);
    int destinationIndex = 0;
    int fd = open(disk_name.c_str(), O_RDWR);
    for (int i = inode->start_block; i < inode->start_block + currentSize; i++) {
        uint8_t buff[BLOCK_SIZE] = {0};
        read_from_block(fd, buff, i);

        uint8_t clear[BLOCK_SIZE] = {0};
        write_to_block(fd, clear, i);

        write_to_block(fd, buff, destination_blocks[destinationIndex]);
        destinationIndex++;
    }
    close(fd);
    inode->start_block = destination_blocks[0];
}