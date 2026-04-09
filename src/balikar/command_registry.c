//
// Command Registry Implementation
//

#include "includes/command_registry.h"
#include "includes/commands.h"
#include "../common/common.h"
#include <string.h>

/**
 * Static command registry - zero heap allocation.
 * All commands registered at compile time.
 * Memory footprint: ~1 KB for 10 commands.
 */
static Command commands[] = {
    {
        .name = "install",
        .description = "Install package with dependency resolution",
        .execute = cmd_install_execute,  // ✅ IMPLEMENTED
        .validate = cmd_install_validate
    },
    {
        .name = "build",
        .description = "Build package from source (with mkdeps)",
        .execute = cmd_build_execute,    // ✅ IMPLEMENTED
        .validate = cmd_install_validate
    },
    {
        .name = "resolve",
        .description = "Resolve dependencies without installing",
        .execute = cmd_resolve_execute,  // ✅ IMPLEMENTED
        .validate = NULL
    },
    {
        .name = "remove",
        .description = "Remove installed package",
        .execute = cmd_remove_execute,   // ✅ IMPLEMENTED
        .validate = cmd_remove_validate
    },
    {
        .name = "autoremove",
        .description = "Remove orphaned dependencies",
        .execute = cmd_autoremove_execute,  // ✅ IMPLEMENTED
        .validate = NULL
    },
    {
        .name = "download",
        .description = "Download packages without installing",
        .execute = cmd_download_execute,  // ✅ IMPLEMENTED
        .validate = cmd_download_validate
    },
    {
        .name = "repo",
        .description = "Manage repositories (list, add, remove, enable, disable)",
        .execute = cmd_repo_execute,      // ⚠️ PARTIAL (only 'list' works)
        .validate = NULL
    },
    {
        .name = "repos",
        .description = "Show repository information",
        .execute = cmd_repos_execute,    // ✅ IMPLEMENTED
        .validate = NULL
    },
    {
        .name = "repo-update",
        .description = "Update repository metadata",
        .execute = cmd_repo_update_execute,  // ✅ IMPLEMENTED
        .validate = NULL
    },
    {
        .name = "clean-cache",
        .description = "Clear download cache",
        .execute = cmd_clean_cache_execute,  // ✅ IMPLEMENTED
        .validate = NULL
    },
    {NULL, NULL, NULL, NULL}  // Terminator
};

/**
 * Lookup command by name.
 * O(n) lookup where n=10 (negligible overhead).
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
 * Dispatch command execution with validation.
 */
int command_dispatch(CommandContext *ctx) {
    if (!ctx || !ctx->config.command) {
        log_error("No command specified");
        return 1;
    }
    
    // Lookup command
    Command *cmd = command_lookup(ctx->config.command);
    if (!cmd) {
        log_error("Unknown command: %s", ctx->config.command);
        log_info("Use 'balikar --help' to see available commands");
        return 1;
    }
    
    // Validate if validator exists
    if (cmd->validate) {
        int rc = cmd->validate(ctx);
        if (rc != 0) {
            log_error("Command validation failed");
            return rc;
        }
    }
    
    // Execute command
    log_debug(1, "Executing command: %s", cmd->name);
    return cmd->execute(ctx);
}

/**
 * List all registered commands (for help).
 */
void command_list_all(void) {
    printf("Available commands:\n");
    for (int i = 0; commands[i].name; i++) {
        printf("  %-15s %s\n", commands[i].name, commands[i].description);
    }
}

// ========================================
// VALIDATION FUNCTIONS
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
