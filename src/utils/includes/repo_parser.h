//
// Created by placeja5 on 11/4/25.
//

#ifndef IMPL_REPO_PARSER_H
#define IMPL_REPO_PARSER_H


#include "../../common/common.h"


json_t* load_json(const char* filepath);

char* generate_package_key(json_t* index);

void collect_packages(const char* root_path, json_t* packages, Pool *pool, Repo *repo);  // Přidej pool a repo parametry

int generate_repodata(const char* repo_path, Pool *pool, Repo *repo);

#endif //IMPL_REPO_PARSER_H