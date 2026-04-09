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
    display_transaction(trans);
    
    // Cleanup
    transaction_free(trans);
    queue_free(solution);
    solver_free(solv);
    
    return 0;
}
