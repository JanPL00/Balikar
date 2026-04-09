//
// Created by placeja5 on 11/2/25.
// Refactored: 2026-04-09 - Command Pattern + Dispatcher
//
#include "includes/command_context.h"
#include "includes/command_registry.h"
#include "../common/common.h"

/**
 * Main entry point for Balikar package manager.
 * Refactored using Command Pattern + Dispatcher for modular command handling.
 * 
 * Workflow:
 * 1. Create command context (holds CLI config + lazy resources)
 * 2. Parse command-line arguments into context
 * 3. Dispatch to appropriate command handler
 * 4. Cleanup and return exit code
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code: 0 on success, error code on failure
 */
int main(int argc, char **argv) {
    // Step 1: Create command context
    CommandContext *ctx = command_context_create();
    if (!ctx) {
        log_error("Failed to create command context.");
        return 1;
    }
    
    // Step 1.5: Initialize subsystem facades
    command_context_init_subsystems(ctx);
    
    // Step 2: Parse command-line arguments
    // This populates CLI config and sets DEBUG_LEVEL
    if (command_context_parse_args(ctx, argc, argv) != 0) {
        // Parse error or --help was requested
        command_context_destroy(ctx);
        return 1;
    }
    
    // Step 3: Dispatch to command handler
    // Lookup command, validate, and execute
    int exit_code = command_dispatch(ctx);
    
    // Step 4: Cleanup
    command_context_destroy(ctx);
    
    return exit_code;
}
