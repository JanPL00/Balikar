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
#include <getopt.h>

// Globální konfigurace z CLI
typedef struct {
    int debug_level;
    int dry_run;
    int no_staging;
    char *staging_dir;
    char *chroot_path;
    char *build_tmp_dir;
    char *repo_path;
    char *local_package;
    const char *command;
    const char *package_name;
} cli_config_t;

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
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hd", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(0);
                break;
            case 'd':
                cfg->debug_level++;
                break;
            case 'D':
                cfg->dry_run = 1;
                break;
            case 'S':
                cfg->no_staging = 1;
                break;
            case 's':
                cfg->staging_dir = strdup(optarg);
                break;
            case 'c':
                cfg->chroot_path = strdup(optarg);
                break;
            case 't':
                cfg->build_tmp_dir = strdup(optarg);
                break;
            case 'r':
                cfg->repo_path = strdup(optarg);
                break;
            case 'l':
                cfg->local_package = strdup(optarg);
                break;
            default:
                print_usage(1);
        }
    }
    
    // Parsuj command a package_name
    if (optind < argc) {
        cfg->command = argv[optind++];
    }
    if (optind < argc) {
        cfg->package_name = argv[optind];
    }
    
    // Výchozí repo path pokud není zadán
    if (!cfg->repo_path) {
        cfg->repo_path = strdup("./repo");
    }
}

int main(int argc, char **argv) {
    // Inicializuj konfiguraci
    cli_config_t cfg = {0};
    parse_args(argc, argv, &cfg);
    
    if (!cfg.command) {
        log_error("No command specified. Use -h for help.");
        print_usage(1);
    }

    // Aplikuj konfiguraci
    if (cfg.staging_dir) set_staging_dir(cfg.staging_dir);
    if (cfg.no_staging) set_staging_enabled(0);
    if (cfg.build_tmp_dir) set_build_tmp_dir(cfg.build_tmp_dir);
    
    log_info("Balikar Package Manager - Command: %s", cfg.command);
    if (cfg.dry_run) log_info("DRY-RUN mode enabled");
    
    // Inicializuj Pool
    Pool *pool = pool_create();
    if (!pool) {
        log_error("Failed to create pool.");
        return 1;
    }
    
    // Načti repozitář
    Repo *repo = load_repository(pool, cfg.repo_path);
    if (!repo) {
        log_error("Failed to load repository from %s.", cfg.repo_path);
        pool_free(pool);
        return 1;
    }
    
    // Zpracuj příkazy
    int exit_code = 0;
    
    if (strcmp(cfg.command, "install") == 0 || strcmp(cfg.command, "build") == 0) {
        if (!cfg.package_name) {
            log_error("Package name required for %s command.", cfg.command);
            print_usage(1);
        }
        
        log_info("Starting %s workflow for package: %s", cfg.command, cfg.package_name);
        
        // 1. Vytvoř solver
        Solver *solv = create_solver(pool);
        if (!solv) {
            log_error("Failed to create solver.");
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // 2. Solve dependencies
        Queue *solution = solve_dependencies(solv, cfg.package_name, pool, repo);
        if (!solution) {
            log_error("Dependency resolution failed for %s.", cfg.package_name);
            solver_free(solv);
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        // 3. Vytvoř a zobraz transakci
        Transaction *trans = solver_create_transaction(solv);
        if (!trans) {
            log_error("Failed to create transaction.");
            queue_free(solution);
            solver_free(solv);
            exit_code = ERR_SOLVE_FAIL;
            goto cleanup;
        }
        
        if (trans->steps.count == 0) {
            log_info("No changes needed - package may already be resolved.");
        } else {
            display_transaction(trans);
        }
        
        // 4. Confirm (pokud není dry-run)
        if (!cfg.dry_run) {
            if (!user_confirm("Proceed with transaction?", 1)) {
                log_info("Transaction cancelled by user.");
                transaction_free(trans);
                queue_free(solution);
                solver_free(solv);
                goto cleanup;
            }
            
            // 5. Download + Build
            log_info("Downloading packages...");
            transaction_order(trans, 0);  // Seřaď podle závislostí
            
            for (int i = 0; i < trans->steps.count; i++) {
                Id p = trans->steps.elements[i];
                Id type = transaction_type(trans, p, SOLVER_TRANSACTION_RPM_ONLY);
                
                if (type == SOLVER_TRANSACTION_INSTALL || type == SOLVER_TRANSACTION_MULTIINSTALL) {
                    Solvable *s = pool_id2solvable(pool, p);
                    const char *pkg_name = pool_id2str(pool, s->name);
                    
                    // Získej download URL
                    Repodata *data = repo_id2repodata(repo, 1);  // První repodata
                    const char *source_url = get_custom_source(data, p, pool);
                    
                    if (!source_url) {
                        log_warn("No source URL for %s, skipping.", pkg_name);
                        continue;
                    }
                    
                    log_info("Downloading %s from %s", pkg_name, source_url);
                    FILE *fp = download_package(s, source_url, pool, data);
                    if (!fp) {
                        log_error("Download failed for %s.", pkg_name);
                        exit_code = ERR_DOWNLOAD_FAIL;
                        break;
                    }
                    
                    // Download extras (patches)
                    json_t *extras = get_custom_extras(data, p, pool);
                    if (extras) {
                        download_extras(extras, pkg_name, NULL);
                        json_decref(extras);
                    }
                    
                    // Build
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
        
    } else if (strcmp(cfg.command, "resolve") == 0) {
        if (!cfg.package_name) {
            log_error("Package name required for resolve command.");
            print_usage(1);
        }
        
        log_info("Resolving dependencies for: %s", cfg.package_name);
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
        
        Transaction *trans = solver_create_transaction(solv);
        display_transaction(trans);
        
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        
    } else if (strcmp(cfg.command, "repos") == 0) {
        log_info("Enabled repositories:");
        log_info("  - %s (%d packages)", cfg.repo_path, repo->nsolvables);
        
    } else if (strcmp(cfg.command, "repo-update") == 0) {
        log_info("Updating repository metadata: %s", cfg.repo_path);
        
        // Uvolni současné repo a pool
        repo_free(repo, 1);
        pool_free(pool);
        
        // Znovu vytvoř pool pro čerstvý start
        pool = pool_create();
        if (!pool) {
            log_error("Failed to recreate pool.");
            return 1;
        }
        
        // Vygeneruj nové repodata.json
        int rc = generate_repodata(cfg.repo_path);
        if (rc != 0) {
            log_error("Failed to update repository metadata.");
            exit_code = ERR_SOLVE_FAIL;
            pool_free(pool);
            goto cleanup_no_repo;
        }
        
        log_info("Repository metadata updated successfully.");
        log_info("Reloading repository...");
        
        // Načti znovu repository s novými metadaty
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
    // Cleanup
    repo_free(repo, 1);
    pool_free(pool);
    
cleanup_no_repo:
    if (cfg.staging_dir) free(cfg.staging_dir);
    if (cfg.chroot_path) free(cfg.chroot_path);
    if (cfg.build_tmp_dir) free(cfg.build_tmp_dir);
    if (cfg.repo_path) free(cfg.repo_path);
    if (cfg.local_package) free(cfg.local_package);
    
    return exit_code;
}


