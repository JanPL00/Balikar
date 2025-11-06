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

# ifndef UNIT_TESTS

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

#endif


const char *get_pool_arch(const Pool *pool) {

    if (!pool) {
        return "unknown";
    }

    // Přímý přístup k lastarch - toto je správný způsob
    if (pool->lastarch) {
        const char *arch = pool_id2str(pool, pool->lastarch);
        if (arch && strlen(arch) > 0) {
            return arch;
        }
    }

    // Fallback
    return "x86_64";
}