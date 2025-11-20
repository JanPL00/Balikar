//
// Created by placeja5 on 11/4/25.
//

#ifndef IMPL_COMMON_H
#define IMPL_COMMON_H

#define _GNU_SOURCE

// ========================================
// 1. SHARED STANDARD INCLUDES (C stdlib)
// ========================================
#include <stdio.h>      // For I/O (printf, fopen)
#include <stdlib.h>     // For memory management (malloc, free, exit)
#include <string.h>     // For strings (strcpy, snprintf)
#include <unistd.h>     // For system calls (getenv, chdir for chroot)
#include <sys/utsname.h> // For architecture (uname in setarch)
#include <dirent.h>     // For directory recursion in parser (opendir, readdir)
#include <time.h>       // For timestamps in metadata (time(NULL))
#include  <check.h>
#include <sys/stat.h>
#include <getopt.h>
#include <errno.h>



// ========================================
// 2. SHARED EXTERNAL LIBRARIES (libsolv, jansson, curl)
// ========================================
// Libsolv – core for pool, solver, repo (use pkg-config for cflags/libs)
#include <solv/pool.h>          // Pool and Solvable
#include <solv/repo.h>          // Repo handling
#include <solv/solver.h>        // Solver and jobs
#include <solv/solverdebug.h>   // Solver debugging
#include <solv/queue.h>           // Pro Queue
#include <solv/transaction.h>   // Transactions
#include <solv/selection.h>     // For globs and provides
#include <solv/poolarch.h>      // Architecture
#include <solv/evr.h>           // Version comparison
#include <solv/knownid.h>       // For SOLVABLE_REQUIRES and other constants
#include <ctype.h>
#include <sys/wait.h>


// Jansson – for JSON parsing (index.json, repodata.json)
#include <jansson.h>            // json_t, json_load_file, etc.

// Conda extension for libsolv installed using miniconda
#include <solv/repo_conda.h>   // conda repo handling

// Curl – for downloader (fetching source tarballs)
#include <curl/curl.h>          // curl_easy_init, etc.

// Optional others (if needed): libarchive for tar extraction (but minimize – use system("tar"))

// ========================================
// 3. SHARED PROJECT UTILS NCLUDES
// ========================================
#include "../utils/includes/system_helpers.h"   // System helper functions
#include "../utils/includes/string_helpers.h"      // String helper functions
#include "../utils/includes/cli_helpers.h"      // CLI helper functions
#include "../utils/includes/debug.h"      // Debug helper functions
#include "../utils/includes/print.h"      // Print functions for logging



// ========================================
// 4. GLOBAL MACROS AND CONSTANTS
// ========================================
#define STAGING_DIR "$P"                // Staging directory for LFS/LLD (DESTDIR=$P)
#define CHROOT_PATH "/tmp/pm-chroot"    // Default chroot for installation (CLI-overridable)
#define MAX_RECURSION_DEPTH 10          // Recursion limit for mkdeps (prevents cycles)
#define REPO_METADATA "repodata.json"   // Name of aggregated file for Conda-like repos
#define PKG_METADATA "index.json"       // Name of metadata file in tar.xz
#define MAX_PATH_LEN 1024               // Buffer size for paths (snprintf)
#define DEBUG_LEVEL 0                   // Default debug level (CLI -d to increase)

// Error codes (enum-like macros for consistent error handling)
#define ERR_NO_REPO 1
#define ERR_SOLVE_FAIL 2
#define ERR_DOWNLOAD_FAIL 3
#define ERR_BUILD_FAIL 4

// ========================================
// 5. GLOBAL STRUCTURES AND TYPEDEFS
// ========================================
typedef struct {
    char *source;               // URL for download (from index.json)
    json_t *mkdeps;             // Build-time dependencies (json_array)
    json_t *extras;             // Patches/extras (json_array)
    int is_group;               // Boolean for meta-package
    int no_package;             // Boolean for virtual package
} CustomMeta;                   // Custom metadata for Solvable->userdata (Conda + LFS)

// ========================================
// 6. SHARED FUNCTION PROTOTYPES (only globally shared ones)
// ========================================

// Logging functions - used throughout the project

// Log fatal error message and exit program (exit code 1)
void log_error(const char *fmt, ...);

// Log debug message if level >= DEBUG_LEVEL (controlled by -d flag)
void log_debug(int level, const char *fmt, ...);

// Log informational message (always shown, to stdout)
void log_info(const char *fmt, ...);

// Log warning message (always shown, to stderr)
void log_warn(const char *fmt, ...);

// Get architecture string from libsolv pool
// Returns "x86_64", "i686", etc., or "unknown" if pool is NULL


#endif //IMPL_COMMON_H