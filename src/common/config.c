// Configuration parser for Balikar repositories
// Reads from /etc/balikar/repos.conf and ~/.config/balikar/repos.conf
// Manages repository list with priorities and enable/disable states

#include "config.h"

#define SYSTEM_CONFIG "/etc/balikar/repos.conf"
#define USER_CONFIG_DIR ".config/balikar"
#define USER_CONFIG_FILE "repos.conf"

// Global linked list of repository configurations
static repo_config_t *repo_list = NULL;

/**
 * Parses a single line from repository configuration file.
 * Handles section headers [repo-name] and key=value pairs.
 * @param line Line to parse
 * @param current Pointer to current repository being configured
 * @return 1 if line was parsed successfully, 0 if skipped, -1 on error
 */
static int parse_config_line(const char *line, repo_config_t **current) {
    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') return 0;
    
    // Check for section header [repo-name]
    if (line[0] == '[') {
        char name[256] = {0};
        if (sscanf(line, "[%255[^]]]", name) == 1) {
            // Create new repository entry with defaults
            repo_config_t *new_repo = calloc(1, sizeof(repo_config_t));
            if (!new_repo) return -1;
            
            new_repo->name = strdup(name);
            new_repo->priority = 5;  // Default priority
            new_repo->enabled = 1;    // Enabled by default
            
            // Add to linked list
            if (!repo_list) {
                repo_list = new_repo;
            } else {
                // Append to end of list
                repo_config_t *tail = repo_list;
                while (tail->next) tail = tail->next;
                tail->next = new_repo;
            }
            *current = new_repo;
            return 1;
        }
    }
    
    // Parse key=value pairs for current repository section
    if (*current) {
        char key[64] = {0}, value[512] = {0};
        if (sscanf(line, " %63[^= ] = %511[^\n]", key, value) == 2) {
            // Trim leading whitespace from value
            char *v = value;
            while (*v == ' ' || *v == '\t') v++;
            
            // Trim trailing whitespace from value
            char *end = v + strlen(v) - 1;
            while (end > v && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';
            
            // Set configuration field based on key
            if (strcmp(key, "url") == 0) {
                (*current)->url = strdup(v);
            } else if (strcmp(key, "priority") == 0) {
                (*current)->priority = atoi(v);
            } else if (strcmp(key, "enabled") == 0) {
                (*current)->enabled = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
            }
            return 1;
        }
    }
    
    return 0;
}

/**
 * Loads repository configuration from a file.
 * Parses INI-style format with [section] headers and key=value pairs.
 * @param path Path to configuration file
 * @return 1 if file was loaded successfully, 0 if file doesn't exist
 */
static int load_config_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;  // File doesn't exist or can't be opened
    
    char line[1024];
    repo_config_t *current = NULL;
    
    // Parse file line by line
    while (fgets(line, sizeof(line), fp)) {
        parse_config_line(line, &current);
    }
    
    fclose(fp);
    return 1;
}

/**
 * Initializes repository configuration from config files.
 * Load order (first found wins):
 * 1. User config: ~/.config/balikar/repos.conf
 * 2. System config: /etc/balikar/repos.conf
 * 3. Default: ./repo with priority 10
 * Only loads once; subsequent calls are no-op.
 */
void init_repo_config(void) {
    // Skip if already initialized
    if (repo_list) return;
    
    // Load user config first (has highest priority)
    const char *home = getenv("HOME");
    if (home) {
        char user_config[512];
        snprintf(user_config, sizeof(user_config), "%s/%s/%s", 
                 home, USER_CONFIG_DIR, USER_CONFIG_FILE);
        if (load_config_file(user_config)) {
            log_debug(1, "Loaded user config from: %s", user_config);
            return;  // User config loaded, skip system config
        }
    }
    
    // Load system config as fallback
    if (load_config_file(SYSTEM_CONFIG)) {
        log_debug(1, "Loaded system config from: %s", SYSTEM_CONFIG);
        return;
    }
    
    // If no config found, create default repository
    if (!repo_list) {
        repo_config_t *default_repo = calloc(1, sizeof(repo_config_t));
        default_repo->name = strdup("default");
        default_repo->url = strdup("./repo");
        default_repo->priority = 10;
        default_repo->enabled = 1;
        repo_list = default_repo;
        
        log_debug(1, "Using default repository: ./repo");
    }
}

/**
 * Gets list of enabled repositories.
 * TODO: Implement sorting by priority and filtering
 * @return Linked list of all configured repositories
 */
repo_config_t *get_enabled_repos(void) {
    // TODO: Sort by priority and filter enabled
    return repo_list;
}

/**
 * Gets URL of primary repository (highest priority enabled repo).
 * Automatically initializes configuration if not already loaded.
 * @return URL of primary repository, or "./repo" as fallback
 */
