#include "includes/system_helpers.h"

/**
 * Zjistí a nakonfiguruje architekturu systému pro resolver závislostí.
 *
 * Používá uname pro detekci aktuální architektury a normalizuje ji.
 * Nastaví architekturu v libsolv pool pro správný výběr balíků.
 * Podporuje x86_64 a i686, pro neznámé architektury použije x86_64.
 *
 * @param pool Instance pool ke konfiguraci
 */
void set_system_arch(Pool *pool) {
    if (!pool) {
        log_error("Invalid pool in set_system_arch.");
        return;
    }

    struct utsname un;
    const char *detected_arch = NULL;

    // Zjisti architekturu systému pomocí uname
    if (uname(&un) == 0) {
        detected_arch = un.machine;
        log_info("Detected system arch: %s", detected_arch);
    } else {
        // uname selhalo – použij bezpečnou výchozí hodnotu
        perror("uname failed");
        log_warn("Falling back to default arch: x86_64");
        detected_arch = "x86_64";
    }

    // Normalizuj architekturu na podporované hodnoty
    // Plně jsou podporovány pouze x86_64 a i686
    const char *final_arch = detected_arch;
    if (strcmp(detected_arch, "x86_64") != 0 && strcmp(detected_arch, "i686") != 0) {
        log_warn("Non-standard arch '%s' detected; defaulting to x86_64 for compatibility.", detected_arch);
        final_arch = "x86_64";
    }

    // Konfiguruj architekturu poolu
    // Inicializuje hodnocení architektury pro správný výběr balíků
    pool_setarch(pool, final_arch);
    log_info("Pool arch set to: %s", final_arch);
}

/**
 * Získá řetězec architektury z libsolv pool.
 *
 * Používá přímý přístup k pool->lastarch pro přesnou detekci architektury.
 *
 * @param pool Instance pool k dotazování
 * @return Řetězec architektury (např. "x86_64", "i686") nebo "unknown" pokud je pool NULL
 */
const char *get_pool_arch(const Pool *pool) {
    if (!pool) return "unknown";

    const char *best = NULL;
    for (Id id = 1; id < pool->lastarch; id++) {
        if (pool->id2arch && pool->id2arch[id]) {
            unsigned int color = pool->id2arch[id] >> 16;
            if (color == 1) best = pool_id2str(pool, id);
        }
    }
    return best ? best : "x86_64";
}
