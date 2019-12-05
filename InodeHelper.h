#pragma once

#include "FileSystem.h"

bool is_inode_used(Inode inode);
uint8_t get_inode_size(Inode inode);
bool is_inode_dir(Inode inode);
uint8_t get_parent_dir(Inode inode);
void set_inode_size(Inode * inode, int size);
bool is_name_set(Inode inode);