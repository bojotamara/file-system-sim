#pragma once

#include <string>
#include <vector>

#include "FileSystem.h"

void allocate_block_in_free_list(int block_number, Super_block * super_block);
void free_block_in_free_list(int block_number, Super_block * super_block);
bool is_block_free(int block_number, Super_block * super_block);
void write_superblock_to_disk(std::string disk_name, Super_block * super_block);
void write_to_block(int fd, uint8_t buff[BLOCK_SIZE], int block_number);
void read_from_block(int fd, uint8_t buff[BLOCK_SIZE], int block_number);
void delete_file(Inode * inode, std::string disk_name, Super_block * super_block);
void delete_directory(int directory, std::string disk_name, Super_block * super_block);
void move_file_to_blocks(Inode * inode, std::string disk_name, Super_block * super_block, std::vector<int> destination_blocks);