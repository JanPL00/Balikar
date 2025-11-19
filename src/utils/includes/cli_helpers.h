//
// Created by placeja5 on 11/4/25.
//

#ifndef IMPL_CLI_HELPERS_H
#define IMPL_CLI_HELPERS_H

#include "../../common/common.h"

// Public CLI helpers API

// Display program usage information and exit.
// Shows all available commands, options, and usage examples.
// exit_code: Exit code to use when terminating (0 for success, non-zero for error)
void print_usage(int exit_code);

// Prompt user for yes/no confirmation with default value.
// Accepts 'y', 'n', or Enter for default. 'q' aborts the program.
// Loops until valid input is received.
// prompt: Question to display to user
// default_yes: Default answer if user presses Enter (1=yes, 0=no)
// Returns 1 for yes, 0 for no (never returns on abort/error)
int user_confirm(const char *prompt, int default_yes);

#endif //IMPL_CLI_HELPERS_H