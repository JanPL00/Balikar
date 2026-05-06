// Parser konfigurace repozitářů Balikar
// Čte z /etc/balikar/repos.conf a ~/.config/balikar/repos.conf
// Spravuje seznam repozitářů s prioritami a stavy povolení/zakázání

#include "config.h"

#define SYSTEM_CONFIG "/etc/balikar/repos.conf"
#define USER_CONFIG_DIR ".config/balikar"
#define USER_CONFIG_FILE "repos.conf"

// Globální spojový seznam konfigurací repozitářů
static repo_config_t *repo_list = NULL;

/**
 * Parsuje jeden řádek z konfiguračního souboru repozitáře.
 *
 * Zpracovává záhlaví sekcí [název-repo] a páry klíč=hodnota.
 *
 * @param line Řádek k parsování
 * @param current Ukazatel na aktuálně konfigurovaný repozitář
 * @return 1 pokud byl řádek úspěšně parsován, 0 pokud přeskočen, -1 při chybě
 */
static int parse_config_line(const char *line, repo_config_t **current) {
    // Přeskoč komentáře a prázdné řádky
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') return 0;
    
    // Zkontroluj záhlaví sekce [název-repo]
    if (line[0] == '[') {
        char name[256] = {0};
        if (sscanf(line, "[%255[^]]]", name) == 1) {
            // Vytvoř nový záznam repozitáře s výchozími hodnotami
            repo_config_t *new_repo = calloc(1, sizeof(repo_config_t));
            if (!new_repo) return -1;
            
            new_repo->name = strdup(name);
            new_repo->priority = 5;  // Výchozí priorita
            new_repo->enabled = 1;    // Ve výchozím stavu povoleno
            
            // Přidej do spojového seznamu
            if (!repo_list) {
                repo_list = new_repo;
            } else {
                // Přidej na konec seznamu
                repo_config_t *tail = repo_list;
                while (tail->next) tail = tail->next;
                tail->next = new_repo;
            }
            *current = new_repo;
            return 1;
        }
    }
    
    // Parsuj páry klíč=hodnota pro aktuální sekci repozitáře
    if (*current) {
        char key[64] = {0}, value[512] = {0};
        if (sscanf(line, " %63[^= ] = %511[^\n]", key, value) == 2) {
            // Ořízni úvodní bílé znaky hodnoty
            char *v = value;
            while (*v == ' ' || *v == '\t') v++;
            
            // Ořízni koncové bílé znaky hodnoty
            char *end = v + strlen(v) - 1;
            while (end > v && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';
            
            // Nastav konfigurační pole podle klíče
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
 * Načte konfiguraci repozitáře ze souboru.
 *
 * Parsuje INI formát se záhlavími [sekce] a páry klíč=hodnota.
 *
 * @param path Cesta ke konfiguračnímu souboru
 * @return 1 pokud byl soubor úspěšně načten, 0 pokud soubor neexistuje
 */
static int load_config_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;  // Soubor neexistuje nebo nelze otevřít
    
    char line[1024];
    repo_config_t *current = NULL;
    
    // Parsuj soubor řádek po řádku
    while (fgets(line, sizeof(line), fp)) {
        parse_config_line(line, &current);
    }
    
    fclose(fp);
    return 1;
}

/**
 * Inicializuje konfiguraci repozitáře z konfiguračních souborů.
 *
 * Pořadí načítání (první nalezený vyhraje):
 * 1. Uživatelský config: ~/.config/balikar/repos.conf
 * 2. Systémový config: /etc/balikar/repos.conf
 * 3. Výchozí: ./repo s prioritou 10
 *
 * Načítá pouze jednou; opakované volání je no-op.
 */
void init_repo_config(void) {
    // Přeskoč pokud již bylo inicializováno
    if (repo_list) return;
    
    // Nejprve načti uživatelský config (má nejvyšší prioritu)
    const char *home = getenv("HOME");
    if (home) {
        char user_config[512];
        snprintf(user_config, sizeof(user_config), "%s/%s/%s", 
                 home, USER_CONFIG_DIR, USER_CONFIG_FILE);
        if (load_config_file(user_config)) {
            log_debug(1, "Loaded user config from: %s", user_config);
            return;  // Uživatelský config načten, přeskoč systémový
        }
    }
    
    // Načti systémový config jako záložní
    if (load_config_file(SYSTEM_CONFIG)) {
        log_debug(1, "Loaded system config from: %s", SYSTEM_CONFIG);
        return;
    }
    
    // Pokud nebyl nalezen žádný config, vytvoř výchozí repozitář
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
 * Vrátí seznam povolených repozitářů.
 *
 * TODO: Implementovat řazení podle priority a filtrování
 *
 * @return Spojový seznam všech nakonfigurovaných repozitářů
 */
repo_config_t *get_enabled_repos(void) {
    // TODO: Seřadit podle priority a filtrovat povolené
    return repo_list;
}

/**
 * Vrátí URL primárního repozitáře (povolený s nejvyšší prioritou).
 *
 * Automaticky inicializuje konfiguraci pokud ještě nebyla načtena.
 *
 * @return URL primárního repozitáře nebo "./repo" jako záložní hodnota
 */
const char *get_primary_repo_url(void) {
    if (!repo_list) {
        init_repo_config();
    }
    
    repo_config_t *primary = NULL;
    int max_priority = -1;
    
    // Najdi povolený repozitář s nejvyšší prioritou
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (r->enabled && r->priority > max_priority) {
            primary = r;
            max_priority = r->priority;
        }
    }
    
    return primary ? primary->url : "./repo";
}

/**
 * Odstraní repozitář z konfigurace.
 *
 * Uvolní přidruženou paměť a odstraní ze spojového seznamu.
 *
 * @param name Název repozitáře k odstranění
 * @return 0 při úspěchu, -1 pokud repozitář nebyl nalezen
 */
int remove_repository(const char *name) {
    repo_config_t *prev = NULL;
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (strcmp(r->name, name) == 0) {
            // Odstraň ze spojového seznamu
            if (prev) {
                prev->next = r->next;
            } else {
                repo_list = r->next;
            }
            // Uvolni paměť
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
 * Povolí nebo zakáže repozitář.
 *
 * Zakázané repozitáře jsou ignorovány při rozlišování balíků.
 *
 * @param name Název repozitáře k úpravě
 * @param enabled 1 pro povolení, 0 pro zakázání
 * @return 0 při úspěchu, -1 pokud repozitář nebyl nalezen
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
 * Přidá nový repozitář do konfigurace.
 *
 * Repozitář je ve výchozím stavu povolen. Selže pokud název již existuje.
 *
 * @param name Jedinečný název repozitáře
 * @param url URL repozitáře nebo lokální cesta
 * @param priority Úroveň priority (vyšší = preferovaný)
 * @return 0 při úspěchu, -1 pokud repozitář již existuje
 */
int add_repository(const char *name, const char *url, int priority) {
    // Zkontroluj zda již existuje
    for (repo_config_t *r = repo_list; r; r = r->next) {
        if (strcmp(r->name, name) == 0) {
            log_warn("Repository '%s' already exists", name);
            return -1;
        }
    }
    
    // Vytvoř nový záznam repozitáře
    repo_config_t *new_repo = calloc(1, sizeof(repo_config_t));
    new_repo->name = strdup(name);
    new_repo->url = strdup(url);
    new_repo->priority = priority;
    new_repo->enabled = 1;  // Ve výchozím stavu povoleno
    
    // Přidej na konec spojového seznamu
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
 * Uloží aktuální konfiguraci repozitáře do uživatelského konfiguračního souboru.
 *
 * Vytvoří adresář ~/.config/balikar/ pokud neexistuje.
 * Zapisuje INI formát: záhlaví [sekce] s páry klíč=hodnota.
 *
 * @return 0 při úspěchu, -1 při chybě
 */
int save_repo_config(void) {
    const char *home = getenv("HOME");
    if (!home) return -1;
    
    char config_dir[512], config_file[512];
    snprintf(config_dir, sizeof(config_dir), "%s/%s", home, USER_CONFIG_DIR);
    snprintf(config_file, sizeof(config_file), "%s/%s", config_dir, USER_CONFIG_FILE);
    
    // Vytvoř adresář pokud je potřeba
    mkdir(config_dir, 0755);
    
    FILE *fp = fopen(config_file, "w");
    if (!fp) {
        log_error("Cannot write config to %s", config_file);
        return -1;
    }
    
    // Zapiš záhlaví
    fprintf(fp, "# Balikar Repository Configuration\n");
    fprintf(fp, "# Auto-generated file\n\n");
    
    // Zapiš každý repozitář jako sekci
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
 * Vypíše všechny nakonfigurované repozitáře v tabulkovém formátu.
 *
 * Zobrazí název, prioritu, stav povolení a URL.
 * Inicializuje konfiguraci pokud ještě nebyla načtena.
 */
void list_repositories(void) {
    if (!repo_list) {
        init_repo_config();
    }
    
    printf("Configured repositories:\n");
    printf("%-20s %-10s %-10s %s\n", "NAME", "PRIORITY", "ENABLED", "URL");
    printf("------------------------------------------------------------------------\n");
    
    // Vypiš každý repozitář
    for (repo_config_t *r = repo_list; r; r = r->next) {
        printf("%-20s %-10d %-10s %s\n", 
               r->name, r->priority, 
               r->enabled ? "yes" : "no", 
               r->url);
    }
}

/**
 * Uvolní veškerou paměť použitou konfigurací repozitáře.
 *
 * Prochází spojový seznam a uvolní každý uzel a jeho řetězce.
 * Mělo by být voláno před ukončením programu pro prevenci memory leaků.
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
