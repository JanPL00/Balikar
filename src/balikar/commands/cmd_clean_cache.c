// Příkaz: clean-cache – vyčistí cache stažených souborů
// Spravuje adresář /var/cache/balikar/downloads

#include "../includes/command_context.h"
#include "../../utils/includes/cli_helpers.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * Spustí příkaz 'clean-cache'.
 *
 * Vyčistí cache stažených souborů: buď pro konkrétní balík nebo celou cache.
 * Nevyžaduje pool/repo – jde o čistou operaci se souborovým systémem.
 */
int cmd_clean_cache_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    char *cache_dir = get_cache_dir();
    if (!cache_dir) {
        log_error("Failed to get cache directory");
        return 1;
    }
    
    if (ctx->config.clean_cache_pkg_allocated) {
        // Vyčisti cache pro konkrétní balík
        const char *pkg = ctx->config.clean_cache_pkg_allocated;
        log_info("Cleaning cache for package: %s", pkg);
        
        // Odstraň tar archivy a soubory kontrolních součtů sha256
        char pattern_cmd[2048];
        snprintf(pattern_cmd, sizeof(pattern_cmd), 
                 "rm -f \"%s/%s\"* 2>/dev/null", 
                 cache_dir, pkg);
        system(pattern_cmd);
        
        // Odstraň adresář extras (záplaty, vlastní build skripty)
        char extras_dir[2048];
        snprintf(extras_dir, sizeof(extras_dir), "%s/extras/%s", cache_dir, pkg);
        char rm_cmd[4096];
        snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\" 2>/dev/null", extras_dir);
        system(rm_cmd);
        
        log_info("Cache cleaned for package: %s", pkg);
        
    } else {
        // Vyčisti celý adresář cache
        log_info("Cleaning entire cache: %s", cache_dir);
        
        // Vyžaduj potvrzení uživatele (destruktivní operace)
        if (user_confirm("This will delete all cached downloads. Continue?", 0)) {
            char rm_cmd[2048];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"/* 2>/dev/null", cache_dir);
            int rc = system(rm_cmd);
            
            if (rc == 0) {
                log_info("Cache cleared successfully: %s", cache_dir);
            } else {
                log_error("Failed to clear cache");
                free(cache_dir);
                return 1;
            }
        } else {
            log_info("Cache cleaning cancelled");
        }
    }
    
    free(cache_dir);
    return 0;
}
