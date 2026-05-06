// Příkaz: install/build – kompletní workflow od řešení závislostí po instalaci
// Nejkomplexnější příkaz – zpracovává jak install, tak build

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
 * Sdílená implementace příkazů install a build.
 *
 * @param ctx Kontext příkazu
 * @param do_install Pokud 1, provede instalaci po sestavení. Pokud 0, pouze sestaví.
 * @return Exit kód
 */
int cmd_install_or_build_execute(CommandContext *ctx, int do_install) {
    if (!ctx) return 1;
    
    // Validace již provedena registrem
    if (!ctx->config.package_name && !ctx->config.local_package_allocated) {
        log_error("Package name or --local-package required");
        return 1;
    }
    
    // Zajisti načtení poolu a repo
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
    
    // Vytvoř solver
    Solver *solv = ctx->subsystems.create_solver(pool);
    if (!solv) {
        log_error("Failed to create solver");
        return ERR_SOLVE_FAIL;
    }
    
    // Vyřeš závislosti
    Queue *solution = ctx->subsystems.solve_dependencies(
        solv, pkg_name, pool, repo, target_root, get_custom_mkdeps
    );
    
    if (!solution) {
        log_error("Dependency resolution failed for %s", pkg_name);
        solver_free(solv);
        return ERR_SOLVE_FAIL;
    }
    
    // Vytvoř transakci
    Transaction *trans = solver_create_transaction(solv);
    if (!trans) {
        log_error("Failed to create transaction");
        queue_free(solution);
        solver_free(solv);
        return ERR_SOLVE_FAIL;
    }
    
    // Zobraz transakci
    if (trans->steps.count == 0) {
        log_info("No changes needed - package may already be resolved");
    } else {
        display_transaction(trans);
    }
    
    // V dry-run módu zastav zde
    if (ctx->config.dry_run) {
        log_info("DRY-RUN: Skipping download and build");
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        return 0;
    }
    
    // Potvrď s uživatelem
    if (!user_confirm("Proceed with transaction?", 1)) {
        log_info("Transaction cancelled by user");
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        return 0;
    }
    
    // Stáhni a sestav balíky
    log_info("Downloading packages...");
    transaction_order(trans, 0);
    
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
        
        // Přeskoč pokud je již nainstalován
        if (is_package_installed(sname, target_root)) {
            log_info("Skipping %s - already installed in %s", sname, target_root);
            continue;
        }
        
        // Získej zdrojovou URL ke stažení
        Repodata *data = repo_id2repodata(repo, 1);
        const char *source_url = get_custom_source(data, p, pool);
        
        if (!source_url) {
            log_warn("No source URL for %s, skipping", sname);
            continue;
        }
        
        // Stáhni zdrojový archiv
        log_info("Downloading %s from %s", sname, source_url);
        const char *repo_path = command_context_get_repo_path(ctx);
        json_t *source_array = get_custom_source_array(data, p, pool);
        FILE *fp = ctx->subsystems.download_package(s, source_url, pool, source_array, repo_path);
        
        if (!fp) {
            log_error("Download failed for %s", sname);
            exit_code = ERR_DOWNLOAD_FAIL;
            break;
        }
        
        // Stáhni extras (záplaty, vlastní build skripty)
        json_t *extras = get_custom_extras(data, p, pool);
        ctx->subsystems.download_extras(extras, sname, version, repo_path, NULL);
        
        // Sestav balík do staging adresáře
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
        
        // Instaluj pokud jde o příkaz install (ne jen build)
        if (do_install) {
            const char *root_display = strcmp(target_root, "/") == 0 ? 
                                       "system root (/)" : target_root;
            log_info("Installing %s to %s...", sname, root_display);
            
            // Urči důvod instalace: manual pokud jde o požadovaný balík
            const char *install_reason = strcmp(sname, pkg_name) == 0 ? 
                                        "manual" : "dependency";
            
            // Instaluj ze staging adresáře do cílového rootu
            if (ctx->subsystems.install_package_with_reason(
                    sname, version, staging, target_root, install_reason) != 0) {
                log_error("Installation failed for %s", sname);
                exit_code = ERR_BUILD_FAIL;
                break;
            }
            
            log_info("Successfully installed %s to %s", sname, root_display);
            
            // Vyčisti staging adresář
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
    return cmd_install_or_build_execute(ctx, 1);  // do_install=1 (provést instalaci)
}

/**
 * Execute 'build' command.
 * Workflow: solve → download → build (no install)
 */
int cmd_build_execute(CommandContext *ctx) {
    return cmd_install_or_build_execute(ctx, 0);  // do_install=0 (pouze sestavení)
}
