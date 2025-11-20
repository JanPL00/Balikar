//
// Created by placeja5 on 11/5/25.
//

#include "includes/string_helpers.h"

/**
 * Converts a solvable to a human-readable string.
 * Format: "name-version.architecture" (e.g., "glibc-2.35.x86_64")
 * Caller must free the returned string.
 * @param pool Pool containing the solvable
 * @param p Solvable ID
 * @return Allocated string representation, or NULL on error
 */
char *solvable_to_string(Pool *pool, Id p) {
    if (!pool || p <= 0) {
        log_error("Invalid pool or solvable ID in solvable_to_string.");
        return NULL;
    }

    // Get solvable from pool
    Solvable *s = pool_id2solvable(pool, p);
    if (!s) {
        log_error("Solvable not found for ID %d.", p);
        return NULL;
    }

    // Extract package components
    const char *name = pool_id2str(pool, s->name);  // Package name
    const char *evr = pool_id2evr(pool, s->evr);    // Epoch:Version-Release
    const char *arch = pool_id2str(pool, s->arch);  // Architecture

    if (!name || !evr || !arch) {
        log_error("Missing name, evr, or arch for solvable ID %d.", p);
        return NULL;
    }

    // Allocate memory for formatted string: "name-evr.arch\0"
    size_t len = strlen(name) + strlen(evr) + strlen(arch) + 3;
    char *str = malloc(len);
    if (!str) {
        log_error("Memory allocation failed in solvable_to_string.");
        return NULL;
    }

    // Format: name-version.architecture
    snprintf(str, len, "%s-%s.%s", name, evr, arch);
    return str;
}

/**
 * Converts a dependency ID to a human-readable string.
 * Formats dependency with name, version constraints, and operators.
 * Examples: "glibc >= 2.35", "bash", "libssl.so.1.1"
 * Caller must free the returned string.
 * @param pool Pool containing the dependency
 * @param dep Dependency ID
 * @return Allocated string representation, or NULL on error
 */
char *dep_to_string(Pool *pool, Id dep) {
    if (!pool || dep <= 0) {
        log_error("Invalid pool or dep ID in dep_to_string.");
        return NULL;
    }

    // Get dependency string from pool
    // pool_dep2str returns internal string, must be duplicated
    const char *dep_str = pool_dep2str(pool, dep);
    if (!dep_str) {
        log_error("Dependency not found for ID %d.", dep);
        return NULL;
    }

    // Duplicate string for caller (pool_dep2str returns internal pointer)
    char *str = strdup(dep_str);
    if (!str) {
        log_error("Memory allocation failed in dep_to_string.");
        return NULL;
    }

    return str;
}