//
// Created by placeja5 on 11/17/25.
//

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

// Initialize repo configuration
void init_repo_config(void);

// Get primary repository (highest priority enabled)
const char *get_primary_repo_url(void);

// Get list of enabled repositories
repo_config_t *get_enabled_repos(void);

// Add new repository
int add_repository(const char *name, const char *url, int priority);

// Remove repository
int remove_repository(const char *name);

// Enable/disable repository
int enable_repository(const char *name, int enabled);

// Save configuration to user config file
int save_repo_config(void);

// List all repositories
void list_repositories(void);

// Cleanup
void free_repo_config(void);

#endif //BALIKAR_CONFIG_H
