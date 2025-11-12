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

// Nové funkce
// Pro parsing depends array do Solvable requires (pomocí SOLVABLE_REQUIRES z constantids)
void parse_depends_to_solvable(Solvable *s, json_t *depends, Pool *pool);  // Přidá do s->requires pomocí solvable_add_deparray

// Pro lookup custom metadat z Repodata (např. pro solver.c rekurzi)
json_t* get_custom_mkdeps(Repodata *data, Id p, Pool *pool);  // Načte "makedeps" string a parse do json_array
const char* get_custom_source(Repodata *data, Id p, Pool *pool);  // Načte "download_url"
json_t* get_custom_extras(Repodata *data, Id p, Pool *pool);  // Načte "extras" string a parse do json_array
int get_custom_is_group(Repodata *data, Id p, Pool *pool);  // Načte "is_group" jako bool (z string "true"/"false")
int get_custom_no_package(Repodata *data, Id p, Pool *pool);  // Podobně pro "no_package"

// Pro internalizaci repo po načtení (z pool manpage: nutné pro lookup funkce)
void internalize_repo(Repo *repo);  // Volá repo_internalize(repo) pro indexaci whatprovides

Id add_package_tar_bz2(const char *tar_path, Pool *pool, Repo *repo);  // Nové: Přidání tar.bz2 balíčku

#endif //IMPL_REPO_PARSER_H