//
// Příkaz: remove – odstraní nainstalovaný balík
// Fasáda nad installer.c erase_package_with_checks()
//

#include "../includes/command_context.h"
#include "../includes/installer.h"

/**
 * Spustí příkaz 'remove'.
 *
 * Odstraní zadaný balík z cílového rootu s kontrolou závislostí.
 * Ověří: package_name je nastaveno (zpracováno validací registru)
 */
int cmd_remove_execute(CommandContext *ctx) {
    if (!ctx || !ctx->config.package_name) {
        log_error("Package name required for remove command");
        return 1;
    }
    
    // Zajisti načtení poolu a repo (potřebné pro kontrolu reverzních závislostí)
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
    
    // Zavolej subsystém installeru s povolenou kontrolou závislostí
    // check_deps=1 zajistí varování pokud na tomto balíku závisí jiné balíky
    int rc = erase_package_with_checks(
        pkg_name,
        target_root,
        pool,
        repo,
        1,                      // check_deps=1 (validuj před odstraněním)
        load_installed_db,      // Callback pro načtení installed.json
        erase_package           // Callback pro skutečné odstranění
    );
    
    if (rc != 0) {
        log_error("Failed to remove package '%s'", pkg_name);
        return 1;
    }
    
    return 0;
}
