//
// Command Registry - Dispatcher Pattern
// Lookup table for all registered commands
//

#ifndef BALIKAR_COMMAND_REGISTRY_H
#define BALIKAR_COMMAND_REGISTRY_H

#include "command.h"
#include "command_context.h"

/**
 * Lookup command by name in registry.
 * Returns: Command pointer or NULL if not found
 */
Command* command_lookup(const char *name);

/**
 * Dispatch command execution.
 * Looks up command, validates, and executes.
 * Returns: Command exit code (0=success)
 */
int command_dispatch(CommandContext *ctx);

/**
 * List all registered commands.
 * Used for help/usage display.
 */
void command_list_all(void);

#endif // BALIKAR_COMMAND_REGISTRY_H
