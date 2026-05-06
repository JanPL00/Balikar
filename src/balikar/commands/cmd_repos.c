//
// Příkaz: repos – zobrazí informace o repozitářích
// Fasáda nad config.c list_repositories()
//

#include "../includes/command_context.h"
#include "../../common/config.h"
#include <stdio.h>

/**
 * Spustí příkaz 'repos'.
 *
 * Zobrazí všechny nakonfigurované repozitáře a aktuálně aktivní repozitář.
 * Nevyžaduje načítání pool/repo – jde o čisté zobrazení konfigurace.
 */
int cmd_repos_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    // Vypiš všechny nakonfigurované repozitáře z repos.conf
    list_repositories();
    
    // Zobraz aktuálně používaný repozitář
    const char *repo_path = command_context_get_repo_path(ctx);
    printf("\nCurrently using: %s\n", repo_path);
    
    // Pokud byl pool/repo již načten, zobraz počet balíků
    // Jinak přeskoč (vyvaruj se zbytečného načítání)
    if (ctx->resources.repo) {
        printf("Packages available: %d\n", ctx->resources.repo->nsolvables);
    }
    
    return 0;
}
