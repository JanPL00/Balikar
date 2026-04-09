//
// Command: autoremove - Remove orphaned dependencies
// Facade over installer.c autoremove_packages()
//

#include "../includes/command_context.h"
#include "../includes/installer.h"

/**
 * Execute 'autoremove' command.
 * Searches for and removes packages installed as dependencies that are no longer needed.
 * Requires pool/repo for dependency checking.
 */
int cmd_autoremove_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    // Ensure pool and repo are loaded (needed for dependency analysis)
    Pool *pool = command_context_ensure_pool(ctx);
    Repo *repo = command_context_ensure_repo(ctx);
    if (!pool || !repo) {
        log_error("Failed to initialize pool/repo for autoremove");
        return 1;
    }
    
    const char *target_root = command_context_get_root_path(ctx);
    
    log_info("Searching for orphaned packages in %s...", target_root);
    
    // Call installer subsystem to find and remove orphans
    // Passes load_installed_db and erase_package as callbacks
    int rc = autoremove_packages(
        target_root, 
        pool, 
        repo, 
        ctx->config.dry_run,
        load_installed_db,  // Callback for loading installed.json
        erase_package       // Callback for package removal
    );
    
    if (rc < 0) {
        log_error("Autoremove failed");
        return 1;
    }
    
    return 0;
}
