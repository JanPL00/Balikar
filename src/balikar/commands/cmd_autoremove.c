// Příkaz: autoremove – odstraní osiřelé závislosti
// Fasáda nad installer.c autoremove_packages()

#include "../includes/command_context.h"
#include "../includes/installer.h"

/**
 * Spustí příkaz 'autoremove'.
 *
 * Vyhledá a odstraní balíky nainstalované jako závislosti, které již nejsou potřeba.
 * Vyžaduje pool/repo pro analýzu závislostí.
 */
int cmd_autoremove_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    // Zajisti načtení poolu a repo (potřebné pro analýzu závislostí)
    Pool *pool = command_context_ensure_pool(ctx);
    Repo *repo = command_context_ensure_repo(ctx);
    if (!pool || !repo) {
        log_error("Failed to initialize pool/repo for autoremove");
        return 1;
    }
    
    const char *target_root = command_context_get_root_path(ctx);
    
    log_info("Searching for orphaned packages in %s...", target_root);
    
    // Zavolej subsystém installeru pro nalezení a odstranění osiřelých balíků
    // Předá load_installed_db a erase_package jako callbacky
    int rc = autoremove_packages(
        target_root, 
        pool, 
        repo, 
        ctx->config.dry_run,
        load_installed_db,  // Callback pro načtení installed.json
        erase_package       // Callback pro odstranění balíku
    );
    
    if (rc < 0) {
        log_error("Autoremove failed");
        return 1;
    }
    
    return 0;
}
