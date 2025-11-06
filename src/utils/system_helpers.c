//
// Created by placeja5 on 11/6/25.
//

#include "includes/system_helpers.h"
#include <solv/pool.h>

//
// system_helpers.c - Helpery pro systémové nastavení (arch, env atd.)
// Adaptováno z solv.c setarch (řádky ~51-70).
// Created by [tvé jméno] on 05/11/2025.
//


void set_system_arch(Pool *pool) {
    if (!pool) {
        log_error("Invalid pool in set_system_arch.");
        return;
    }

    struct utsname un;
    const char *arch = NULL;

    // Zkus detekovat arch podle systému
    if (uname(&un) == 0) {
        arch = un.machine;  // Např. "x86_64"
        log_info("Detected system arch: %s", arch);
    } else {
        perror("uname failed");
        log_warn("Falling back to default arch: x86_64");
        arch = "x86_64";
    }

    // Adaptace pro náš projekt: Podpora noarch defaultu
    // (Později: Pokud parser načte index.json s "noarch", přepište – viz subkrok 2.1)
    if (strcmp(arch, "x86_64") != 0 && strcmp(arch, "i686") != 0) {
        log_warn("Non-standard arch '%s' detected; defaulting to x86_64 for compatibility.", arch);
        arch = "x86_64";
    }

    // Nastav do libsolv poolu
    const char *set_arch = arch;
    pool_setarch(pool, arch);
    log_info("Pool arch set to: %s", set_arch);  // Použij uložený string
    }
