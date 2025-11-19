//
// Created by placeja5 on 11/6/25.
//

#include "includes/system_helpers.h"
#include <solv/pool.h>

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
