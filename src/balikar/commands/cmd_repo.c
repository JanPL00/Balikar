//
// Příkaz: repo – správa repozitářů (list, add, remove, enable, disable)
// Fasáda nad funkcemi správy repozitářů v config.c
//

#include "../includes/command_context.h"
#include "../../common/config.h"
#include <stdlib.h>
#include <string.h>

/**
 * Spustí příkaz 'repo' s podpříkazy.
 *
 * Nevyžaduje pool/repo – jde o čistou správu konfigurace.
 *
 * Podpříkazy:
 * - list: Zobrazí všechny repozitáře
 * - add <název> <url> [priorita]: Přidá nový repozitář
 * - remove <název>: Odstraní repozitář
 * - enable <název>: Povolí repozitář
 * - disable <název>: Zakáže repozitář
 */
int cmd_repo_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    // Inicializuj konfiguraci repo
    init_repo_config();
    
    // Podpříkaz je v poli package_name
    const char *subcommand = ctx->config.package_name;
    
    if (!subcommand) {
        log_error("Subcommand required for 'repo'. Use: list, add, remove, enable, disable");
        free_repo_config();
        return 1;
    }
    
    // Zpracuj podpříkaz 'list'
    if (strcmp(subcommand, "list") == 0) {
        list_repositories();
        free_repo_config();
        return 0;
    }
    
    // Pro ostatní podpříkazy potřebujeme přístup k surové argc/argv
    // Kontext neuchovává původní argc/argv, proto je třeba to řešit jinak
    // Prozatím zaloguj chybu a navrhni použití implementace main.c
    // Bude správně ošetřeno při integraci s main.c
    
    log_error("Repo subcommand '%s' requires additional arguments not available in current context", subcommand);
    log_error("This will be fixed when main.c is updated to use command pattern");
    free_repo_config();
    return 1;
}
