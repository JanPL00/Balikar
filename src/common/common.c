//
// Created by placeja5 on 11/4/25.
//

#include "common.h"       // Own header (includes stdarg.h? No – add it here if needed)
#include <stdarg.h>       // For va_list and variadic args
#include <stdio.h>        // Already in common.h, but explicit for clarity

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