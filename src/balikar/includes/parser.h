#ifndef IMPL_REPO_PARSER_H
#define IMPL_REPO_PARSER_H

#include "../../common/common.h"

json_t* load_json(const char* filepath);

Repo *load_repository(Pool *pool, const char *repo_path);

json_t* generate_default_metadata(const char* basename);

char* generate_package_key(json_t* index);

void collect_packages(const char* root_path, json_t* packages, Pool *pool, Repo *repo);

int generate_repodata(const char* repo_path);

void parse_depends_to_solvable(Solvable *s, json_t *depends, Pool *pool);

json_t* get_custom_mkdeps(Repodata *data, Id p, Pool *pool);

const char* get_custom_source(Repodata *data, Id p, Pool *pool);

json_t* get_custom_source_array(Repodata *data, Id p, Pool *pool);

json_t* get_custom_extras(Repodata *data, Id p, Pool *pool);

int get_custom_is_group(Repodata *data, Id p, Pool *pool);

int get_custom_no_package(Repodata *data, Id p, Pool *pool);

void internalize_repo(Repo *repo);

Id add_package_tar_bz2(const char *tar_path, Pool *pool, Repo *repo);

int update_repository_metadata(const char *repo_path);

#endif