//
// Created by placeja5 on 11/2/25.
//
#include "includes/parser.h"
#include "includes/solver.h"
#include "includes/downloader.h"
#include "includes/builder.h"
#include "../utils/includes/system_helpers.h"
#include "../utils/includes/cli_helpers.h"
#include "../common/common.h"
#include "../common/config.h"
#include <getopt.h>

/**
 * CLI configuration structure.
 * Holds all command-line arguments and options parsed from user input.
 * Controls program behavior including debug level, staging, paths, and commands.
 */
typedef struct {
    int debug_level;         // Verbosity level for debugging output
    int dry_run;             // If 1, simulate actions without executing them
    int no_staging;          // If 1, disable staging directory usage
    char *staging_dir;       // Custom staging directory path
    char *chroot_path;       // Chroot environment path (for isolated builds)
    char *build_tmp_dir;     // Custom temporary directory for builds
    char *repo_path;         // Repository URL or local path
    char *local_package;     // Local package file path
    char *clean_cache_pkg;   // Package name for cache cleaning
    const char *command;     // Main command (install, build, resolve, etc.)
    const char *package_name;// Package or subcommand name
} cli_config_t;

/**
 * Parses command-line arguments and populates configuration structure.
 * Handles both short and long options using getopt_long.
 * Supports options for debugging, staging, paths, and various commands.
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @param cfg Configuration structure to populate
 */
static void parse_args(int argc, char **argv, cli_config_t *cfg) {
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"debug", no_argument, 0, 'd'},
        {"dry-run", no_argument, 0, 'D'},
        {"no-staging", no_argument, 0, 'S'},
        {"staging-dir", required_argument, 0, 's'},
        {"chroot-path", required_argument, 0, 'c'},
        {"build-tmp-dir", required_argument, 0, 't'},
        {"repo", required_argument, 0, 'r'},
        {"local-package", required_argument, 0, 'l'},
        {"pkg", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };
    
    int opt;
    // Parse all command-line options
    while ((opt = getopt_long(argc, argv, "hd", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':  // Help - display usage and exit
                print_usage(0);
                break;
            case 'd':  // Debug - increment debug level (can be used multiple times)
                cfg->debug_level++;
                break;
            case 'D':  // Dry-run - simulate without executing
                cfg->dry_run = 1;
                break;
            case 'S':  // No-staging - disable staging directory
                cfg->no_staging = 1;
                break;
            case 's':  // Staging-dir - custom staging directory
                cfg->staging_dir = strdup(optarg);
                break;
            case 'c':  // Chroot-path - chroot environment path
                cfg->chroot_path = strdup(optarg);
                break;
            case 't':  // Build-tmp-dir - custom build temporary directory
                cfg->build_tmp_dir = strdup(optarg);
                break;
            case 'r':  // Repo - repository URL or path
                cfg->repo_path = strdup(optarg);
                break;
            case 'l':  // Local-package - local package file
                cfg->local_package = strdup(optarg);
                break;
            case 'p':  // Pkg - package name for cache cleaning
                cfg->clean_cache_pkg = strdup(optarg);
                break;
            default:
                print_usage(1);
        }
    }
    
    // Parse positional arguments: command and package name
    // Command is the first non-option argument (e.g., "install", "build")
    if (optind < argc) {
        cfg->command = argv[optind++];
    }
    
    // Package name or subcommand is the second positional argument
    if (optind < argc) {
        cfg->package_name = argv[optind];
    }
    
    // Set default repository path if not specified via CLI
    // Load from configuration file and use primary repository
    if (!cfg->repo_path) {
        init_repo_config();
        cfg->repo_path = strdup(get_primary_repo_url());
    }
}

/**
 * Main entry point for Balikar package manager.
 * Orchestrates the complete workflow:
 * 1. Parse command-line arguments
 * 2. Handle special commands (repo, clean-cache)
 * 3. Initialize libsolv pool and load repository
 * 4. Execute requested command (install, build, resolve, etc.)
 * 5. Cleanup and return exit code
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code: 0 on success, error code on failure
 */
