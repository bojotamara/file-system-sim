#include <set>
#include <vector>
#include <map>
#include <algorithm>
#include <string.h>

#include "IO.h"
#include "InodeHelper.h"
#include "ConsistencyCheck.h"


/**
 * @brief Performs the first consistency check. Blocks that are marked free in the free-space list
 * cannot be allocated to any file. Similarly, blocks marked in use in the free-space list must be
 * allocated to exactly one file.
 *
 * @param super_block - The super_block to check
 * @return True if the consistency check passes. False otherwise
 */
bool consistency_check_1(Super_block * super_block) {
    std::set<int> free_blocks;
    std::set<int> used_blocks;

    int block_number = 1;
    while (block_number < 128) {
        if (is_block_free(block_number, super_block)) {
            free_blocks.insert(block_number);
        } else {
            used_blocks.insert(block_number);
        }

        block_number++;
    }

    std::vector<int> inode_used_blocks;
    for (int i = 0; i < 126; i ++) {
        Inode inode = super_block->inode[i];
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

/**
 * @brief Performs the second consistency check. The name of every file/directory must be
 * unique in each directory
 *
 * @param super_block - The super_block to check
 * @return True if the consistency check passes. False otherwise
 */
bool consistency_check_2(Super_block * super_block) {
    // Maps directories to the files/directories within them
    std::map<uint8_t, std::vector<Inode>> directory;

    for (int i = 0; i < 126; i++) {
        Inode inode = super_block->inode[i];
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

/**
 * @brief Performs the third consistency check. If the state of an inode is free,
 * all bits in this inode must be zero. Otherwise, the name attribute stored in
 * the inode must have at least one bit that is not zero.
 *
 * @param super_block - The super_block to check
 * @return True if the consistency check passes. False otherwise
 */
bool consistency_check_3(Super_block * super_block) {
    for (int i = 0; i < 126; i++) {
        Inode inode = super_block->inode[i];

        if (is_inode_used(inode)) {
            for (int i = 0; i < 5; i++) {
                if (inode.name[i] != 0) {
                    return true;
                }
            }
            return false;
        } else {
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

/**
 * @brief Performs the fourth consistency check. The start block of every inode that is marked as a
 * file must have a value between 1 and 127 inclusive.
 *
 * @param super_block - The super_block to check
 * @return True if the consistency check passes. False otherwise.
 */
bool consistency_check_4(Super_block * super_block) {
    for (int i = 0; i < 126; i++) {
        Inode inode = super_block->inode[i];
        if (is_inode_used(inode) && !is_inode_dir(inode) && (inode.start_block < 1 || inode.start_block > 127)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Performs the fifth consistency check. The size and start block of an inode
 * that is marked as a directory must be zero.
 *
 * @param super_block - The super_block to check
 * @return True if the consistency check passes. False otherwise.
 */
bool consistency_check_5(Super_block * super_block) {
    for (int i = 0; i < 126; i++) {
        Inode inode = super_block->inode[i];
        if (is_inode_used(inode) && is_inode_dir(inode) && (inode.start_block != 0 || get_inode_size(inode) != 0)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Performs the sixth consistency check. For every inode, the index of its
 * parent inode cannot be 126. Moreover, if the index of the parent inode is between
 * 0 and 125 inclusive, then the parent inode must be in use and marked as a directory.
 *
 * @param super_block - The super_block to check
 * @return True if the consistency check passes. False otherwise.
 */
bool consistency_check_6(Super_block * super_block) {
    for (int i = 0; i < 126; i++) {
        Inode inode = super_block->inode[i];
        if (is_inode_used(inode)) {
            uint8_t parent_dir = get_parent_dir(inode);
            if (parent_dir == 126) {
                return false;
            } else if (parent_dir >= 0 && parent_dir <= 125) {
                Inode parent_inode = super_block->inode[parent_dir];
                if (!is_inode_used(parent_inode) || !is_inode_dir(parent_inode)) {
                    return false;
                }
            }
        }
    }
    return true;
}

/**
 * @brief Checks if the provided super_block is consistent. Performs 6 different
 * checks. Returns the error code of the check that failed.
 *
 * @param super_block - The super_block to check
 * @return The error code of the check that failed
 */
int check_consistency(Super_block * super_block) {
    int errorCode = 0;

    if (!consistency_check_1(super_block)) {
        errorCode = 1;
    } else if (!consistency_check_2(super_block)) {
        errorCode = 2;
    } else if (!consistency_check_3(super_block)) {
        errorCode = 3;
    } else if (!consistency_check_4(super_block)) {
        errorCode = 4;
    } else if (!consistency_check_5(super_block)) {
        errorCode = 5;
    } else if (!consistency_check_6(super_block)) {
        errorCode = 6;
    }

    return errorCode;
}