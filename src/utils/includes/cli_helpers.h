#ifndef IMPL_CLI_HELPERS_H
#define IMPL_CLI_HELPERS_H

#include "../../common/common.h"

void print_usage(int exit_code);

int user_confirm(const char *prompt, int default_yes);

#endif