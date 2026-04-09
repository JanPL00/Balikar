//
// Command: repo-update - Refresh repository metadata  
// Facade over parser.c update_repository_metadata()
//

#include "../includes/command_context.h"
#include "../includes/parser.h"

/**
 * Execute 'repo-update' command.
 * Downloads new metadata for remote repos or regenerates for local repos.
 * Reloads repository after update.
 */
int cmd_repo_update_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    const char *repo_path = command_context_get_repo_path(ctx);
    log_info("Updating repository metadata: %s", repo_path);
    
    // If pool/repo already loaded, free them to start fresh
    if (ctx->resources.repo) {
        repo_free(ctx->resources.repo, 1);
        ctx->resources.repo = NULL;
    }
    
    if (ctx->resources.pool) {
        pool_free(ctx->resources.pool);
        ctx->resources.pool = NULL;
    }
    
    // Update metadata (download for remote, regenerate for local)
    int rc = update_repository_metadata(repo_path);
    if (rc != 0) {
        log_error("Failed to update repository metadata");
        return 1;
    }
    
    log_info("Reloading repository...");
    
    // Reload with fresh metadata
    Pool *pool = command_context_ensure_pool(ctx);
    Repo *repo = command_context_ensure_repo(ctx);
    
    if (!pool || !repo) {
        log_error("Failed to reload repository");
        return 1;
    }
    
    log_info("Repository reloaded: %d packages found", repo->nsolvables);
    
    return 0;
}
