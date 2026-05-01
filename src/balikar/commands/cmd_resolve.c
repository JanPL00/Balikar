//
// Command: resolve - Resolve dependencies without installing
// Facade over solver.c solve_dependencies() + print.c display_transaction()
//

#include "../includes/command_context.h"
#include "../includes/solver.h"
#include "../includes/parser.h"
#include "../../utils/includes/print.h"

/**
 * Execute 'resolve' command.
 * Shows what packages would be installed to satisfy dependencies.
 * Dry-run mode by nature - no downloads or installations.
 */
int cmd_resolve_execute(CommandContext *ctx) {
    if (!ctx || !ctx->config.package_name) {
        log_error("Package name required for resolve command");
        return 1;
    }
    
    // Ensure pool and repo are loaded
    Pool *pool = command_context_ensure_pool(ctx);
    Repo *repo = command_context_ensure_repo(ctx);
    if (!pool || !repo) {
        log_error("Failed to initialize pool/repo for resolve");
        return 1;
    }
    
    const char *pkg_name = ctx->config.package_name;
    const char *target_root = command_context_get_root_path(ctx);
    
    log_info("Resolving dependencies for: %s", pkg_name);
    
    // Create solver
    Solver *solv = ctx->subsystems.create_solver(pool);
    if (!solv) {
        log_error("Failed to create solver");
        return 1;
    }
    
    // Solve dependencies
    Queue *solution = ctx->subsystems.solve_dependencies(
        solv,
        pkg_name,
        pool,
        repo,
        target_root,
        get_custom_mkdeps  // Makedeps callback for build-time deps
    );
    
    if (!solution) {
        log_error("Dependency resolution failed");
        solver_free(solv);
        return 1;
    }
    
    // Create and display transaction
    Transaction *trans = solver_create_transaction(solv);
    if (!trans) {
        log_error("Failed to create transaction");
        queue_free(solution);
        solver_free(solv);
        return 1;
    }
    
    // Display what would be installed
    transaction_order(trans, 0);

    // Reorder so mkdep (build-time) packages appear before the packages that need them
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
                        for (int j = i + 1; j < n; j++) {
                            Solvable *sj = pool_id2solvable(pool, trans->steps.elements[j]);
                            if (sj->name == mkdep_name_id) {
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

    display_transaction(trans);
    
    // Cleanup
    transaction_free(trans);
    queue_free(solution);
    solver_free(solv);
    
    return 0;
}
