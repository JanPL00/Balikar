#ifndef BALIKAR_CONFIG_H
#define BALIKAR_CONFIG_H

#include "common.h"

typedef struct repo_config {
    char *name;
    char *url;
    int priority;
    int enabled;
    struct repo_config *next;
} repo_config_t;

void init_repo_config(void);

const char *get_primary_repo_url(void);

repo_config_t *get_enabled_repos(void);

int add_repository(const char *name, const char *url, int priority);

int remove_repository(const char *name);

int enable_repository(const char *name, int enabled);

int save_repo_config(void);

void list_repositories(void);

void free_repo_config(void);

#endif
