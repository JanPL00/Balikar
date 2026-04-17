//
// Command Context - Centralized state management
// Eliminates global variables, enables testing via dependency injection
//

#ifndef BALIKAR_COMMAND_CONTEXT_H
#define BALIKAR_COMMAND_CONTEXT_H

#include "../../common/common.h"
#include "command.h"
#include <jansson.h>

/**
 * CLI configuration parsed from command-line arguments.
 * Uses pointers to argv strings where possible (no duplication).
 * Only allocated strings are marked with _allocated suffix.
 */
typedef struct {
    // Debug and behavior flags
    int debug_level;         // Verbosity (0=quiet, 1=debug, 2=verbose)
    int dry_run;             // Simulate without executing
    int no_deps;             // Download without dependencies
    
    // Path configuration (allocated via strdup, must be freed)
    char *staging_dir_allocated;     // Custom staging dir (or NULL for default)
    char *root_path_allocated;       // Target root for install/remove (or NULL for /)
    char *build_tmp_dir_allocated;   // Custom build temp dir (or NULL for default)
    char *repo_path_allocated;       // Repository URL/path (or NULL for config default)
    char *output_dir_allocated;      // Output dir for download command
    
    // Package specification (allocated)
    char *local_package_allocated;   // Local package file path
    char *clean_cache_pkg_allocated; // Package name for cache cleaning
    
    // Command arguments (point directly to argv - DO NOT FREE)
    const char *command;      // Main command (install, build, etc.)
    const char *package_name; // Package name argument
    
} CLIConfig;

/**
 * Lazy-loaded runtime resources.
 * Initialized on first use to minimize memory footprint.
 * Commands that don't need libsolv (e.g., "repo list") don't allocate these.
 */
typedef struct {
    Pool *pool;   // NULL until first use (via context_ensure_pool)
    Repo *repo;   // NULL until first use (via context_ensure_repo)
} RuntimeResources;

/**
 * Subsystem facade - function pointers for dependency injection.
 * Enables testing with mock implementations.
 * Default: points to real implementations from parser/solver/downloader/builder/installer.
 */
typedef struct {
    // Parser subsystem
    Repo* (*load_repository)(Pool*, const char*);
    int (*generate_repodata)(const char*);
    
    // Solver subsystem  
    Solver* (*create_solver)(Pool*);
    Queue* (*solve_dependencies)(Solver*, const char*, Pool*, Repo*, const char*, 
                                  json_t* (*)(Repodata*, Id, Pool*));
    
    // Downloader subsystem
    FILE* (*download_package)(Solvable*, const char*, Pool*, json_t*, const char*);
    int (*download_extras)(json_t*, const char*, const char*, const char*, const char*);
    
    // Builder subsystem
    int (*commit_element)(Transaction*, Id, FILE*, Pool*, int, int, json_t*);
    
    // Installer subsystem
    int (*install_package_with_reason)(const char*, const char*, const char*, const char*, const char*);
    int (*erase_package)(const char*, const char*);
    int (*is_package_installed)(const char*, const char*);
    
} SubsystemFacade;

/**
 * Command Context - complete state for command execution.
 * Single source of truth, passed to all command handlers.
 * 
 * Memory efficiency:
 * - Base size: ~500 bytes (config + pointers)
 * - Pool/Repo: lazy-loaded, NULL by default
 * - Total overhead: <1 KB for commands without libsolv
 */
typedef struct command_context {
    CLIConfig config;           // Parsed CLI arguments
    RuntimeResources resources; // Lazy-loaded pool/repo
    SubsystemFacade subsystems; // Facade for testability
    
    int exit_code;              // Command exit code (0=success)
    
} CommandContext;

// ========================================
// Context Lifecycle Management
// ========================================

// Create new command context with default values
CommandContext* command_context_create(void);

// Initialize subsystem facade with real implementations
void command_context_init_subsystems(CommandContext *ctx);

// Parse CLI arguments into context config
// Returns: 0 on success, non-zero on parse error
int command_context_parse_args(CommandContext *ctx, int argc, char **argv);

// Lazy-load pool if not already initialized
// Returns: pool pointer or NULL on error
Pool* command_context_ensure_pool(CommandContext *ctx);

// Lazy-load repository if not already initialized
// Requires pool to be initialized first
// Returns: repo pointer or NULL on error
Repo* command_context_ensure_repo(CommandContext *ctx);

// Destroy context and free all allocated resources
void command_context_destroy(CommandContext *ctx);

// ========================================
// Context Helper Functions
// ========================================

// Get staging directory (from config or default)
const char* command_context_get_staging_dir(CommandContext *ctx);

// Get root path (from config or default "/")
const char* command_context_get_root_path(CommandContext *ctx);

// Get repository path (from config or primary repo from repos.conf)
const char* command_context_get_repo_path(CommandContext *ctx);

#endif // BALIKAR_COMMAND_CONTEXT_H
