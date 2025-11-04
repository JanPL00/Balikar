//
// Created by placeja5 on 11/4/25.
//

#ifndef IMPL_REPO_PARSER_H
#define IMPL_REPO_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <jansson.h>

json_t* load_json(const char* filepath);

char* generate_package_key(json_t* index);

void collect_packages(const char* root_path, json_t* packages);

int generate_repodata(const char* repo_path);

#endif //IMPL_REPO_PARSER_H