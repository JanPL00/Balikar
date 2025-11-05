//
// Created by placeja5 on 11/5/25.
//

#include "includes/string_helpers.h"

#include <solv/pool.h>      // Pro pool_id2str, pool_id2evr
#include <solv/solvable.h>  // Pro Solvable struktura
#include <solv/evr.h>       // Pro EVR handling (version)
#include <string.h>         // Pro strlen, strcpy
#include <stdlib.h>         // Pro malloc, free
#include <stdio.h>          // Pro snprintf

char *solvable_to_string(Pool *pool, Id p) {
    if (!pool || p <= 0) {
        log_error("Invalid pool or solvable ID in solvable_to_string.");
        return NULL;
    }

    Solvable *s = pool_id2solvable(pool, p);
    if (!s) {
        log_error("Solvable not found for ID %d.", p);
        return NULL;
    }

    const char *name = pool_id2str(pool, s->name);
    const char *evr = pool_id2evr(pool, s->evr);
    const char *arch = pool_id2str(pool, s->arch);

    if (!name || !evr || !arch) {
        log_error("Missing name, evr, or arch for solvable ID %d.", p);
        return NULL;
    }

    // Alokuj dostatek paměti: name + '-' + evr + '.' + arch + \0
    size_t len = strlen(name) + strlen(evr) + strlen(arch) + 3;
    char *str = malloc(len);
    if (!str) {
        log_error("Memory allocation failed in solvable_to_string.");
        return NULL;
    }

    snprintf(str, len, "%s-%s.%s", name, evr, arch);
    return str;
}

char *dep_to_string(Pool *pool, Id dep) {
    if (!pool || dep <= 0) {
        log_error("Invalid pool or dep ID in dep_to_string.");
        return NULL;
    }

    const char *dep_str = pool_dep2str(pool, dep);
    if (!dep_str) {
        log_error("Dependency not found for ID %d.", dep);
        return NULL;
    }

    // Alokuj kopii (pool_dep2str vrací interní string, takže duplikuj)
    char *str = strdup(dep_str);
    if (!str) {
        log_error("Memory allocation failed in dep_to_string.");
        return NULL;
    }

    return str;
}