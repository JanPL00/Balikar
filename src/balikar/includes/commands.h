#ifndef BALIKAR_COMMANDS_H
#define BALIKAR_COMMANDS_H

#include "command_context.h"

int cmd_repos_execute(CommandContext *ctx);
int cmd_autoremove_execute(CommandContext *ctx);
int cmd_remove_execute(CommandContext *ctx);
int cmd_resolve_execute(CommandContext *ctx);
int cmd_clean_cache_execute(CommandContext *ctx);
int cmd_repo_update_execute(CommandContext *ctx);

int cmd_install_execute(CommandContext *ctx);
int cmd_build_execute(CommandContext *ctx);
int cmd_download_execute(CommandContext *ctx);
int cmd_repo_execute(CommandContext *ctx);

int cmd_install_validate(CommandContext *ctx);
int cmd_remove_validate(CommandContext *ctx);
int cmd_download_validate(CommandContext *ctx);

#endif
