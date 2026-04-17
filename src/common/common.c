//
// Created by placeja5 on 11/4/25.
//

#include "common.h"       // Own header (includes stdarg.h? No – add it here if needed)

// Global utilities implementation
// Provides logging functions and architecture helpers used throughout the project

// Global debug level variable (set by main.c from CLI -d flag)
int DEBUG_LEVEL = 0;

# ifndef UNIT_TESTS

// ANSI color codes (used only when writing to a terminal)
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_CYAN    "\033[1;36m"

/**
 * Logs an error message to stderr and exits the program.
 * Use for fatal errors that cannot be recovered from.
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (isatty(fileno(stderr))) {
        fprintf(stderr, COLOR_RED "[ERROR]" COLOR_RESET " ");
    } else {
        fprintf(stderr, "[ERROR] ");
    }
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
    // Skip if requested debug level is higher than configured DEBUG_LEVEL
    // e.g., if DEBUG_LEVEL=0 (no debug), skip all debug messages (level 1, 2, ...)
    if (DEBUG_LEVEL < level) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    if (isatty(fileno(stderr))) {
        fprintf(stderr, COLOR_CYAN "[DEBUG %d]" COLOR_RESET " ", level);
    } else {
        fprintf(stderr, "[DEBUG %d] ", level);
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);  // Immediate output for real-time debugging
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (isatty(fileno(stdout))) {
        fprintf(stdout, COLOR_GREEN "[INFO]" COLOR_RESET " ");
    } else {
        fprintf(stdout, "[INFO] ");
    }
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
    fflush(stdout);
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (isatty(fileno(stderr))) {
        fprintf(stderr, COLOR_YELLOW "[WARN]" COLOR_RESET " ");
    } else {
        fprintf(stderr, "[WARN] ");
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
}

#endif

