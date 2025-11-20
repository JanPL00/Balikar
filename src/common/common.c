//
// Created by placeja5 on 11/4/25.
//

#include "common.h"       // Own header (includes stdarg.h? No – add it here if needed)

// Global utilities implementation
// Provides logging functions and architecture helpers used throughout the project

# ifndef UNIT_TESTS

/**
 * Logs an error message to stderr and exits the program.
 * Use for fatal errors that cannot be recovered from.
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, fmt, args);  // Print formatted message
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);  // Fatal error - terminate program
}

/**
 * Logs a debug message to stderr if level meets threshold.
 * Only prints if level >= DEBUG_LEVEL (controlled by -d flag).
 * @param level Debug verbosity level (1=basic, 2=verbose, etc.)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
void log_debug(int level, const char *fmt, ...) {
    // Skip if debug level is below threshold
    if (level < DEBUG_LEVEL) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[DEBUG %d] ", level);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);  // Immediate output for real-time debugging
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

