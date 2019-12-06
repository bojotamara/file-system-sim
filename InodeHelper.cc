#include "InodeHelper.h"

/**
 * @brief Checks if the passed in inode is currently being used.
 * 
 * @param Inode - The inode to check
 * @return True if the inode is in use. False otherwise.
 */
bool is_inode_used(Inode inode) {
    return (inode.used_size >> 7) & 1;
}

/**
 * @brief Returns the size of file represented by the inode
 * 
 * @param Inode - The inode to to get the size off
 * @return The number of blocks allocated to the file
 */
uint8_t get_inode_size(Inode inode) {
    return (inode.used_size & ~(1UL << 7));
}

/**
 * @brief Checks if the inode represents a directory
 * 
 * @param Inode - The inode to check
 * @return True if the inode represents a directory. False if it represents a file.
 */
bool is_inode_dir(Inode inode) {
    return (inode.dir_parent  >> 7) & 1;
}

/**
 * @brief Gets the parent directory of the file/directory the inode represents
 * 
 * @param Inode - The inode to get the parent directory of
 * @return The parent directory of the inode. Represented by the index of the parent directory
 * in the inode list
 */
uint8_t get_parent_dir(Inode inode) {
    return (inode.dir_parent & ~(1UL << 7));
}

/**
 * @brief Set the number of blocks allocated to the file the inode represents.
 * 
 * @param Inode - The inode to set the size of
 * @param size - The new size of the file
 */
void set_inode_size(Inode * inode, int size) {
    inode->used_size = (uint8_t) size;
    inode->used_size |= 1UL << 7; // set most significant bit
}

/**
 * @brief Determines whether the inode has a name set. The name is set if there is at least one
 * non-zero char in the name.
 * 
 * @param Inode - The inode to check the name of
 * @return True if the name is set. False otherwise.
 */
bool is_name_set(Inode inode) {
    for (int i = 0; i < 5; i++) {
        if (inode.name[i] != 0) {
            return true;
        }
    }
    return false;
}
