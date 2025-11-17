// Configuration parser for Balikar repositories
// Reads from /etc/balikar/repos.conf and ~/.config/balikar/repos.conf

#include "config.h"
#include <sys/stat.h>

#define SYSTEM_CONFIG "/etc/balikar/repos.conf"
#define USER_CONFIG_DIR ".config/balikar"
#define USER_CONFIG_FILE "repos.conf"

static repo_config_t *repo_list = NULL;

// Parse single line from config
static int parse_config_line(const char *line, repo_config_t **current) {
    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') return 0;
    
    // Check for section header [repo-name]
    if (line[0] == '[') {
        char name[256] = {0};
        if (sscanf(line, "[%255[^]]]", name) == 1) {
            // Create new repo entry
            repo_config_t *new_repo = calloc(1, sizeof(repo_config_t));
            if (!new_repo) return -1;
            
            new_repo->name = strdup(name);
            new_repo->priority = 5;  // default
            new_repo->enabled = 1;    // default enabled
            
            // Add to list
            if (!repo_list) {
                repo_list = new_repo;
            } else {
                repo_config_t *tail = repo_list;
                while (tail->next) tail = tail->next;
                tail->next = new_repo;
            }
            *current = new_repo;
            return 1;
        }
    }
    
    // Parse key=value pairs
    if (*current) {
        char key[64] = {0}, value[512] = {0};
        if (sscanf(line, " %63[^= ] = %511[^\n]", key, value) == 2) {
            // Trim whitespace from value
            char *v = value;
            while (*v == ' ' || *v == '\t') v++;
            char *end = v + strlen(v) - 1;
            while (end > v && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';
            
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

// Load config from file
static int load_config_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    
    char line[1024];
    repo_config_t *current = NULL;
    
    while (fgets(line, sizeof(line), fp)) {
        parse_config_line(line, &current);
    }
    
    fclose(fp);
    return 1;
}

// Initialize repo configuration
void init_repo_config(void) {
    // Skip if already initialized
    if (repo_list) return;
    
    // Load user config first (has priority)
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
    
    // If no config found, use default
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

// Get list of enabled repositories (sorted by priority)
repo_config_t *get_enabled_repos(void) {
    // TODO: Sort by priority and filter enabled
    return repo_list;
}

// Get primary repository (highest priority enabled)
const char *get_primary_repo_url(void) {
    if (!repo_list) {
        init_repo_config();
    }
    
    repo_config_t *primary = NULL;
    int max_priority = -1;
    
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (r->enabled && r->priority > max_priority) {
            primary = r;
            max_priority = r->priority;
        }
    }
    
    return primary ? primary->url : "./repo";
}

// Remove repository
int remove_repository(const char *name) {
    repo_config_t *prev = NULL;
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (strcmp(r->name, name) == 0) {
            if (prev) {
                prev->next = r->next;
            } else {
                repo_list = r->next;
            }
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

// Enable/disable repository
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

// Add new repository
int add_repository(const char *name, const char *url, int priority) {
    // Check if already exists
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (strcmp(r->name, name) == 0) {
            log_warn("Repository '%s' already exists", name);
            return -1;
        }
    }
    
    repo_config_t *new_repo = calloc(1, sizeof(repo_config_t));
    new_repo->name = strdup(name);
    new_repo->url = strdup(url);
    new_repo->priority = priority;
    new_repo->enabled = 1;
    
    // Add to list
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

// Save configuration to user config file
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
    
    fprintf(fp, "# Balikar Repository Configuration\n");
    fprintf(fp, "# Auto-generated file\n\n");
    
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

// List all repositories
void list_repositories(void) {
    if (!repo_list) {
        init_repo_config();
    }
    
    printf("Configured repositories:\n");
    printf("%-20s %-10s %-10s %s\n", "NAME", "PRIORITY", "ENABLED", "URL");
    printf("------------------------------------------------------------------------\n");
    
    for (repo_config_t *r = repo_list; r; r = r->next) {
        printf("%-20s %-10d %-10s %s\n", 
               r->name, r->priority, 
               r->enabled ? "yes" : "no", 
               r->url);
    }
}

// Cleanup
void free_repo_config(void) {
    while (repo_list) {
        repo_config_t *next = repo_list->next;
        free(repo_list->name);
        free(repo_list->url);
        free(repo_list);
        repo_list = next;
    }
}
