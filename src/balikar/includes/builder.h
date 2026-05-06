#ifndef IMPL_BUILDER_H
#define IMPL_BUILDER_H

#include "../../common/common.h"

void set_staging_enabled(int enabled);

void set_staging_dir(const char *dir);

void set_build_tmp_dir(const char *dir);

typedef int (*build_callback)(void *data);

int run_with_chroot(const char *root, build_callback func, void *data);

int commit_element(Transaction *trans, Id p, FILE *fp, Pool *pool, int is_group, int no_package, json_t *extras);

char *find_source_path(const char *tmp_dir);

void apply_extras(json_t *extras, const char *pkg_name, const char *source_dir);

#endif