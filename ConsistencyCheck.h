#pragma once

#include "FileSystem.h"

/**
 * @brief Checks if the provided super_block is consistent. Performs 6 different
 * checks. Returns the error code of the check that failed.
 *
 * @param super_block - The super_block to check
 * @return The error code of the check that failed
 */
int check_consistency(Super_block * super_block);