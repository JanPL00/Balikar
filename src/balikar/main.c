//
// Created by placeja5 on 11/2/25.
//
#include "includes/parser.h"
#include "includes/solver.h"
#include "includes/downloader.h"
#include "includes/builder.h"
#include "includes/installer.h"
#include "../common/common.h"
#include "../common/config.h"
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

/**
 * CLI configuration structure.
 * Holds all command-line arguments and options parsed from user input.
 * Controls program behavior including debug level, staging, paths, and commands.
 */
typedef struct {
    int debug_level;         // Verbosity level for debugging output
    int dry_run;             // If 1, simulate actions without executing them
    int no_deps;             // If 1, download without dependencies
    char *staging_dir;       // Custom staging directory path
    char *root_path;         // Target root for installation (default /)
    char *build_tmp_dir;     // Custom temporary directory for builds
    char *repo_path;         // Repository URL or local path
    char *local_package;     // Local package file path
    char *clean_cache_pkg;   // Package name for cache cleaning
    char *output_dir;        // Output directory for download command
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
        {"no-deps", no_argument, 0, 'N'},
        {"staging-dir", required_argument, 0, 's'},
        {"root", required_argument, 0, 'R'},
        {"build-tmp-dir", required_argument, 0, 't'},
        {"repo", required_argument, 0, 'r'},
        {"local-package", required_argument, 0, 'l'},
        {"pkg", required_argument, 0, 'p'},
        {"output-dir", required_argument, 0, 'o'},
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
            case 'N':  // No-deps - download without dependencies
                cfg->no_deps = 1;
                break;
            case 's':  // Staging-dir - custom staging directory
                cfg->staging_dir = strdup(optarg);
                break;
            case 'R':  // Root - target root for installation
                cfg->root_path = strdup(optarg);
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
            case 'o':  // Output-dir - output directory for download command
                cfg->output_dir = strdup(optarg);
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
    
    // Set global debug level BEFORE any log_debug() calls
    DEBUG_LEVEL = cfg->debug_level;
    
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
    
    // Parse and validate command-line arguments (sets DEBUG_LEVEL internally)
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
        // Handle local package installation (bypass repository and dependency resolution)
        if (cfg.local_package) {
            log_info("Local package mode: %s", cfg.local_package);
            
            // Validate file exists
            if (access(cfg.local_package, F_OK) != 0) {
                log_error("Local package file not found: %s", cfg.local_package);
                exit_code = 1;
                goto cleanup;
            }
            
            // Validate file is readable
            if (access(cfg.local_package, R_OK) != 0) {
                log_error("Local package file not readable: %s", cfg.local_package);
                exit_code = 1;
                goto cleanup;
            }
            
            // Extract package name and version from filename
            // Expected format: <name>-<version>.tar.bz2 or <name>-<version>-<release>.tar.bz2
            char *basename = strrchr(cfg.local_package, '/');
            basename = basename ? basename + 1 : cfg.local_package;
            
            char pkg_name[256] = {0};
            char pkg_version[256] = {0};
            
            // Copy basename and remove tar extension
            char *name_copy = strdup(basename);
            char *ext = strstr(name_copy, ".tar.bz2");
            if (!ext) {
                ext = strstr(name_copy, ".tar.gz");
            }
            if (!ext) {
                ext = strstr(name_copy, ".tar.xz");
            }
            if (!ext) {
                log_error("Invalid package file: must be .tar.bz2, .tar.gz or .tar.xz format");
                free(name_copy);
                exit_code = 1;
                goto cleanup;
            }
            *ext = '\0';
            
            // Parse name-version format (find last dash)
            char *last_dash = strrchr(name_copy, '-');
            if (last_dash) {
                *last_dash = '\0';
                strncpy(pkg_name, name_copy, sizeof(pkg_name) - 1);
                strncpy(pkg_version, last_dash + 1, sizeof(pkg_version) - 1);
                
                // Check if there's a release number (another dash before version)
                char *version_dash = strrchr(pkg_name, '-');
                if (version_dash && isdigit(*(version_dash + 1))) {
                    // This is actually name-version-release format
                    // Combine version-release as version
                    snprintf(pkg_version, sizeof(pkg_version), "%s-%s", version_dash + 1, last_dash + 1);
                    *version_dash = '\0';
                }
            } else {
                // No version in filename, use just the name
                strncpy(pkg_name, name_copy, sizeof(pkg_name) - 1);
                strcpy(pkg_version, "local");
            }
            free(name_copy);
            
            log_info("Detected package: %s, version: %s", pkg_name, pkg_version);
            log_info("Note: --local-package does not resolve dependencies automatically");
            log_info("Make sure all required dependencies are already installed");
            
            if (!cfg.dry_run) {
                // Open local package file
                FILE *local_fp = fopen(cfg.local_package, "rb");
                if (!local_fp) {
                    log_error("Failed to open local package: %s", cfg.local_package);
                    exit_code = 1;
                    goto cleanup;
                }
                
                // Build package from local file
                log_info("Building %s from local package...", pkg_name);
                
                // Create temporary build directory
                char build_dir[512];
                snprintf(build_dir, sizeof(build_dir), "/tmp/balikar_build_%s_local_XXXXXX", pkg_name);
                if (!mkdtemp(build_dir)) {
                    log_error("Failed to create build directory");
                    fclose(local_fp);
                    exit_code = ERR_BUILD_FAIL;
                    goto cleanup;
                }
                
                log_info("Build directory: %s", build_dir);
                
                // Extract archive
                struct archive *a = archive_read_new();
                archive_read_support_filter_all(a);
                archive_read_support_format_all(a);
                
                if (archive_read_open_FILE(a, local_fp) != ARCHIVE_OK) {
                    log_error("Failed to open archive: %s", archive_error_string(a));
                    archive_read_free(a);
                    fclose(local_fp);
                    exit_code = ERR_BUILD_FAIL;
                    goto cleanup;
                }
                
                struct archive_entry *entry;
                while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
                    // Prepend build_dir to entry pathname
                    const char *current_file = archive_entry_pathname(entry);
                    char full_path[1024];
                    snprintf(full_path, sizeof(full_path), "%s/%s", build_dir, current_file);
                    archive_entry_set_pathname(entry, full_path);
                    
                    // Extract entry
                    if (archive_read_extract(a, entry, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM) != ARCHIVE_OK) {
                        log_warn("Extract warning: %s", archive_error_string(a));
                    }
                }
                
                archive_read_close(a);
                archive_read_free(a);
                fclose(local_fp);
                
                log_info("Archive extracted successfully");
                
                // Find source directory (first subdirectory)
                DIR *dir = opendir(build_dir);
                if (!dir) {
                    log_error("Failed to open build directory");
                    exit_code = ERR_BUILD_FAIL;
                    goto cleanup;
                }
                
                char source_dir[1024] = {0};
                struct dirent *ent;
                while ((ent = readdir(dir)) != NULL) {
                    if (ent->d_name[0] == '.') continue;
                    
                    char test_path[1024];
                    snprintf(test_path, sizeof(test_path), "%s/%s", build_dir, ent->d_name);
                    
                    struct stat st;
                    if (stat(test_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                        strncpy(source_dir, test_path, sizeof(source_dir) - 1);
                        break;
                    }
                }
                closedir(dir);
                
                if (!source_dir[0]) {
                    log_error("Source directory not found in archive");
                    exit_code = ERR_BUILD_FAIL;
                    goto cleanup;
                }
                
                log_info("Source directory: %s", source_dir);
                
                // Check if this is a balikar package with nested source archive
                char index_json_path[2048];
                snprintf(index_json_path, sizeof(index_json_path), "%s/index.json", source_dir);
                
                if (access(index_json_path, F_OK) == 0) {
                    log_info("Detected balikar package with index.json");
                    
                    // Load index.json
                    json_error_t error;
                    json_t *index_root = json_load_file(index_json_path, 0, &error);
                    if (index_root) {
                        json_t *source_array = json_object_get(index_root, "source");
                        if (source_array && json_is_array(source_array) && json_array_size(source_array) > 0) {
                            json_t *first_source = json_array_get(source_array, 0);
                            json_t *url_obj = json_object_get(first_source, "url");
                            
                            if (url_obj && json_is_string(url_obj)) {
                                const char *source_file = json_string_value(url_obj);
                                
                                // Check if it's a relative path (no scheme)
                                if (strstr(source_file, "://") == NULL) {
                                    char nested_tar_path[2048];
                                    snprintf(nested_tar_path, sizeof(nested_tar_path), "%s/%s", source_dir, source_file);
                                    
                                    if (access(nested_tar_path, F_OK) == 0) {
                                        log_info("Found nested source archive: %s", source_file);
                                        log_info("Extracting nested source to build directory...");
                                        
                                        // Extract nested tar directly to source_dir
                                        char extract_cmd[4096];
                                        snprintf(extract_cmd, sizeof(extract_cmd), "tar -xf '%s' -C '%s' 2>/dev/null", nested_tar_path, source_dir);
                                        
                                        int extract_status = system(extract_cmd);
                                        if (extract_status == 0) {
                                            // Find the extracted source directory
                                            DIR *extract_dir = opendir(source_dir);
                                            if (extract_dir) {
                                                struct dirent *extract_ent;
                                                char actual_source_dir[2048] = {0};
                                                
                                                while ((extract_ent = readdir(extract_dir)) != NULL) {
                                                    if (extract_ent->d_name[0] == '.') continue;
                                                    if (strcmp(extract_ent->d_name, "index.json") == 0) continue;
                                                    if (strcmp(extract_ent->d_name, "build.sh") == 0) continue;
                                                    if (strcmp(extract_ent->d_name, ".install") == 0) continue;
                                                    if (strstr(extract_ent->d_name, ".tar.") != NULL) continue;
                                                    
                                                    char test_extract_path[2048];
                                                    snprintf(test_extract_path, sizeof(test_extract_path), "%s/%s", source_dir, extract_ent->d_name);
                                                    
                                                    struct stat extract_st;
                                                    if (stat(test_extract_path, &extract_st) == 0 && S_ISDIR(extract_st.st_mode)) {
                                                        strncpy(actual_source_dir, test_extract_path, sizeof(actual_source_dir) - 1);
                                                        break;
                                                    }
                                                }
                                                closedir(extract_dir);
                                                
                                                if (actual_source_dir[0]) {
                                                    log_info("Using nested source directory: %s", actual_source_dir);
                                                    strncpy(source_dir, actual_source_dir, sizeof(source_dir) - 1);
                                                }
                                            }
                                        } else {
                                            log_warn("Failed to extract nested source archive (status=%d)", extract_status);
                                        }
                                    }
                                }
                            }
                        }
                        json_decref(index_root);
                    }
                }
                
                log_info("Final source directory: %s", source_dir);
                
                // Download extra files (including build.sh script) from repository
                log_info("Attempting to download build.sh script and extras from repository...");
                download_extras(NULL, pkg_name, pkg_version, cfg.repo_path, source_dir);
                
                // Set up build environment
                const char *staging = cfg.staging_dir ? cfg.staging_dir : STAGING_DIR;
                setenv("DESTDIR", staging, 1);
                setenv("P", staging, 1);  // P is alias for DESTDIR used in build.sh scripts
                
                // Set ROOT to point to installed dependencies (chroot with previous packages)
                if (cfg.root_path) {
                    setenv("ROOT", cfg.root_path, 1);
                }
                
                long nproc = sysconf(_SC_NPROCESSORS_ONLN);
                if (nproc < 1) nproc = 1;
                char makeflags[64];
                snprintf(makeflags, sizeof(makeflags), "-j%ld", nproc);
                setenv("MAKEFLAGS", makeflags, 1);
                
                // Copy build.sh script from cache to source directory if it exists
                char build_cache[1024];
                char *cache_dir = get_cache_dir();
                snprintf(build_cache, sizeof(build_cache), "%s/extras/%s/build.sh", cache_dir, pkg_name);
                free(cache_dir);
                
                if (access(build_cache, F_OK) == 0) {
                    char build_dest[1024];
                    snprintf(build_dest, sizeof(build_dest), "%s/build.sh", source_dir);
                    
                    char cp_cmd[2048];
                    snprintf(cp_cmd, sizeof(cp_cmd), "cp '%s' '%s' && chmod +x '%s'", 
                             build_cache, build_dest, build_dest);
                    if (system(cp_cmd) == 0) {
                        log_info("Copied build.sh script to source directory");
                    }
                }
                
                // Change to source directory
                if (chdir(source_dir) != 0) {
                    log_error("Failed to chdir to source directory");
                    exit_code = ERR_BUILD_FAIL;
                    goto cleanup;
                }
                
                // Build the package
                int have_build = access("build.sh", X_OK) == 0;
                const char *build_cmd = have_build ? 
                    "bash -e build.sh > build.log 2>&1" : 
                    "./configure --prefix=/usr >/dev/null 2>&1 && make >/dev/null 2>&1 && make install >/dev/null 2>&1";
                
                log_info("Running build: %s", have_build ? "build.sh script" : "configure+make");
                
                int build_rc = system(build_cmd);
                if (build_rc != 0) {
                    log_error("Build failed with exit code %d", build_rc);
                    exit_code = ERR_BUILD_FAIL;
                    
                    // Cleanup
                    chdir("/tmp");
                    char cleanup[1024];
                    snprintf(cleanup, sizeof(cleanup), "rm -rf '%s'", build_dir);
                    system(cleanup);
                    goto cleanup;
                }
                
                log_info("Successfully built %s", pkg_name);
                
                // Cleanup build directory
                chdir("/tmp");
                char cleanup[1024];
                snprintf(cleanup, sizeof(cleanup), "rm -rf '%s'", build_dir);
                system(cleanup);
                
                // Install if command is 'install'
                if (strcmp(cfg.command, "install") == 0) {
                    const char *target_root = cfg.root_path ? cfg.root_path : "/";
                    const char *staging = cfg.staging_dir ? cfg.staging_dir : STAGING_DIR;
                    
                    const char *root_display = strcmp(target_root, "/") == 0 ? "system root (/)" : target_root;
                    log_info("Installing %s to %s...", pkg_name, root_display);
                    
                    // Local package is always installed manually
                    if (install_package_with_reason(pkg_name, pkg_version, staging, target_root, "manual") != 0) {
                        log_error("Installation failed for %s.", pkg_name);
                        exit_code = ERR_BUILD_FAIL;
                        goto cleanup;
                    }
                    
                    log_info("Successfully installed %s to %s", pkg_name, root_display);
                    
                    // Cleanup staging
                    char cleanup_cmd[512];
                    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf '%s' 2>/dev/null && mkdir -p '%s'", staging, staging);
                    system(cleanup_cmd);
                    log_debug(1, "Cleaned up staging directory: %s", staging);
                }
            } else {
                log_info("DRY-RUN: Would build and install %s from %s", pkg_name, cfg.local_package);
            }
            
            goto cleanup;
        }
        
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
        // Pass root_path so solver can check for already installed packages
        const char *target_root = cfg.root_path ? cfg.root_path : "/";
        Queue *solution = solve_dependencies(solv, cfg.package_name, pool, repo, target_root, get_custom_mkdeps);
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
                    
                    // Skip if package is already installed in target root
                    if (is_package_installed(pkg_name, target_root)) {
                        log_info("Skipping %s - already installed in %s", pkg_name, target_root);
                        continue;
                    }
                    
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
                    
                    // Build package into staging directory
                    // Always build to staging first, then install if needed
                    log_info("Building %s...", pkg_name);
                    int is_group = get_custom_is_group(data, p, pool);
                    int no_package = get_custom_no_package(data, p, pool);
                    int build_rc = commit_element(trans, p, fp, pool, is_group, no_package, extras);
                    if (build_rc != 0) {
                        log_error("Build failed for %s.", pkg_name);
                        exit_code = ERR_BUILD_FAIL;
                        break;
                    }
                    
                    log_info("Successfully built %s.", pkg_name);
                    
                    // Install package if command is 'install' (not just 'build')
                    // Two-stage approach: build → install
                    if (strcmp(cfg.command, "install") == 0) {
                        // Determine target root for installation
                        const char *target_root = cfg.root_path ? cfg.root_path : "/"; // Default: native system
                        
                        // Get staging directory where builder created files
                        const char *staging = cfg.staging_dir ? cfg.staging_dir : STAGING_DIR;
                        
                        const char *root_display = strcmp(target_root, "/") == 0 ? "system root (/)" : target_root;
                        log_info("Installing %s to %s...", pkg_name, root_display);
                        
                        // Determine install reason: manual if it's the requested package, dependency otherwise
                        const char *install_reason = strcmp(pkg_name, cfg.package_name) == 0 ? "manual" : "dependency";
                        
                        // Call installer to merge staging → target root
                        if (install_package_with_reason(pkg_name, version, staging, target_root, install_reason) != 0) {
                            log_error("Installation failed for %s.", pkg_name);
                            exit_code = ERR_BUILD_FAIL;
                            break;
                        }
                        
                        log_info("Successfully installed %s to %s", pkg_name, root_display);
                        
                        // Cleanup staging directory after successful installation
                        char cleanup_cmd[512];
                        snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf '%s' 2>/dev/null && mkdir -p '%s'", staging, staging);
                        system(cleanup_cmd);
                        log_debug(1, "Cleaned up staging directory: %s", staging);
                    }
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
        
        const char *target_root = cfg.root_path ? cfg.root_path : "/";
        Queue *solution = solve_dependencies(solv, cfg.package_name, pool, repo, target_root, get_custom_mkdeps);
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
    // Command: remove - remove installed package
    // Uninstalls package from the system by removing all installed files
    else if (strcmp(cfg.command, "remove") == 0) {
        if (!cfg.package_name) {
            log_error("Package name required for remove command.");
            print_usage(1);
        }
        
        log_info("Removing package: %s", cfg.package_name);
        
        // Determine target root where package is installed
        const char *target_root = cfg.root_path ? cfg.root_path : "/";
        
        // Use erase_package_with_checks for dependency checking and orphan detection
        if (!cfg.dry_run) {
            if (erase_package_with_checks(cfg.package_name, target_root, pool, repo, 1, load_installed_db, erase_package) != 0) {
                log_error("Failed to remove package '%s'.", cfg.package_name);
                exit_code = 1;
                goto cleanup;
            }
        } else {
            log_info("DRY-RUN: Would remove package '%s' from %s", cfg.package_name, target_root);
        }
        
    }
    // Command: autoremove - remove orphaned dependencies
    // Removes packages installed as dependencies that are no longer needed
    else if (strcmp(cfg.command, "autoremove") == 0) {
        const char *target_root = cfg.root_path ? cfg.root_path : "/";
        
        log_info("Searching for orphaned packages in %s...", target_root);
        
        if (autoremove_packages(target_root, pool, repo, cfg.dry_run, load_installed_db, erase_package) < 0) {
            log_error("Autoremove failed.");
            exit_code = 1;
            goto cleanup;
        }
        
    } 
    // Command: download - resolve dependencies and download packages without building
    // Downloads to local repository structure or custom output directory
    else if (strcmp(cfg.command, "download") == 0) {
        if (!cfg.package_name) {
            log_error("Package name required for download command.");
            print_usage(1);
        }
        
        log_info("Starting download workflow for package: %s", cfg.package_name);
        if (cfg.no_deps) {
            log_info("Downloading without dependencies (--no-deps)");
        }
        
        // Determine output directory: --output-dir or first enabled local repo
        char *output_directory = NULL;
        if (cfg.output_dir) {
            output_directory = strdup(cfg.output_dir);
            log_info("Using custom output directory: %s", output_directory);
        } else {
            // Find first enabled local repository from config
            init_repo_config();
            repo_config_t *repos = get_enabled_repos();
            for (repo_config_t *r = repos; r; r = r->next) {
                if (r->enabled) {
                    // Check if URL is local path (not starting with http/https)
                    if (strncmp(r->url, "http://", 7) != 0 && 
                        strncmp(r->url, "https://", 8) != 0 &&
                        strncmp(r->url, "ftp://", 6) != 0) {
                        output_directory = strdup(r->url);
                        log_info("Using local repository from config: %s", output_directory);
                        break;
                    }
                }
            }
            
            // Fallback to ./repo if no local repository found
            if (!output_directory) {
                output_directory = strdup("./repo");
                log_info("No local repository found in config, using default: ./repo");
            }
        }
        
        // Handle --no-deps: download only the specified package
        if (cfg.no_deps) {
            // Find package in pool by name
            Id name_id = pool_str2id(pool, cfg.package_name, 0);
            if (!name_id) {
                log_error("Package '%s' not found in repository.", cfg.package_name);
                free(output_directory);
                exit_code = 1;
                goto cleanup;
            }
            
            // Find the solvable with this name
            Solvable *s = NULL;
            Id p = 0;
            for (Id i = 2; i < pool->nsolvables; i++) {
                Solvable *candidate = pool_id2solvable(pool, i);
                if (candidate->name == name_id) {
                    s = candidate;
                    p = i;
                    break;
                }
            }
            
            if (!s) {
                log_error("Package '%s' not found in pool.", cfg.package_name);
                free(output_directory);
                exit_code = 1;
                goto cleanup;
            }
            
            const char *pkg_name = pool_id2str(pool, s->name);
            const char *version = pool_id2str(pool, s->evr);
            log_info("Found package: %s-%s", pkg_name, version);
            
            // Get source URL from sources array for downloading source tarball
            // Try sources in order until we find an absolute URL (skip relative ones)
            Repodata *data = repo_id2repodata(repo, 1);
            const char *download_url = NULL;
            static char full_url[4096];
            
            json_t *source_array = get_custom_source_array(data, p, pool);
            
            if (source_array && json_is_array(source_array)) {
                size_t source_count = json_array_size(source_array);
                for (size_t i = 0; i < source_count; i++) {
                    json_t *source_obj = json_array_get(source_array, i);
                    if (!source_obj || !json_is_object(source_obj)) continue;
                    
                    json_t *url_json = json_object_get(source_obj, "url");
                    if (!url_json || !json_is_string(url_json)) continue;
                    
                    const char *source_url = json_string_value(url_json);
                    
                    // Check if URL is absolute (has scheme) - prefer absolute URLs
                    int is_absolute = (strncmp(source_url, "http://", 7) == 0 ||
                                     strncmp(source_url, "https://", 8) == 0 ||
                                     strncmp(source_url, "ftp://", 6) == 0 ||
                                     strncmp(source_url, "file://", 7) == 0);
                    
                    if (is_absolute) {
                        download_url = source_url;
                        break;  // Use first absolute URL
                    }
                }
            }
            
            if (!download_url) {
                // Fallback to location field if sources not available
                const char *location = solvable_get_location(s, NULL);
                if (location) {
                    snprintf(full_url, sizeof(full_url), "%s/%s", cfg.repo_path, location);
                    download_url = full_url;
                }
            }
            
            if (!download_url) {
                log_error("No download URL for %s.", pkg_name);
                if (source_array) json_decref(source_array);
                free(output_directory);
                exit_code = 1;
                goto cleanup;
            }
            
            // Detect file extension from URL
            const char *ext = ".tar.bz2";  // default
            const char *last_dot = strrchr(download_url, '.');
            if (last_dot) {
                const char *last_slash = strrchr(download_url, '/');
                const char *filename = last_slash ? last_slash + 1 : download_url;
                
                if (strstr(filename, ".tar.xz")) ext = ".tar.xz";
                else if (strstr(filename, ".tar.gz")) ext = ".tar.gz";
                else if (strstr(filename, ".tar.bz2")) ext = ".tar.bz2";
                else if (strstr(filename, ".tgz")) ext = ".tgz";
                else ext = last_dot;
            }
            
            // Create output path
            char output_path[2048];
            snprintf(output_path, sizeof(output_path), "%s/%s-%s%s", output_directory, pkg_name, version, ext);
            
            log_info("Downloading %s from: %s", pkg_name, download_url);
            int rc = download_url_to_file(download_url, output_path);
            if (rc != 0) {
                log_error("Download failed for %s.", pkg_name);
                if (source_array) json_decref(source_array);
                free(output_directory);
                exit_code = ERR_DOWNLOAD_FAIL;
                goto cleanup;
            }
            
            log_info("Successfully downloaded %s to %s", pkg_name, output_path);
            if (source_array) json_decref(source_array);
            free(output_directory);
            goto cleanup;
        }
        
        // Step 1: Create solver for dependency resolution (with dependencies)
        Solver *solv = create_solver(pool);
        if (!solv) {
            log_error("Failed to create solver.");
            free(output_directory);
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // Step 2: Solve dependencies
        // For download command, use system root (downloads are independent of target root)
        Queue *solution = solve_dependencies(solv, cfg.package_name, pool, repo, "/", get_custom_mkdeps);
        if (!solution) {
            log_error("Dependency resolution failed for %s.", cfg.package_name);
            solver_free(solv);
            free(output_directory);
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // Step 3: Create transaction from solution
        Transaction *trans = solver_create_transaction(solv);
        if (!trans) {
            log_error("Failed to create transaction.");
            queue_free(solution);
            solver_free(solv);
            free(output_directory);
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // Display transaction to user
        if (trans->steps.count == 0) {
            log_info("No packages to download - package may already be resolved.");
        } else {
            display_transaction(trans);
        }
        
        // Step 4: Confirm transaction
        if (!user_confirm("Proceed with download?", 1)) {
            log_info("Download cancelled by user.");
            transaction_free(trans);
            queue_free(solution);
            solver_free(solv);
            free(output_directory);
            goto cleanup;
        }
        
        // Step 5: Download packages
        log_info("Downloading packages to: %s", output_directory);
        
        // Order transaction by dependencies
        transaction_order(trans, 0);
        
        // Process each package in transaction
        int download_count = 0;
        for (int i = 0; i < trans->steps.count; i++) {
            Id p = trans->steps.elements[i];
            Id type = transaction_type(trans, p, SOLVER_TRANSACTION_RPM_ONLY);
            
            // Only process install transactions
            if (type == SOLVER_TRANSACTION_INSTALL || type == SOLVER_TRANSACTION_MULTIINSTALL) {
                Solvable *s = pool_id2solvable(pool, p);
                const char *pkg_name = pool_id2str(pool, s->name);
                
                // Get source URL from sources array for downloading source tarball
                // Try sources in order until we find an absolute URL (skip relative ones)
                Repodata *data = repo_id2repodata(repo, 1);
                const char *download_url = NULL;
                json_t *source_array = NULL;
                static char full_url_buf[4096];
                
                source_array = get_custom_source_array(data, p, pool);
                
                if (source_array && json_is_array(source_array)) {
                    size_t source_count = json_array_size(source_array);
                    for (size_t i = 0; i < source_count; i++) {
                        json_t *source_obj = json_array_get(source_array, i);
                        if (!source_obj || !json_is_object(source_obj)) continue;
                        
                        json_t *url_json = json_object_get(source_obj, "url");
                        if (!url_json || !json_is_string(url_json)) continue;
                        
                        const char *source_url = json_string_value(url_json);
                        
                        // Check if URL is absolute (has scheme) - prefer absolute URLs
                        int is_absolute = (strncmp(source_url, "http://", 7) == 0 ||
                                         strncmp(source_url, "https://", 8) == 0 ||
                                         strncmp(source_url, "ftp://", 6) == 0 ||
                                         strncmp(source_url, "file://", 7) == 0);
                        
                        if (is_absolute) {
                            download_url = source_url;
                            break;  // Use first absolute URL
                        }
                    }
                }
                
                if (!download_url) {
                    // Fallback to location field if sources not available
                    const char *location = solvable_get_location(s, NULL);
                    if (location) {
                        snprintf(full_url_buf, sizeof(full_url_buf), "%s/%s", cfg.repo_path, location);
                        download_url = full_url_buf;
                    }
                }
                
                if (!download_url) {
                    log_warn("No download URL for %s, skipping.", pkg_name);
                    if (source_array) json_decref(source_array);
                    continue;
                }
                
                // Detect file extension from URL
                const char *version = pool_id2str(pool, s->evr);
                const char *ext = ".tar.bz2";  // default
                const char *last_dot = strrchr(download_url, '.');
                if (last_dot) {
                    const char *last_slash = strrchr(download_url, '/');
                    const char *filename = last_slash ? last_slash + 1 : download_url;
                    
                    if (strstr(filename, ".tar.xz")) ext = ".tar.xz";
                    else if (strstr(filename, ".tar.gz")) ext = ".tar.gz";
                    else if (strstr(filename, ".tar.bz2")) ext = ".tar.bz2";
                    else if (strstr(filename, ".tgz")) ext = ".tgz";
                    else ext = last_dot;
                }
                
                // Create output path
                char output_path[2048];
                snprintf(output_path, sizeof(output_path), "%s/%s-%s%s", output_directory, pkg_name, version, ext);
                
                // Download package to output directory
                log_info("Downloading %s from %s", pkg_name, download_url);
                int rc = download_url_to_file(download_url, output_path);
                if (source_array) json_decref(source_array);
                
                if (rc != 0) {
                    log_error("Download failed for %s.", pkg_name);
                    exit_code = ERR_DOWNLOAD_FAIL;
                    break;
                }
                
                download_count++;
                log_info("Successfully downloaded %s.", pkg_name);
            }
        }
        
        if (exit_code == 0) {
            log_info("Download complete: %d packages downloaded to %s", download_count, output_directory);
        }
        
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        free(output_directory);
        
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
    if (cfg.root_path) free(cfg.root_path);
    if (cfg.build_tmp_dir) free(cfg.build_tmp_dir);
    if (cfg.repo_path) free(cfg.repo_path);
    if (cfg.local_package) free(cfg.local_package);
    if (cfg.clean_cache_pkg) free(cfg.clean_cache_pkg);
    if (cfg.output_dir) free(cfg.output_dir);
    
    return exit_code;
}


