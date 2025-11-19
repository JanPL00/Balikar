//
// Created by placeja5 on 11/17/25.
//

#ifndef BALIKAR_CONFIG_H
#define BALIKAR_CONFIG_H

#include "common.h"

// Repository configuration structure - linked list node
typedef struct repo_config {
    char *name;                  // Repository name (unique identifier)
    char *url;                   // Repository URL or local path
    int priority;                // Priority level (higher = preferred)
    int enabled;                 // 1 if enabled, 0 if disabled
    struct repo_config *next;    // Next repository in linked list
} repo_config_t;

// Public configuration API

// Initialize repository configuration from config files.
// Load order: user config, system config, or default (./repo)
void init_repo_config(void);

// Get URL of primary repository (highest priority enabled).
// Returns URL string, or "./repo" as fallback
const char *get_primary_repo_url(void);

// Get list of enabled repositories.
// Returns linked list of all configured repositories
repo_config_t *get_enabled_repos(void);

// Add new repository to configuration.
// name: Unique name for repository
// url: Repository URL or local path
// priority: Priority level (higher = preferred)
// Returns 0 on success, -1 if name already exists
int add_repository(const char *name, const char *url, int priority);

// Remove repository from configuration.
// name: Name of repository to remove
// Returns 0 on success, -1 if not found
int remove_repository(const char *name);

// Enable or disable a repository.
// name: Name of repository to modify
// enabled: 1 to enable, 0 to disable
// Returns 0 on success, -1 if not found
int enable_repository(const char *name, int enabled);

// Save current configuration to user config file.
// Creates ~/.config/balikar/repos.conf
// Returns 0 on success, -1 on error
int save_repo_config(void);

// List all configured repositories in tabular format.
// Shows name, priority, enabled status, and URL
void list_repositories(void);

// Free all memory used by repository configuration.
// Should be called before program exit
void free_repo_config(void);

#endif //BALIKAR_CONFIG_H
