//
// Příkaz: repo-update – obnoví metadata repozitáře
// Fasáda nad parser.c update_repository_metadata()
//

#include "../includes/command_context.h"
#include "../includes/parser.h"

/**
 * Spustí příkaz 'repo-update'.
 *
 * Stáhne nová metadata pro vzdálené repozitáře nebo regeneruje pro lokální.
 * Po aktualizaci znovu načte repozitář.
 */
int cmd_repo_update_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    const char *repo_path = command_context_get_repo_path(ctx);
    log_info("Updating repository metadata: %s", repo_path);
    
    // Pokud jsou pool/repo již načteny, uvolni je pro čerstvý start
    if (ctx->resources.repo) {
        repo_free(ctx->resources.repo, 1);
        ctx->resources.repo = NULL;
    }
    
    if (ctx->resources.pool) {
        pool_free(ctx->resources.pool);
        ctx->resources.pool = NULL;
    }
    
    // Aktualizuj metadata (stáhni pro vzdálené, regeneruj pro lokální)
    int rc = update_repository_metadata(repo_path);
    if (rc != 0) {
        log_error("Failed to update repository metadata");
        return 1;
    }
    
    log_info("Reloading repository...");
    
    // Znovu načti s čerstvými metadaty
    Pool *pool = command_context_ensure_pool(ctx);
    Repo *repo = command_context_ensure_repo(ctx);
    
    if (!pool || !repo) {
        log_error("Failed to reload repository");
        return 1;
    }
    
    log_info("Repository reloaded: %d packages found", repo->nsolvables);
    
    return 0;
}
