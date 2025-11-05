//
// Created by placeja5 on 11/4/25.
//

#ifndef IMPL_CLI_HELPERS_H
#define IMPL_CLI_HELPERS_H

#include "../../common/common.h"

// ========================================
// CLI HELPERS PROTOTYPES
// ========================================

/**
 * Print usage information for the PM CLI tool.
 * @param exit_code Exit code to use if calling exit() (e.g., 1 for error).
 */
void print_usage(int exit_code);

/**
 * Prompt user for yes/no confirmation.
 * @param prompt The prompt string to display.
 * @param default_yes Default response if enter is pressed ('y' or 'n').
 * @return 1 for yes/true, 0 for no/false. Exits on 'q' or EOF.
 */
int user_confirm(const char *prompt, int default_yes);

#endif //IMPL_CLI_HELPERS_H