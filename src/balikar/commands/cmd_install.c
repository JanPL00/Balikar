//
// Command: install/build - Complete workflow from solving to installation
// This is the most complex command - handles both install and build
//

#include "../includes/command_context.h"
#include "../includes/solver.h"
#include "../includes/downloader.h"
#include "../includes/builder.h"
#include "../includes/installer.h"
#include "../includes/parser.h"
#include "../../utils/includes/print.h"
#include "../../utils/includes/cli_helpers.h"
#include <stdlib.h>
#include <string.h>

/**
 * Shared implementation for install and build commands.
 * 
 * @param ctx Command context
 * @param do_install If 1, perform installation after build. If 0, only build.
 * @return Exit code
 */
int cmd_install_or_build_execute(CommandContext *ctx, int do_install) {
    if (!ctx) return 1;
    
    // Validation already done by registry
    if (!ctx->config.package_name && !ctx->config.local_package_allocated) {
        log_error("Package name or --local-package required");
        return 1;
    }
    
    // Ensure pool and repo are loaded
    Pool *pool = command_context_ensure_pool(ctx);
    Repo *repo = command_context_ensure_repo(ctx);
    if (!pool || !repo) {
        log_error("Failed to initialize pool/repo");
        return 1;
    }
    
    const char *pkg_name = ctx->config.package_name;
    const char *target_root = command_context_get_root_path(ctx);
    const char *staging = command_context_get_staging_dir(ctx);
    
    log_info("Starting %s workflow for package: %s", 
             do_install ? "install" : "build", 
             pkg_name ? pkg_name : ctx->config.local_package_allocated);
    
    // Create solver
    Solver *solv = ctx->subsystems.create_solver(pool);
    if (!solv) {
        log_error("Failed to create solver");
        return ERR_SOLVE_FAIL;
    }
    
    // Solve dependencies
    Queue *solution = ctx->subsystems.solve_dependencies(
        solv, pkg_name, pool, repo, target_root, get_custom_mkdeps
    );
    
    if (!solution) {
        log_error("Dependency resolution failed for %s", pkg_name);
        solver_free(solv);
        return ERR_SOLVE_FAIL;
    }
    
    // Create transaction
    Transaction *trans = solver_create_transaction(solv);
    if (!trans) {
        log_error("Failed to create transaction");
        queue_free(solution);
        solver_free(solv);
        return ERR_SOLVE_FAIL;
    }
    
    // Display transaction
    if (trans->steps.count == 0) {
        log_info("No changes needed - package may already be resolved");
    } else {
        transaction_order(trans, 0);
        display_transaction(trans);
    }
    
    // In dry-run mode, stop here
    if (ctx->config.dry_run) {
        log_info("DRY-RUN: Skipping download and build");
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        return 0;
    }
    
    // Confirm with user
    if (!user_confirm("Proceed with transaction?", 1)) {
        log_info("Transaction cancelled by user");
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        return 0;
    }
    
    // Download and build packages
    log_info("Downloading packages...");
    transaction_order(trans, 0);

    // Reorder: ensure mkdep (build-time) packages come before packages that need them.
    // transaction_order() only respects runtime deps; mkdep edges must be applied manually.
    {
        Repodata *order_data = repo_id2repodata(repo, 1);
        if (order_data) {
            int n = trans->steps.count;
            int changed = 1;
            int passes = 0;
            while (changed && passes++ < n) {
                changed = 0;
                for (int i = 0; i < n; i++) {
                    Id p = trans->steps.elements[i];
                    json_t *mkdeps = get_custom_mkdeps(order_data, p, pool);
                    if (!mkdeps || !json_is_array(mkdeps)) {
                        if (mkdeps) json_decref(mkdeps);
                        continue;
                    }
                    size_t ki;
                    json_t *mk_item;
                    json_array_foreach(mkdeps, ki, mk_item) {
                        if (!json_is_string(mk_item)) continue;
                        const char *mkdep_name = json_string_value(mk_item);
                        Id mkdep_name_id = pool_str2id(pool, mkdep_name, 0);
                        if (!mkdep_name_id) continue;
                        // If mkdep appears at position j > i it would be installed after P — wrong
                        for (int j = i + 1; j < n; j++) {
                            Solvable *sj = pool_id2solvable(pool, trans->steps.elements[j]);
                            if (sj->name == mkdep_name_id) {
                                // Move steps[j] to position i, shift i..j-1 one position right
                                Id mkdep_solvable = trans->steps.elements[j];
                                for (int kk = j; kk > i; kk--)
                                    trans->steps.elements[kk] = trans->steps.elements[kk - 1];
                                trans->steps.elements[i] = mkdep_solvable;
                                changed = 1;
                                break;
                            }
                        }
                    }
                    json_decref(mkdeps);
                }
            }
        }
    }
    
    int exit_code = 0;
    
    for (int i = 0; i < trans->steps.count; i++) {
        Id p = trans->steps.elements[i];
        Id type = transaction_type(trans, p, SOLVER_TRANSACTION_RPM_ONLY);
        
        if (type != SOLVER_TRANSACTION_INSTALL && 
            type != SOLVER_TRANSACTION_MULTIINSTALL) {
            continue;
        }
        
        Solvable *s = pool_id2solvable(pool, p);
        const char *sname = pool_id2str(pool, s->name);
        const char *version = pool_id2str(pool, s->evr);
        
        // Skip if already installed
        if (is_package_installed(sname, target_root)) {
            log_info("Skipping %s - already installed in %s", sname, target_root);
            continue;
        }
        
        // Get source download URL
        Repodata *data = repo_id2repodata(repo, 1);
        const char *source_url = get_custom_source(data, p, pool);
        
        if (!source_url) {
            log_warn("No source URL for %s, skipping", sname);
            continue;
        }
        
        // Download source archive
        log_info("Downloading %s from %s", sname, source_url);
        const char *repo_path = command_context_get_repo_path(ctx);
        json_t *source_array = get_custom_source_array(data, p, pool);
        FILE *fp = ctx->subsystems.download_package(s, source_url, pool, source_array, repo_path);
        
        if (!fp) {
            log_error("Download failed for %s", sname);
            exit_code = ERR_DOWNLOAD_FAIL;
            break;
        }
        
        // Download extras (patches, custom build scripts)
        json_t *extras = get_custom_extras(data, p, pool);
        ctx->subsystems.download_extras(extras, sname, version, repo_path, NULL);
        
        // Build package into staging directory
        log_info("Building %s...", sname);
        int is_group = get_custom_is_group(data, p, pool);
        int no_package = get_custom_no_package(data, p, pool);
        
        int build_rc = ctx->subsystems.commit_element(
            trans, p, fp, pool, is_group, no_package, extras
        );
        
        if (build_rc != 0) {
            log_error("Build failed for %s", sname);
            exit_code = ERR_BUILD_FAIL;
            break;
        }
        
        log_info("Successfully built %s", sname);
        
        // Install if this is install command (not just build)
        if (do_install) {
            const char *root_display = strcmp(target_root, "/") == 0 ? 
                                       "system root (/)" : target_root;
            log_info("Installing %s to %s...", sname, root_display);
            
            // Determine install reason: manual if it's the requested package
            const char *install_reason = strcmp(sname, pkg_name) == 0 ? 
                                        "manual" : "dependency";
            
            // Install from staging to target root
            if (ctx->subsystems.install_package_with_reason(
                    sname, version, staging, target_root, install_reason) != 0) {
                log_error("Installation failed for %s", sname);
                exit_code = ERR_BUILD_FAIL;
                break;
            }
            
            log_info("Successfully installed %s to %s", sname, root_display);
            
            // Cleanup staging directory
            char cleanup_cmd[512];
            snprintf(cleanup_cmd, sizeof(cleanup_cmd), 
                    "rm -rf '%s' 2>/dev/null && mkdir -p '%s'", staging, staging);
            system(cleanup_cmd);
            log_debug(1, "Cleaned up staging directory: %s", staging);
        }
    }
    
    transaction_free(trans);
    queue_free(solution);
    solver_free(solv);
    
    return exit_code;
}

/**
 * Execute 'install' command.
 * Complete workflow: solve → download → build → install
 */
int cmd_install_execute(CommandContext *ctx) {
    return cmd_install_or_build_execute(ctx, 1);  // do_install=1
}

/**
 * Execute 'build' command.
 * Workflow: solve → download → build (no install)
 */
int cmd_build_execute(CommandContext *ctx) {
    return cmd_install_or_build_execute(ctx, 0);  // do_install=0
}
