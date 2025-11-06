//
// Created by placeja5 on 11/6/25.
//

#ifndef IMPL_SYSTEM_HELPERS_H
#define IMPL_SYSTEM_HELPERS_H

#include "../../common/common.h"

/**
 * Nastaví architekturu poolu podle systému (adaptace setarch z solv.c).
 * Použije uname pro detekci, fallback na "x86_64".
 * Podpora noarch defaultů pro data-only pkgs (budoucí integrace s parserem).
 */
void set_system_arch(Pool *pool);

#endif //IMPL_SYSTEM_HELPERS_H