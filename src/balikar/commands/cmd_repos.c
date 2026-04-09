//
// Command: repos - Display repository information
// Facade over config.c list_repositories()
//

#include "../includes/command_context.h"
#include "../../common/config.h"
#include <stdio.h>

/**
 * Execute 'repos' command.
 * Shows all configured repositories and currently active repository.
 * No pool/repo loading needed - pure config display.
 */
int cmd_repos_execute(CommandContext *ctx) {
    if (!ctx) return 1;
    
    // List all configured repositories from repos.conf
    list_repositories();
    
    // Show currently used repository
    const char *repo_path = command_context_get_repo_path(ctx);
    printf("\nCurrently using: %s\n", repo_path);
    
    // If pool/repo were already loaded, show package count
    // Otherwise skip (avoid unnecessary loading)
    if (ctx->resources.repo) {
        printf("Packages available: %d\n", ctx->resources.repo->nsolvables);
    }
    
    return 0;
}
