//
// Created by placeja5 on 11/4/25.
//

#ifndef IMPL_REPO_PARSER_H
#define IMPL_REPO_PARSER_H


#include "../../common/common.h"
#include <curl/curl.h>

// Load JSON from a file.
json_t* load_json(const char* filepath);

// Load a repository into the pool.
Repo *load_repository(Pool *pool, const char *repo_path);

// Generate default metadata for a package.
json_t* generate_default_metadata(const char* basename);

// Generate a package key from JSON index.
char* generate_package_key(json_t* index);

// Collect packages from directory and add them to the repository.
void collect_packages(const char* root_path, json_t* packages, Pool *pool, Repo *repo);

// Generate repository data file.
int generate_repodata(const char* repo_path);

// Parse depends array into Solvable requires (using SOLVABLE_REQUIRES).
void parse_depends_to_solvable(Solvable *s, json_t *depends, Pool *pool);

// Lookup custom makedeps metadata from Repodata.

json_t* get_custom_mkdeps(Repodata *data, Id p, Pool *pool);

// Lookup custom download_url metadata from Repodata.
const char* get_custom_source(Repodata *data, Id p, Pool *pool);

// Lookup custom source array metadata from Repodata.
json_t* get_custom_source_array(Repodata *data, Id p, Pool *pool);

// Lookup custom extras metadata from Repodata.
json_t* get_custom_extras(Repodata *data, Id p, Pool *pool);

// Lookup custom is_group flag from Repodata.
int get_custom_is_group(Repodata *data, Id p, Pool *pool);

// Lookup custom no_package flag from Repodata.
int get_custom_no_package(Repodata *data, Id p, Pool *pool);

// Internalize repository after loading (necessary for lookup functions).
void internalize_repo(Repo *repo);

// Add a tar.bz2 package to the repository.
Id add_package_tar_bz2(const char *tar_path, Pool *pool, Repo *repo);

// Update repository metadata (download for remote, generate for local).
int update_repository_metadata(const char *repo_path);

#endif