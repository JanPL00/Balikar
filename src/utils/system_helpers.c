//
// Created by placeja5 on 11/6/25.
//

#include "includes/system_helpers.h"

/**
 * Detects and configures system architecture for the dependency resolver.
 * Uses uname to detect current system architecture and normalizes it.
 * Sets architecture in libsolv pool for proper package selection.
 * Supports x86_64 and i686, defaults to x86_64 for unknown architectures.
 * Based on architecture scoring from libsolv's solv.c.
 * @param pool Pool instance to configure
 */
void set_system_arch(Pool *pool) {
    if (!pool) {
        log_error("Invalid pool in set_system_arch.");
        return;
    }

    struct utsname un;
    const char *detected_arch = NULL;

    // Detect system architecture using uname
    if (uname(&un) == 0) {
        detected_arch = un.machine;
        log_info("Detected system arch: %s", detected_arch);
    } else {
        // uname failed - fall back to safe default
        perror("uname failed");
        log_warn("Falling back to default arch: x86_64");
        detected_arch = "x86_64";
    }

    // Normalize architecture to supported values
    // Only x86_64 and i686 are fully supported
    const char *final_arch = detected_arch;
    if (strcmp(detected_arch, "x86_64") != 0 && strcmp(detected_arch, "i686") != 0) {
        log_warn("Non-standard arch '%s' detected; defaulting to x86_64 for compatibility.", detected_arch);
        final_arch = "x86_64";
    }

    // Configure pool architecture
    // This initializes architecture scoring for proper package selection
    pool_setarch(pool, final_arch);
    log_info("Pool arch set to: %s", final_arch);
}

/**
 * Retrieves the architecture string from a libsolv pool.
 * Uses direct access to pool->lastarch for accurate architecture detection.
 * @param pool Pool instance to query
 * @return Architecture string (e.g., "x86_64", "i686"), or "unknown" if pool is NULL
 */
const char *get_pool_arch(const Pool *pool) {

    if (!pool) {
        return "unknown";
    }

    // Direct access to lastarch - correct method for getting current architecture
    if (pool->lastarch) {
        const char *arch = pool_id2str(pool, pool->lastarch);
        if (arch && strlen(arch) > 0) {
            return arch;
        }
    }

    // Fallback to default architecture
    return "x86_64";
}
