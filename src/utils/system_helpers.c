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
    const char *detected_arch = NULL;

    // Detekce architektury
    if (uname(&un) == 0) {
        detected_arch = un.machine;
        log_info("Detected system arch: %s", detected_arch);
    } else {
        perror("uname failed");
        log_warn("Falling back to default arch: x86_64");  // Ujistěte se, že tato řádka existuje
        detected_arch = "x86_64";
    }

    // Normalizace architektury
    const char *final_arch = detected_arch;
    if (strcmp(detected_arch, "x86_64") != 0 && strcmp(detected_arch, "i686") != 0) {
        log_warn("Non-standard arch '%s' detected; defaulting to x86_64 for compatibility.", detected_arch);
        final_arch = "x86_64";
    }

    // Nastavení architektury pomocí libsolv pool_setarch (správně inicializuje arch scoring)
    pool_setarch(pool, final_arch);
    log_info("Pool arch set to: %s", final_arch);
}
