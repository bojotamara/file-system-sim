#include "InodeHelper.h"

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
