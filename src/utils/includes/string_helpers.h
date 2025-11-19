//
// Created by placeja5 on 11/5/25.
//

#ifndef IMPL_STRING_HELPERS_H
#define IMPL_STRING_HELPERS_H

#include "../../common/common.h"

// Public string helpers API

// Convert a solvable to a human-readable string.
// Format: "name-version.architecture" (e.g., "glibc-2.35.x86_64")
// Allocates memory - caller must free the returned string.
// pool: Pool containing the solvable
// p: Solvable ID
// Returns allocated string representation, or NULL on error
char *solvable_to_string(Pool *pool, Id p);

// Convert a dependency ID to a human-readable string.
// Formats dependency with name, version constraints, and operators.
// Examples: "glibc >= 2.35", "bash", "libssl.so.1.1"
// Allocates memory - caller must free the returned string.
// pool: Pool containing the dependency
// dep: Dependency ID
// Returns allocated string representation, or NULL on error
char *dep_to_string(Pool *pool, Id dep);

#endif //IMPL_STRING_HELPERS_H