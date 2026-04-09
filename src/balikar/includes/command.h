//
// Command Pattern Interface
// Defines unified interface for all balikar commands
//

#ifndef BALIKAR_COMMAND_H
#define BALIKAR_COMMAND_H

// Forward declaration to avoid circular dependency
typedef struct command_context CommandContext;

/**
 * Command interface using function pointers.
 * Each command implements execute() and optionally validate().
 * Commands are stateless - all state is in CommandContext.
 */
typedef struct command {
    const char *name;        // Command name (e.g., "install", "remove")
    const char *description; // Short description for help text
    
    // Execute command with given context
    // Returns: 0 on success, non-zero error code on failure
    int (*execute)(CommandContext *ctx);
    
    // Optional: Validate prerequisites before execution
    // Returns: 0 if valid, non-zero if validation fails
    // Can be NULL if no validation needed
    int (*validate)(CommandContext *ctx);
    
} Command;

#endif // BALIKAR_COMMAND_H
