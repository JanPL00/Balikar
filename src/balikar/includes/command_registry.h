#ifndef BALIKAR_COMMAND_REGISTRY_H
#define BALIKAR_COMMAND_REGISTRY_H

#include "command.h"
#include "command_context.h"

Command* command_lookup(const char *name);

int command_dispatch(CommandContext *ctx);

void command_list_all(void);

#endif
