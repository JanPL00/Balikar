//
// Command Implementations - Forward Declarations
// Include this in command_registry.c
//

#ifndef BALIKAR_COMMANDS_H
#define BALIKAR_COMMANDS_H

#include "command_context.h"

// Simple commands (already implemented)
int cmd_repos_execute(CommandContext *ctx);
int cmd_autoremove_execute(CommandContext *ctx);
int cmd_remove_execute(CommandContext *ctx);
int cmd_resolve_execute(CommandContext *ctx);
int cmd_clean_cache_execute(CommandContext *ctx);
int cmd_repo_update_execute(CommandContext *ctx);

// Complex commands (to be implemented in later milestones)
int cmd_install_execute(CommandContext *ctx);
int cmd_build_execute(CommandContext *ctx);
int cmd_download_execute(CommandContext *ctx);
int cmd_repo_execute(CommandContext *ctx);

// Validation functions
int cmd_install_validate(CommandContext *ctx);
int cmd_remove_validate(CommandContext *ctx);
int cmd_download_validate(CommandContext *ctx);

#endif // BALIKAR_COMMANDS_H
