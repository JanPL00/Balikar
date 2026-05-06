//
// Implementace registru příkazů
//

#include "includes/command_registry.h"
#include "includes/commands.h"
#include "../common/common.h"

/**
 * Statický registr příkazů – žádná heap alokace.
 *
 * Všechny příkazy zaregistrovány v době kompilace.
 * Paměťová stopa: ~1 KB pro 10 příkazů.
 */
static Command commands[] = {
    {
        .name = "install",
        .description = "Install package with dependency resolution",
        .execute = cmd_install_execute,  
        .validate = cmd_install_validate
    },
    {
        .name = "build",
        .description = "Build package from source (with mkdeps)",
        .execute = cmd_build_execute,    
        .validate = cmd_install_validate
    },
    {
        .name = "resolve",
        .description = "Resolve dependencies without installing",
        .execute = cmd_resolve_execute,  
        .validate = NULL
    },
    {
        .name = "remove",
        .description = "Remove installed package",
        .execute = cmd_remove_execute,   
        .validate = cmd_remove_validate
    },
    {
        .name = "autoremove",
        .description = "Remove orphaned dependencies",
        .execute = cmd_autoremove_execute,  
        .validate = NULL
    },
    {
        .name = "download",
        .description = "Download packages without installing",
        .execute = cmd_download_execute,  
        .validate = cmd_download_validate
    },
    {
        .name = "repo",
        .description = "Manage repositories (list, add, remove, enable, disable)",
        .execute = cmd_repo_execute,      // ČÁSTEČNĚ (funguje pouze 'list')
        .validate = NULL
    },
    {
        .name = "repos",
        .description = "Show repository information",
        .execute = cmd_repos_execute,    
        .validate = NULL
    },
    {
        .name = "repo-update",
        .description = "Update repository metadata",
        .execute = cmd_repo_update_execute,  
        .validate = NULL
    },
    {
        .name = "clean-cache",
        .description = "Clear download cache",
        .execute = cmd_clean_cache_execute, 
        .validate = NULL
    },
    {NULL, NULL, NULL, NULL}  // Ukončovač
};

/**
 * Vyhledá příkaz podle názvu.
 *
 * Složitost O(n) kde n=10 (zanedbatelná režie).
 */
Command* command_lookup(const char *name) {
    if (!name) return NULL;
    
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    
    return NULL;
}

/**
 * Odešle příkaz k vykonání s validací.
 */
int command_dispatch(CommandContext *ctx) {
    if (!ctx || !ctx->config.command) {
        log_error("No command specified");
        return 1;
    }
    
    // Vyhledej příkaz
    Command *cmd = command_lookup(ctx->config.command);
    if (!cmd) {
        log_error("Unknown command: %s", ctx->config.command);
        log_info("Use 'balikar --help' to see available commands");
        return 1;
    }
    
    // Validuj pokud existuje validátor
    if (cmd->validate) {
        int rc = cmd->validate(ctx);
        if (rc != 0) {
            log_error("Command validation failed");
            return rc;
        }
    }
    
    // Spusť příkaz
    log_debug(1, "Executing command: %s", cmd->name);
    return cmd->execute(ctx);
}

/**
 * Vypíše všechny registrované příkazy (pro nápovědu).
 */
void command_list_all(void) {
    printf("Available commands:\n");
    for (int i = 0; commands[i].name; i++) {
        printf("  %-15s %s\n", commands[i].name, commands[i].description);
    }
}

// ========================================
// VALIDAČNÍ FUNKCE
// ========================================

int cmd_install_validate(CommandContext *ctx) {
    if (!ctx->config.package_name && !ctx->config.local_package_allocated) {
        log_error("Package name required for install/build command");
        return 1;
    }
    return 0;
}

int cmd_remove_validate(CommandContext *ctx) {
    if (!ctx->config.package_name) {
        log_error("Package name required for remove command");
        return 1;
    }
    return 0;
}

int cmd_download_validate(CommandContext *ctx) {
    if (!ctx->config.package_name) {
        log_error("Package name required for download command");
        return 1;
    }
    return 0;
}
