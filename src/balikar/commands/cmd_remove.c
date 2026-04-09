//
// Command: remove - Remove installed package
// Facade over installer.c erase_package_with_checks()
//

#include "../includes/command_context.h"
#include "../includes/installer.h"

/**
 * Execute 'remove' command.
 * Removes specified package from target root with dependency checking.
 * Validates: package_name is set (handled by registry validation)
 */
int cmd_remove_execute(CommandContext *ctx) {
    if (!ctx || !ctx->config.package_name) {
        log_error("Package name required for remove command");
        return 1;
    }
    
    // Ensure pool and repo are loaded (needed for reverse dependency checking)
    Pool *pool = command_context_ensure_pool(ctx);
    Repo *repo = command_context_ensure_repo(ctx);
    if (!pool || !repo) {
        log_error("Failed to initialize pool/repo for remove");
        return 1;
    }
    
    const char *target_root = command_context_get_root_path(ctx);
    const char *pkg_name = ctx->config.package_name;
    
    log_info("Removing package: %s", pkg_name);
    
    if (ctx->config.dry_run) {
        log_info("DRY-RUN: Would remove package '%s' from %s", pkg_name, target_root);
        return 0;
    }
    
    // Call installer subsystem with dependency checking enabled
    // check_deps=1 ensures we warn if other packages depend on this one
    int rc = erase_package_with_checks(
        pkg_name,
        target_root,
        pool,
        repo,
        1,                      // check_deps=1 (validate before removing)
        load_installed_db,      // Callback for loading installed.json
        erase_package           // Callback for actual removal
    );
    
    if (rc != 0) {
        log_error("Failed to remove package '%s'", pkg_name);
        return 1;
    }
    
    return 0;
}
