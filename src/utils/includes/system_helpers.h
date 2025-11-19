//
// Created by placeja5 on 11/6/25.
//

#ifndef IMPL_SYSTEM_HELPERS_H
#define IMPL_SYSTEM_HELPERS_H

#include "../../common/common.h"

// Public system helpers API

// Detect and configure system architecture for the dependency resolver.
// Uses uname to detect current system architecture and normalizes it.
// Sets architecture in libsolv pool for proper package selection.
// Supports x86_64 and i686, defaults to x86_64 for unknown architectures.
// Based on architecture scoring from libsolv's solv.c setarch.
// pool: Pool instance to configure
void set_system_arch(Pool *pool);

#endif //IMPL_SYSTEM_HELPERS_H