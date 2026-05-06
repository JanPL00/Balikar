#ifndef PROJ6_BALIKAR_DOWNLOADER_H
#define PROJ6_BALIKAR_DOWNLOADER_H

#include "../../common/common.h"

FILE *download_package(Solvable *s, const char *loc, Pool *pool, json_t *source_array, const char *repo_path);

void download_extras(json_t *extras, const char *pkg_name, const char *version, const char *repo_base_url, const char *tmp_dir);

int download_package_to_directory(Solvable *s, const char *loc, Pool *pool, json_t *source_array,
                                  const char *repo_path, const char *output_dir);

int download_url_to_file(const char *url, const char *output_path);

#endif