//
// Created by placeja5 on 11/2/25.
//

#ifndef PROJ6_BALIKAR_DOWNLOADER_H
#define PROJ6_BALIKAR_DOWNLOADER_H

#include "../../common/common.h"


// Download a package and return a file pointer to the downloaded archive.
FILE *download_package(Solvable *s, const char *loc, Pool *pool, json_t *source_array, const char *repo_path);

// Download extra files (patches, etc.) for a package.
void download_extras(json_t *extras, const char *pkg_name, const char *version, const char *repo_base_url, const char *tmp_dir);

// Download a package to a specific directory (for download command).
// Creates repository structure: output_dir/<letter>/<pkg_name>/<pkg_name>-<version>/
// Returns 0 on success, -1 on error.
int download_package_to_directory(Solvable *s, const char *loc, Pool *pool, json_t *source_array, 
                                  const char *repo_path, const char *output_dir);

// Download a file from URL to specified path (simple direct download).
// Used by download command for straightforward URL->file downloads.
// Returns 0 on success, -1 on error.
int download_url_to_file(const char *url, const char *output_path);

#endif