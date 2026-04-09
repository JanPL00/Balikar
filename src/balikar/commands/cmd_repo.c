//
// Command: repo - Manage repositories (list, add, remove, enable, disable)
// Facade over config.c repository management functions
//

#include "../includes/command_context.h"
#include "../../common/config.h"
#include <stdlib.h>
#include <string.h>

/**
 * Execute 'repo' command with subcommands.
 * No pool/repo needed - pure configuration management.
 * 
 * Subcommands:
 * - list: Show all repositories
 * - add <name> <url> [priority]: Add new repository
 * - remove <name>: Remove repository
 * - enable <name>: Enable repository
 * - disable <name>: Disable repository
 */
int cmd_repo_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    // Initialize repo config
    init_repo_config();
    
    // Subcommand is in package_name field
    const char *subcommand = ctx->config.package_name;
    
    if (!subcommand) {
        log_error("Subcommand required for 'repo'. Use: list, add, remove, enable, disable");
        free_repo_config();
        return 1;
    }
    
    // Handle 'list' subcommand
    if (strcmp(subcommand, "list") == 0) {
        list_repositories();
        free_repo_config();
        return 0;
    }
    
    // For other subcommands, we need to access raw argc/argv
    // Context doesn't store original argc/argv, so we need to handle this differently
    // For now, log error and suggest using main.c implementation
    // This will be properly handled when we integrate with main.c
    
    log_error("Repo subcommand '%s' requires additional arguments not available in current context", subcommand);
    log_error("This will be fixed when main.c is updated to use command pattern");
    free_repo_config();
    return 1;
}
