//
// Created by placeja5 on 11/14/25.
//

#ifndef IMPL_BUILDER_H
#define IMPL_BUILDER_H

#include "../../common/common.h"
#include "parser.h"
#include "downloader.h"
#include "solver.h"
#include "../../utils/includes/cli_helpers.h"

// Public builder API

// Set staging mode (DESTDIR). 1 = enabled, 0 = disabled.
void set_staging_enabled(int enabled);

// Set custom staging path (DESTDIR). If NULL, uses STAGING_DIR from common.h.
void set_staging_dir(const char *dir);

// Set custom tmp directory for build (extraction, patches). If NULL, uses /tmp.
void set_build_tmp_dir(const char *dir);

// Build isolation in chroot (for security). If root != NULL, fork + chroot before build.
// Callback build_func receives build_data as argument.
typedef int (*build_callback)(void *data);

int run_with_chroot(const char *root, build_callback func, void *data);

// Main function for building one transaction element.
// Returns 0 on success, non-zero on error.
int commit_element(Transaction *trans, Repodata *data, Id p, FILE *fp, Pool *pool);

// Find first subdirectory (source dir) after tar archive extraction.
// Caller must free the returned malloc'd path.
char *find_source_path(const char *tmp_dir);

// Apply patch files from extras cache (cache/extras/<pkg_name>). Ignores errors.
void apply_extras(json_t *extras, const char *pkg_name, const char *source_dir);

#endif //IMPL_BUILDER_H