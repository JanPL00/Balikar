//
// Created by placeja5 on 11/2/25.
//

#ifndef PROJ6_BALIKAR_DOWNLOADER_H
#define PROJ6_BALIKAR_DOWNLOADER_H

#include "../../common/common.h"
#include "../../utils/includes/cli_helpers.h"
#include "../../utils/includes/string_helpers.h"
#include "parser.h"

// Safe cache dir allocator macro (caller frees result of CACHE_DIR)
#define CACHE_DIR (get_cache_dir())

// Get the cache directory path.
static inline char *get_cache_dir(void) {
	return strdup("/var/cache/balikar/downloads");
}

// Download a package and return a file pointer to the downloaded archive.
FILE *download_package(Solvable *s, const char *loc, Pool *pool, Repodata *data, const char *repo_path);

// Download extra files (patches, etc.) for a package.
void download_extras(json_t *extras, const char *pkg_name, const char *version, const char *repo_base_url, const char *tmp_dir);

#endif