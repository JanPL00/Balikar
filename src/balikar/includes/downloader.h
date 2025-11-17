//
// Created by placeja5 on 11/2/25.
//

#ifndef PROJ6_BALIKAR_DOWNLOADER_H
#define PROJ6_BALIKAR_DOWNLOADER_H

#include "../../common/common.h"
#include "../../utils/includes/cli_helpers.h"
#include "../../utils/includes/string_helpers.h"  // Pro solvable_to_string pokud potřeba
#include "parser.h"  // Pro get_custom_source, get_custom_extras


// Safe cache dir allocator macro (caller frees result of CACHE_DIR)
#define CACHE_DIR (get_cache_dir())

static inline char *get_cache_dir(void) {
	return strdup("/var/cache/balikar/downloads/");
}

// Hlavní funkce
FILE *download_package(Solvable *s, const char *loc, Pool *pool, Repodata *data);
void download_extras(json_t *extras, const char *pkg_name, const char *tmp_dir);

#endif //PROJ6_BALIKAR_DOWNLOADER_H