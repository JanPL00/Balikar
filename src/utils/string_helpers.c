#include "includes/string_helpers.h"

/**
 * Převede solvable na čitelný řetězec.
 *
 * Formát: "název-verze.architektura" (např. "glibc-2.35.x86_64").
 * Volající musí vrácený řetězec uvolnit.
 *
 * @param pool Pool obsahující solvable
 * @param p ID solvable
 * @return Alokovaný řetězec nebo NULL při chybě
 */
char *solvable_to_string(Pool *pool, Id p) {
    if (!pool || p <= 0) {
        log_error("Invalid pool or solvable ID in solvable_to_string.");
        return NULL;
    }

    // Načti solvable z poolu
    Solvable *s = pool_id2solvable(pool, p);
    if (!s) {
        log_error("Solvable not found for ID %d.", p);
        return NULL;
    }

    // Extrahuj komponenty balíku
    const char *name = pool_id2str(pool, s->name);  // Název balíku
    const char *evr = pool_id2evr(pool, s->evr);    // Epocha:Verze-Release
    const char *arch = pool_id2str(pool, s->arch);  // Architektura

    if (!name || !evr || !arch) {
        log_error("Missing name, evr, or arch for solvable ID %d.", p);
        return NULL;
    }

    // Alokuj paměť pro formátovaný řetězec: "název-evr.arch\0"
    size_t len = strlen(name) + strlen(evr) + strlen(arch) + 3;
    char *str = malloc(len);
    if (!str) {
        log_error("Memory allocation failed in solvable_to_string.");
        return NULL;
    }

    // Formát: název-verze.architektura
    snprintf(str, len, "%s-%s.%s", name, evr, arch);
    return str;
}

/**
 * Převede ID závislosti na čitelný řetězec.
 *
 * Formátuje závislost s názvem, verزیávislostmi a operátory.
 * Příklady: "glibc >= 2.35", "bash", "libssl.so.1.1".
 * Volající musí vrácený řetězec uvolnit.
 *
 * @param pool Pool obsahující závislost
 * @param dep ID závislosti
 * @return Alokovaný řetězec nebo NULL při chybě
 */
char *dep_to_string(Pool *pool, Id dep) {
    if (!pool || dep <= 0) {
        log_error("Invalid pool or dep ID in dep_to_string.");
        return NULL;
    }

    // Načti řetězec závislosti z poolu
    // pool_dep2str vrací interní řetězec, musí být duplikován
    const char *dep_str = pool_dep2str(pool, dep);
    if (!dep_str) {
        log_error("Dependency not found for ID %d.", dep);
        return NULL;
    }

    // Duplikuj řetězec pro volajícího (pool_dep2str vrací interní ukazatel)
    char *str = strdup(dep_str);
    if (!str) {
        log_error("Memory allocation failed in dep_to_string.");
        return NULL;
    }

    return str;
}