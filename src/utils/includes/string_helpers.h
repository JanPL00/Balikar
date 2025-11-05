//
// Created by placeja5 on 11/5/25.
//

#ifndef IMPL_STRING_HELPERS_H
#define IMPL_STRING_HELPERS_H

#include "../../common/common.h"


// ========================================
// STRING HELPERS PROTOTYPES
// ========================================

/**
 * Convert a Solvable ID to a readable string in format "name-version.arch".
 * Allocates memory - caller must free().
 * @param pool The libsolv Pool.
 * @param p The Solvable ID.
 * @return Allocated string, or NULL on error.
 */
char *solvable_to_string(Pool *pool, Id p);

/**
 * Convert a dependency ID to a readable string (e.g., "requires: pkg >=1.0").
 * Allocates memory - caller must free().
 * @param pool The libsolv Pool.
 * @param dep The dependency ID.
 * @return Allocated string, or NULL on error.
 */
char *dep_to_string(Pool *pool, Id dep);

#endif //IMPL_STRING_HELPERS_H