int main(int argc, char **argv) {
    // Initialize configuration structure with zeros
    cli_config_t cfg = {0};
    
    // Parse and validate command-line arguments
    parse_args(argc, argv, &cfg);
    
    if (!cfg.command) {
        log_error("No command specified. Use -h for help.");
        print_usage(1);
    }

    // Handle 'repo' commands early - they don't require pool/repository loading
    // These commands manage repository configuration: list, add, remove, enable, disable
    if (strcmp(cfg.command, "repo") == 0) {
        init_repo_config();
        
        if (!cfg.package_name) {
            log_error("Subcommand required for 'repo'. Use: list, add, remove, enable, disable");
            print_usage(1);
        }
        
        // Subcommand: list - display all configured repositories
        if (strcmp(cfg.package_name, "list") == 0) {
            list_repositories();
            free_repo_config();
            return 0;
        } 
        // Subcommand: add - add new repository with name, URL, and optional priority
        else if (strcmp(cfg.package_name, "add") == 0) {
            // Expects: name url [priority]
            // Priority defaults to 50 if not specified
            if (optind + 2 >= argc) {
                log_error("Usage: balikar repo add <name> <url> [priority]");
                return 1;
            }
            const char *name = argv[optind + 1];
            const char *url = argv[optind + 2];
            int priority = (optind + 3 < argc) ? atoi(argv[optind + 3]) : 50;
            
            if (add_repository(name, url, priority) == 0) {
                save_repo_config();
            }
            free_repo_config();
            return 0;
        } 
        // Subcommand: remove - remove repository by name
        else if (strcmp(cfg.package_name, "remove") == 0) {
            // Expects: name
            if (optind + 1 >= argc) {
                log_error("Usage: balikar repo remove <name>");
                return 1;
            }
            const char *name = argv[optind + 1];
            if (remove_repository(name) == 0) {
                save_repo_config();
            }
            free_repo_config();
            return 0;
        } 
        // Subcommand: enable - enable repository by name
        else if (strcmp(cfg.package_name, "enable") == 0) {
            if (optind + 1 >= argc) {
                log_error("Usage: balikar repo enable <name>");
                return 1;
            }
            const char *name = argv[optind + 1];
            if (enable_repository(name, 1) == 0) {
                save_repo_config();
            }
            free_repo_config();
            return 0;
        } 
        // Subcommand: disable - disable repository by name
        else if (strcmp(cfg.package_name, "disable") == 0) {
            if (optind + 1 >= argc) {
                log_error("Usage: balikar repo disable <name>");
                return 1;
            }
            const char *name = argv[optind + 1];
            if (enable_repository(name, 0) == 0) {
                save_repo_config();
            }
            free_repo_config();
            return 0;
        } else {
            log_error("Unknown repo subcommand: %s", cfg.package_name);
            print_usage(1);
        }
    }

    // Handle 'clean-cache' command early - doesn't require pool/repository
    // Cleans download cache: either specific package or entire cache directory
    if (strcmp(cfg.command, "clean-cache") == 0) {
        char *cache_dir = get_cache_dir();
        if (!cache_dir) {
            log_error("Failed to get cache directory.");
            return 1;
        }
        
        if (cfg.clean_cache_pkg) {
            // Clean cache for specific package
            // Removes: tar archives, sha256 checksums, and extras directory
            log_info("Cleaning cache for package: %s", cfg.clean_cache_pkg);
            
            // Remove tar archives and sha256 checksum files
            char pattern_cmd[2048];
            snprintf(pattern_cmd, sizeof(pattern_cmd), 
                     "rm -f \"%s/%s\"* 2>/dev/null", 
                     cache_dir, cfg.clean_cache_pkg);
            system(pattern_cmd);
            
            // Remove extras directory (patches, custom build scripts)
            char extras_dir[2048];
            snprintf(extras_dir, sizeof(extras_dir), "%s/extras/%s", cache_dir, cfg.clean_cache_pkg);
            char rm_cmd[4096];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\" 2>/dev/null", extras_dir);
            system(rm_cmd);
            
            log_info("Cache cleaned for package: %s", cfg.clean_cache_pkg);
        } else {
            // Clean entire cache directory
            // Requires user confirmation due to destructive nature
            log_info("Cleaning entire cache: %s", cache_dir);
            
            if (user_confirm("This will delete all cached downloads. Continue?", 0)) {
                char rm_cmd[2048];
                snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"/* 2>/dev/null", cache_dir);
                int rc = system(rm_cmd);
                if (rc == 0) {
                    log_info("Cache cleared successfully: %s", cache_dir);
                } else {
                    log_error("Failed to clear cache.");
                    free(cache_dir);
                    return 1;
                }
            } else {
                log_info("Cache cleaning cancelled.");
            }
        }
        
        free(cache_dir);
        return 0;
    }

    // Apply configuration settings from CLI arguments
    // These affect how builds are performed
    if (cfg.staging_dir) set_staging_dir(cfg.staging_dir);
    if (cfg.no_staging) set_staging_enabled(0);
    if (cfg.build_tmp_dir) set_build_tmp_dir(cfg.build_tmp_dir);
    
    log_info("Balikar Package Manager - Command: %s", cfg.command);
    if (cfg.dry_run) log_info("DRY-RUN mode enabled");
    
    // Initialize libsolv Pool - central data structure for dependency resolution
    Pool *pool = pool_create();
    if (!pool) {
        log_error("Failed to create pool.");
        return 1;
    }
    
    // Load repository metadata (from configuration or CLI override)
    // Repository contains package information and dependencies
    log_info("Using repository: %s", cfg.repo_path);
    Repo *repo = load_repository(pool, cfg.repo_path);
    if (!repo) {
        log_error("Failed to load repository from %s.", cfg.repo_path);
        pool_free(pool);
        return 1;
    }
    
    // Process main commands
    int exit_code = 0;
    
    // Command: install/build - complete workflow from dependency resolution to build
    if (strcmp(cfg.command, "install") == 0 || strcmp(cfg.command, "build") == 0) {
        if (!cfg.package_name) {
            log_error("Package name required for %s command.", cfg.command);
            print_usage(1);
        }
        
        log_info("Starting %s workflow for package: %s", cfg.command, cfg.package_name);
        
        // Step 1: Create solver for dependency resolution
        Solver *solv = create_solver(pool);
        if (!solv) {
            log_error("Failed to create solver.");
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // Step 2: Solve dependencies
        // Determines all packages needed to satisfy requirements
        Queue *solution = solve_dependencies(solv, cfg.package_name, pool, repo);
        if (!solution) {
            log_error("Dependency resolution failed for %s.", cfg.package_name);
            solver_free(solv);
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // Step 3: Create transaction from solution
        // Transaction contains ordered list of packages to install
        Transaction *trans = solver_create_transaction(solv);
        if (!trans) {
            log_error("Failed to create transaction.");
            queue_free(solution);
            solver_free(solv);
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // Display transaction to user
        if (trans->steps.count == 0) {
            log_info("No changes needed - package may already be resolved.");
        } else {
            display_transaction(trans);
        }
        
        // Step 4: Confirm transaction (unless in dry-run mode)
        if (!cfg.dry_run) {
            if (!user_confirm("Proceed with transaction?", 1)) {
                log_info("Transaction cancelled by user.");
                transaction_free(trans);
                queue_free(solution);
                solver_free(solv);
                goto cleanup;
            }
            
            // Step 5: Download and Build packages
            log_info("Downloading packages...");
            
            // Order transaction by dependencies (ensures correct build order)
            transaction_order(trans, 0);
            
            // Process each package in transaction
            for (int i = 0; i < trans->steps.count; i++) {
                Id p = trans->steps.elements[i];
                Id type = transaction_type(trans, p, SOLVER_TRANSACTION_RPM_ONLY);
                
                // Only process install transactions
                if (type == SOLVER_TRANSACTION_INSTALL || type == SOLVER_TRANSACTION_MULTIINSTALL) {
                    Solvable *s = pool_id2solvable(pool, p);
                    const char *pkg_name = pool_id2str(pool, s->name);
                    
                    // Get source download URL from repository metadata
                    Repodata *data = repo_id2repodata(repo, 1);  // First repodata
                    const char *source_url = get_custom_source(data, p, pool);
                    
                    if (!source_url) {
                        log_warn("No source URL for %s, skipping.", pkg_name);
                        continue;
                    }
                    
                    // Download source archive
                    log_info("Downloading %s from %s", pkg_name, source_url);
                    FILE *fp = download_package(s, source_url, pool, data, cfg.repo_path);
                    if (!fp) {
                        log_error("Download failed for %s.", pkg_name);
                        exit_code = ERR_DOWNLOAD_FAIL;
                        break;
                    }
                    
                    // Download extra files (patches, custom build scripts)
                    json_t *extras = get_custom_extras(data, p, pool);
                    const char *version = pool_id2str(pool, s->evr);
                    download_extras(extras, pkg_name, version, cfg.repo_path, NULL);
                    if (extras) json_decref(extras);
                    
                    // Build and install package
                    log_info("Building %s...", pkg_name);
                    int build_rc = commit_element(trans, data, p, fp, pool);
                    if (build_rc != 0) {
                        log_error("Build failed for %s.", pkg_name);
                        exit_code = ERR_BUILD_FAIL;
                        break;
                    }
                    
                    log_info("Successfully built %s.", pkg_name);
                }
            }
        } else {
            log_info("DRY-RUN: Skipping download and build.");
        }
        
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        
    } 
    // Command: resolve - display dependencies without downloading or building
    // Useful for previewing what would be installed
    else if (strcmp(cfg.command, "resolve") == 0) {
        if (!cfg.package_name) {
            log_error("Package name required for resolve command.");
            print_usage(1);
        }
        
        log_info("Resolving dependencies for: %s", cfg.package_name);
        
        // Create solver and resolve dependencies
        Solver *solv = create_solver(pool);
        if (!solv) {
            log_error("Failed to create solver.");
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        Queue *solution = solve_dependencies(solv, cfg.package_name, pool, repo);
        if (!solution) {
            log_error("Dependency resolution failed.");
            solver_free(solv);
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // Display what would be installed (without actually doing it)
        Transaction *trans = solver_create_transaction(solv);
        display_transaction(trans);
        
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        
    } 
    // Command: repos - display repository information
    // Shows all configured repositories and currently active one
    else if (strcmp(cfg.command, "repos") == 0) {
        // List all configured repositories from config
        list_repositories();
        
        // Show currently active repository and package count
        log_info("\nCurrently using: %s", cfg.repo_path);
        if (repo) {
            log_info("Packages available: %d", repo->nsolvables);
        }
        
    } 
    // Command: repo-update - refresh repository metadata
    // Downloads new metadata for remote repos or regenerates for local repos
    else if (strcmp(cfg.command, "repo-update") == 0) {
        log_info("Updating repository metadata: %s", cfg.repo_path);
        
        // Free current repo and pool to start fresh
        repo_free(repo, 1);
        pool_free(pool);
        
        // Recreate pool for clean state
        pool = pool_create();
        if (!pool) {
            log_error("Failed to recreate pool.");
            return 1;
        }
        
        // Update metadata:
        // - For remote repos: download new metadata files
        // - For local repos: regenerate metadata from packages
        int rc = update_repository_metadata(cfg.repo_path);
        if (rc != 0) {
            log_error("Failed to update repository metadata.");
            exit_code = ERR_SOLVE_FAIL;
            pool_free(pool);
            goto cleanup_no_repo;
        }
        
        log_info("Reloading repository...");
        
        // Reload repository with fresh metadata
        repo = load_repository(pool, cfg.repo_path);
        if (!repo) {
            log_error("Failed to reload repository.");
            exit_code = ERR_SOLVE_FAIL;
            pool_free(pool);
            goto cleanup_no_repo;
        }
        
        log_info("Repository reloaded: %d packages found.", repo->nsolvables);
        
    } else {
        log_error("Unknown command: %s", cfg.command);
        print_usage(1);
    }
    
cleanup:
    // Cleanup: free repository and pool resources
    repo_free(repo, 1);
    pool_free(pool);
    
cleanup_no_repo:
    // Cleanup: free all dynamically allocated configuration strings
    if (cfg.staging_dir) free(cfg.staging_dir);
    if (cfg.chroot_path) free(cfg.chroot_path);
    if (cfg.build_tmp_dir) free(cfg.build_tmp_dir);
    if (cfg.repo_path) free(cfg.repo_path);
    if (cfg.local_package) free(cfg.local_package);
    if (cfg.clean_cache_pkg) free(cfg.clean_cache_pkg);
    
    return exit_code;
}


