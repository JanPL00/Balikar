//
// Created by placeja5 on 11/4/25.
//

#include "common.h"       // Own header (includes stdarg.h? No – add it here if needed)
#include <stdarg.h>       // For va_list and variadic args
#include <stdio.h>        // Already in common.h, but explicit for clarity
#include <solv/poolarch.h>
// ========================================
// GLOBAL UTILS IMPLEMENTATION
// ========================================

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, fmt, args);  // Variadic print
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);  // Simple exit(1); use ERR_SOLVE_FAIL if you want specific codes
}

void log_debug(int level, const char *fmt, ...) {
    if (level < DEBUG_LEVEL) {
        return;  // Skip if below threshold
    }
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[DEBUG %d] ", level);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);  // Ensure immediate output for debugging
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "[INFO] ");   // Stdout místo stderr pro info
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
    fflush(stdout);
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[WARN] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
}


const char *get_pool_arch(const Pool *pool) {

    if (!pool) return "unknown";

    if (pool->id2arch && pool->lastarch > 0) {
        return pool_id2str(pool, pool->lastarch);
    }

    // Zkus získat z repodata
    if (pool->solvables && pool->nsolvables > 0) {
        for (int i = 1; i < pool->nsolvables; i++) {
            Solvable *s = pool->solvables + i;
            if (s->name && s->arch) {
                const char *arch = pool_id2str(pool, s->arch);
                if (arch && strcmp(arch, "noarch") != 0) {
                    return arch;
                }
            }
        }
    }
    //Fallback default
    return "x86_64";
}