const char *get_primary_repo_url(void) {
    if (!repo_list) {
        init_repo_config();
    }
    
    repo_config_t *primary = NULL;
    int max_priority = -1;
    
    // Find enabled repository with highest priority
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (r->enabled && r->priority > max_priority) {
            primary = r;
            max_priority = r->priority;
        }
    }
    
    return primary ? primary->url : "./repo";
}

/**
 * Removes a repository from configuration.
 * Frees all associated memory and removes from linked list.
 * @param name Name of repository to remove
 * @return 0 on success, -1 if repository not found
 */
int remove_repository(const char *name) {
    repo_config_t *prev = NULL;
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (strcmp(r->name, name) == 0) {
            // Remove from linked list
            if (prev) {
                prev->next = r->next;
            } else {
                repo_list = r->next;
            }
            // Free memory
            free(r->name);
            free(r->url);
            free(r);
            log_info("Removed repository: %s", name);
            return 0;
        }
        prev = r;
    }
    log_error("Repository '%s' not found", name);
    return -1;
}

/**
 * Enables or disables a repository.
 * Disabled repositories are ignored during package resolution.
 * @param name Name of repository to modify
 * @param enabled 1 to enable, 0 to disable
 * @return 0 on success, -1 if repository not found
 */
int enable_repository(const char *name, int enabled) {
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (strcmp(r->name, name) == 0) {
            r->enabled = enabled;
            log_info("Repository '%s' %s", name, enabled ? "enabled" : "disabled");
            return 0;
        }
    }
    log_error("Repository '%s' not found", name);
    return -1;
}

/**
 * Adds a new repository to configuration.
 * Repository is enabled by default. Fails if name already exists.
 * @param name Unique name for repository
 * @param url Repository URL or local path
 * @param priority Priority level (higher = preferred)
 * @return 0 on success, -1 if repository already exists
 */
int add_repository(const char *name, const char *url, int priority) {
    // Check if already exists
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (strcmp(r->name, name) == 0) {
            log_warn("Repository '%s' already exists", name);
            return -1;
        }
    }
    
    // Create new repository entry
    repo_config_t *new_repo = calloc(1, sizeof(repo_config_t));
    new_repo->name = strdup(name);
    new_repo->url = strdup(url);
    new_repo->priority = priority;
    new_repo->enabled = 1;  // Enabled by default
    
    // Add to end of linked list
    if (!repo_list) {
        repo_list = new_repo;
    } else {
        repo_config_t *tail = repo_list;
        while (tail->next) tail = tail->next;
        tail->next = new_repo;
    }
    
    log_info("Added repository: %s (%s)", name, url);
    return 0;
}

/**
 * Saves current repository configuration to user config file.
 * Creates ~/.config/balikar/ directory if it doesn't exist.
 * Writes INI-style format: [section] headers with key=value pairs.
 * @return 0 on success, -1 on error
 */
int save_repo_config(void) {
    const char *home = getenv("HOME");
    if (!home) return -1;
    
    char config_dir[512], config_file[512];
    snprintf(config_dir, sizeof(config_dir), "%s/%s", home, USER_CONFIG_DIR);
    snprintf(config_file, sizeof(config_file), "%s/%s", config_dir, USER_CONFIG_FILE);
    
    // Create directory if needed
    mkdir(config_dir, 0755);
    
    FILE *fp = fopen(config_file, "w");
    if (!fp) {
        log_error("Cannot write config to %s", config_file);
        return -1;
    }
    
    // Write header
    fprintf(fp, "# Balikar Repository Configuration\n");
    fprintf(fp, "# Auto-generated file\n\n");
    
    // Write each repository as a section
    for (repo_config_t *r = repo_list; r; r = r->next) {
        fprintf(fp, "[%s]\n", r->name);
        fprintf(fp, "url = %s\n", r->url);
        fprintf(fp, "priority = %d\n", r->priority);
        fprintf(fp, "enabled = %s\n\n", r->enabled ? "true" : "false");
    }
    
    fclose(fp);
    log_info("Configuration saved to %s", config_file);
    return 0;
}

/**
 * Lists all configured repositories in tabular format.
 * Shows name, priority, enabled status, and URL.
 * Initializes configuration if not already loaded.
 */
void list_repositories(void) {
    if (!repo_list) {
        init_repo_config();
    }
    
    printf("Configured repositories:\n");
    printf("%-20s %-10s %-10s %s\n", "NAME", "PRIORITY", "ENABLED", "URL");
    printf("------------------------------------------------------------------------\n");
    
    // Print each repository
    for (repo_config_t *r = repo_list; r; r = r->next) {
        printf("%-20s %-10d %-10s %s\n", 
               r->name, r->priority, 
               r->enabled ? "yes" : "no", 
               r->url);
    }
}

/**
 * Frees all memory used by repository configuration.
 * Walks through linked list and frees each node and its strings.
 * Should be called before program exit to prevent memory leaks.
 */
void free_repo_config(void) {
    while (repo_list) {
        repo_config_t *next = repo_list->next;
        free(repo_list->name);
        free(repo_list->url);
        free(repo_list);
        repo_list = next;
    }
}
