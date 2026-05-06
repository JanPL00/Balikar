#ifndef BALIKAR_COMMAND_H
#define BALIKAR_COMMAND_H

typedef struct command_context CommandContext;

typedef struct command {
    const char *name;
    const char *description;

    int (*execute)(CommandContext *ctx);

    int (*validate)(CommandContext *ctx);

} Command;

#endif
