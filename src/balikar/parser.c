//
// Created by placeja5 on 11/4/25.
//

#include "includes/parser.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>  // pro isspace v trim

// Helper: Ořízni trailing/leading whitespace z stringu (vrací nový strdup)
static char* trim_str(const char* str) {
    if (!str) return NULL;

    // Skip leading whitespace
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return strdup("");  // Prázdný

    // Najdi konec a ořízni trailing
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Vytvoř nový string
    size_t len = (end - str) + 1;
    char* trimmed = malloc(len + 1);
    if (trimmed) {
        strncpy(trimmed, str, len);
        trimmed[len] = '\0';
    } else {
        log_error("Malloc failed for trim_str");
    }
    return trimmed;
}

// Function to load JSON from file
json_t* load_json(const char* filepath) {
    json_error_t error;
    json_t* root = json_load_file(filepath, 0, &error);
    if (!root) {
        log_error("Error loading JSON from %s: %s", filepath, error.text);
        return NULL;
    }
    return root;
}

// Helper: Rekurzivně najdi a načti první index.json v dir (pro tar.bz2 struktury s podadresářem)
static json_t* find_and_load_index(const char* dir_path) {
    if (!dir_path) return NULL;

    DIR* d = opendir(dir_path);
    if (!d) {
        log_error("Cannot open dir for index search: %s", dir_path);
        return NULL;
    }

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Nejdřív zkus přímo v tomto podadresáři (root/info pattern)
            char index_path[1024];
            snprintf(index_path, sizeof(index_path), "%s/index.json", full_path);
            json_t* index = load_json(index_path);
            if (!index) {
                snprintf(index_path, sizeof(index_path), "%s/info/index.json", full_path);
                index = load_json(index_path);
            }
            if (index) {
                log_info("Found index.json in subdir: %s", index_path);
                closedir(d);
                return index;
            }

            // Pokud ne, rekurzivně hledej dál
            index = find_and_load_index(full_path);
            if (index) {
                closedir(d);
                return index;
            }
        } else if (entry->d_type == DT_REG && strcmp(entry->d_name, "index.json") == 0) {
            json_t* index = load_json(full_path);
            if (index) {
                log_info("Found index.json directly: %s", full_path);
                closedir(d);
                return index;
            }
        }
    }
    closedir(d);
    return NULL;
}

// Helper function to load JSON from directory structure (pro plain: jen root/info)
static json_t* load_json_from_directory(const char* dir_path) {
    char index_path[1024];

    // Try root directory first
    snprintf(index_path, sizeof(index_path), "%s/index.json", dir_path);
    json_t* index = load_json(index_path);

    if (!index) {
        // Try info/ subdirectory
        snprintf(index_path, sizeof(index_path), "%s/info/index.json", dir_path);
        index = load_json(index_path);
    }

    return index;
}

// Add default fields to index JSON if missing
static void add_default_fields(json_t* index) {
    if (!json_object_get(index, "build_number")) {
        json_object_set_nocheck(index, "build_number", json_integer(0));
    }
    if (!json_object_get(index, "license")) {
        json_object_set_nocheck(index, "license", json_string("unknown"));
    }
    if (!json_object_get(index, "md5")) {
        json_object_set_nocheck(index, "md5", json_string(""));
    }
    if (!json_object_get(index, "sha256")) {
        json_object_set_nocheck(index, "sha256", json_string(""));
    }
    if (!json_object_get(index, "size")) {
        json_object_set_nocheck(index, "size", json_integer(0));
    }
    if (!json_object_get(index, "subdir")) {
        json_object_set_nocheck(index, "subdir", json_string("noarch"));
    }
    if (!json_object_get(index, "timestamp")) {
        time_t now = time(NULL);
        json_object_set_nocheck(index, "timestamp", json_integer((json_int_t)now * 1000));
    }
}

// Společná funkce pro inicializaci Solvable z JSON indexu
static Id init_solvable_from_json(json_t *index, Pool *pool, Repo *repo) {
    if (!index || !pool || !repo) return 0;

    Id p = repo_add_solvable(repo);
    if (p == 0) {
        log_error("Failed to add solvable");
        return 0;
    }

    Solvable *s = pool->solvables + p;

    // Nastav name, evr, arch
    const char *name = json_string_value(json_object_get(index, "name"));
    if (name) s->name = pool_str2id(pool, name, 1);

    const char *version = json_string_value(json_object_get(index, "version"));
    if (version) {
        const char *build = json_string_value(json_object_get(index, "build"));
        char *evr = NULL;
        if (build) asprintf(&evr, "%s-%s", version, build);
        else evr = strdup(version);
        if (evr) {
            s->evr = pool_str2id(pool, evr, 1);
            free(evr);
        }
    }

    json_t *json_arch = json_object_get(index, "arch");
    if (json_arch && json_is_string(json_arch)) {
        s->arch = pool_str2id(pool, json_string_value(json_arch), 1);
    } else {
        const char *default_arch = get_pool_arch(pool);
        s->arch = pool_str2id(pool, default_arch, 1);
        log_warn("No arch in index.json; using default %s", default_arch);
    }

    // Parsuj depends
    json_t *depends = json_object_get(index, "depends");
    if (depends && json_is_array(depends)) {
        parse_depends_to_solvable(s, depends, pool);
    }

    return p;
}

// Společná funkce pro přidání custom polí do Repodata (s fallback pro source)
static void add_custom_fields_to_repodata(json_t *index, Repodata *data, Id p, Pool *pool, const char* fallback_source) {
    if (!index || !data) return;

    // Makedeps
    json_t *mkdeps = json_object_get(index, "mkdeps");
    if (mkdeps && json_is_array(mkdeps)) {
        char *str = json_dumps(mkdeps, JSON_COMPACT);
        if (str) {
            repodata_set_str(data, p, pool_str2id(pool, "makedeps", 1), str);
            free(str);
        }
    }

    // Source/download_url (s trim)
    json_t *source = json_object_get(index, "source");
    int source_set = 0;
    if (source && json_is_string(source)) {
        const char* source_val = json_string_value(source);
        char* trimmed_source = trim_str(source_val);
        if (trimmed_source && trimmed_source[0] != '\0') {
            repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), trimmed_source);
            source_set = 1;
        }
        free(trimmed_source);
    }
    if (!source_set && fallback_source) {
        repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), fallback_source);
        log_info("Set fallback download_url: %s", fallback_source);
    }

    // Extras
    json_t *extras = json_object_get(index, "extras");
    if (extras && json_is_array(extras)) {
        char *str = json_dumps(extras, JSON_COMPACT);
        if (str) {
            repodata_set_str(data, p, pool_str2id(pool, "extras", 1), str);
            free(str);
        }
    }

    // Boolean fields
    json_t *is_group = json_object_get(index, "is_group");
    repodata_set_str(data, p, pool_str2id(pool, "is_group", 1),
                    json_is_true(is_group) ? "true" : "false");

    json_t *no_package = json_object_get(index, "no_package");
    repodata_set_str(data, p, pool_str2id(pool, "no_package", 1),
                    json_is_true(no_package) ? "true" : "false");
}

// Refaktoroaná funkce: Zpracování indexu (přijímá již načtený index, přidá do packages pokud poskytnuto, inicializuje solvable pokud pool/repo)
static Id process_index_json(json_t *index, json_t *packages, Pool *pool, Repo *repo, const char* fallback_source) {
    if (!index) return 0;

    // Trim source přímo v index JSON (pro packages výstup)
    json_t *source = json_object_get(index, "source");
    if (source && json_is_string(source)) {
        const char* source_val = json_string_value(source);
        char* trimmed_source = trim_str(source_val);
        if (trimmed_source) {
            json_object_set_nocheck(index, "source", json_string(trimmed_source));
            log_info("Trimmed source in index: %s", trimmed_source);
            free(trimmed_source);
        }
    }

    add_default_fields(index);

    Id p = 0;
    if (pool && repo) {
        // Inicializuj Solvable
        p = init_solvable_from_json(index, pool, repo);
        if (p == 0) {
            json_decref(index);
            return 0;
        }

        // Přidej custom pole
        Repodata *data = repo_add_repodata(repo, 0);
        if (data) {
            add_custom_fields_to_repodata(index, data, p, pool, fallback_source);

            // Přidej standardní metadata
            const char *license = json_string_value(json_object_get(index, "license"));
            if (license) repodata_set_str(data, p, SOLVABLE_LICENSE, license);

            const char *summary = json_string_value(json_object_get(index, "summary"));
            if (summary) repodata_set_str(data, p, SOLVABLE_SUMMARY, summary);
        }
    }

    // Přidej do packages JSON objektu (pokud packages poskytnuto)
    if (packages) {
        char* key = generate_package_key(index);
        if (key) {
            json_object_set_nocheck(packages, key, index);
            log_info("Added to packages: %s", key);  // Debug log
            free(key);
        } else {
            log_warn("Failed to generate package key – skipping packages entry");
        }
    }

    // Uvolni reference (set už má svou, pokud byl)
    json_decref(index);

    return p;
}

// Function to generate package key: "name-version-build.tar.bz2"
char* generate_package_key(json_t* index) {
    const char* name = json_string_value(json_object_get(index, "name"));
    const char* version = json_string_value(json_object_get(index, "version"));
    const char* build = json_string_value(json_object_get(index, "build"));
    if (!name) {
        log_warn("Missing name for package key in index.json");
        return NULL;
    }
    // Version a build mohou být fallback "unknown"/"1"
    if (!version) version = "unknown";
    if (!build) build = "1";
    size_t len = strlen(name) + strlen(version) + strlen(build) + 12;
    char* key = malloc(len);
    if (key) {
        snprintf(key, len, "%s-%s-%s.tar.bz2", name, version, build);
    } else {
        log_error("Malloc failed for package key");
    }
    return key;
}

// Helper function to generate default metadata from filename
static json_t* generate_default_metadata(const char* basename) {
    char clean_base[256];
    strncpy(clean_base, basename, sizeof(clean_base) - 1);
    clean_base[sizeof(clean_base) - 1] = '\0';

    // Ořízni .tar.bz2 nebo .tbz2, pokud končí tím
    size_t len = strlen(clean_base);
    if (len > 8 && strcmp(clean_base + len - 8, ".tar.bz2") == 0) {
        clean_base[len - 8] = '\0';
    } else if (len > 5 && strcmp(clean_base + len - 5, ".tbz2") == 0) {
        clean_base[len - 5] = '\0';
    }

    char name_buf[256], ver_buf[256], build_buf[256];
    int parsed = sscanf(clean_base, "%255[^-]-%255[^-]-%255s", name_buf, ver_buf, build_buf);

    json_t* index = json_object();
    if (!index) return NULL;

    json_object_set_nocheck(index, "name", json_string(parsed >= 1 ? name_buf : "unknown"));
    json_object_set_nocheck(index, "version", json_string(parsed >= 2 ? ver_buf : "unknown"));
    json_object_set_nocheck(index, "build", json_string(parsed >= 3 ? build_buf : "1"));
    json_object_set_nocheck(index, "arch", json_string("noarch"));
    json_object_set_nocheck(index, "depends", json_array());
    json_object_set_nocheck(index, "mkdeps", json_array());
    json_object_set_nocheck(index, "extras", json_array());
    json_object_set_nocheck(index, "is_group", json_false());
    json_object_set_nocheck(index, "no_package", json_false());
    json_object_set_nocheck(index, "source", json_string(""));

    log_info("Generated default for %s: name=%s, ver=%s, build=%s", basename,
             parsed >= 1 ? name_buf : "unknown",
             parsed >= 2 ? ver_buf : "unknown",
             parsed >= 3 ? build_buf : "1");

    return index;
}

// Upravená collect_packages funkce pro plain adresáře i tar.bz2 soubory
void collect_packages(const char* root_path, json_t* packages, Pool *pool, Repo *repo) {
    DIR* dir = opendir(root_path);
    if (!dir) {
        log_error("Error opening directory %s: %s", root_path, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", root_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Rekurzivně projdi podadresáře
            collect_packages(full_path, packages, pool, repo);

            // Zkontroluj zda adresář obsahuje index.json (plain package)
            char index_path[MAX_PATH_LEN];
            snprintf(index_path, sizeof(index_path), "%s/index.json", full_path);
            struct stat st;
            if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
                // Našli jsme index.json v adresáři - zpracuj jako plain package
                log_info("Processing plain directory package: %s", full_path);
                json_t* index = load_json(index_path);
                if (index) {
                    process_index_json(index, packages, pool, repo, NULL);
                }
            }
        } else if (entry->d_type == DT_REG) {
            // Zkontroluj zda se jedná o tar.bz2 soubor (správná kontrola suffixu)
            size_t len = strlen(entry->d_name);
            int is_tar_bz2 = (len > 8 && strcmp(entry->d_name + len - 8, ".tar.bz2") == 0);
            int is_tbz2 = (len > 5 && strcmp(entry->d_name + len - 5, ".tbz2") == 0);
            if (is_tar_bz2 || is_tbz2) {
                log_info("Processing tar.bz2 package: %s", full_path);

                // Jedna extrakce pro vše
                char tmp_template[] = "/tmp/balikar_temp_XXXXXX";
                char* tmp_dir = mkdtemp(tmp_template);
                if (!tmp_dir) {
                    log_error("Failed to create tmp dir for %s: %s", full_path, strerror(errno));
                    continue;
                }
                log_debug(1, "Extracting %s to %s", full_path, tmp_dir);

                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "tar -xjf \"%s\" -C \"%s\"", full_path, tmp_dir);
                int extract_ret = system(cmd);
                if (extract_ret != 0) {
                    log_error("Failed to extract %s (exit %d)", full_path, extract_ret);
                    goto cleanup_tar;
                }
                log_info("Extracted %s to %s successfully", full_path, tmp_dir);

                // Načti index.json: Nejdřív rekurzivně hledej (pro struktury s podadresářem)
                json_t* index = find_and_load_index(tmp_dir);
                if (!index) {
                    log_warn("No index.json found in %s (searched recursively) – generating default", full_path);
                    char *basename = entry->d_name;  // Použij entry->d_name
                    index = generate_default_metadata(basename);
                    if (!index) {
                        log_error("Failed to generate default for %s", basename);
                        goto cleanup_tar;
                    }
                } else {
                    log_info("Loaded index.json from extracted %s (recursive search)", full_path);
                    add_default_fields(index);  // Pro jistotu
                }

                // Zpracuj index (libsolv + packages)
                process_index_json(index, packages, pool, repo, full_path);  // fallback_source = full_path

            cleanup_tar:
                snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
                system(cmd);
                log_debug(1, "Cleaned up %s", tmp_dir);
            }
        }
    }
    closedir(dir);
}

// Main function to generate repodata.json
int generate_repodata(const char* repo_path, Pool *pool, Repo *repo) {
    json_t* repodata = json_object();
    if (!repodata) return 1;

    json_t* info = json_object();
    json_object_set_nocheck(info, "subdir", json_string("noarch"));
    json_object_set_nocheck(info, "platform", json_string("linux"));
    json_object_set_nocheck(info, "arch", json_string("x86_64"));
    json_object_set_nocheck(repodata, "info", info);

    json_t* packages = json_object();
    collect_packages(repo_path, packages, pool, repo);
    json_object_set_nocheck(repodata, "packages", packages);

    json_t* packages_conda = json_object();
    json_object_set_nocheck(repodata, "packages.conda", packages_conda);

    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/repodata.json", repo_path);
    if (json_dump_file(repodata, output_path, JSON_INDENT(4)) != 0) {
        log_error("Error writing repodata.json");
        json_decref(repodata);
        return 1;
    }

    json_decref(repodata);
    printf("repodata.json generated at %s\n", output_path);
    return 0;
}

// parse_depends_to_solvable
void parse_depends_to_solvable(Solvable *s, json_t *depends, Pool *pool) {
    size_t i;
    json_t *dep_item;

    json_array_foreach(depends, i, dep_item) {
        if (!json_is_string(dep_item)) {
            log_warn("Invalid depend item: not a string - skipping");
            continue;
        }

        const char *dep_str = json_string_value(dep_item);
        char name_buf[256], op_buf[3], ver_buf[256];
        int parsed = sscanf(dep_str, "%255s %2s %255s", name_buf, op_buf, ver_buf);

        Id name_id = pool_str2id(pool, name_buf, 1);
        Id dep_id;

        if (parsed == 1) {
            dep_id = name_id;
        } else if (parsed == 3) {
            Id evr_id = pool_str2id(pool, ver_buf, 1);
            Id rel;
            if (strcmp(op_buf, "==") == 0 || strcmp(op_buf, "=") == 0) {
                rel = REL_EQ;
            } else if (strcmp(op_buf, ">") == 0) {
                rel = REL_GT;
            } else if (strcmp(op_buf, "<") == 0) {
                rel = REL_LT;
            } else if (strcmp(op_buf, ">=") == 0) {
                rel = REL_GT | REL_EQ;
            } else if (strcmp(op_buf, "<=") == 0) {
                rel = REL_LT | REL_EQ;
            } else {
                log_warn("Unknown relation operator '%s' in depend '%s' - skipping", op_buf, dep_str);
                continue;
            }
            dep_id = pool_rel2id(pool, name_id, evr_id, rel, 1);
        } else {
            log_warn("Invalid depend format '%s' - skipping", dep_str);
            continue;
        }

        solvable_add_deparray(s, SOLVABLE_REQUIRES, dep_id, 0);
    }
}

// Custom metadata lookup functions
json_t* get_custom_mkdeps(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "makedeps", 0);
    const char *mkdeps_str = repodata_lookup_str(data, p, key);

    if (!mkdeps_str) {
        log_warn("No 'makedeps' found for solvable %d", p);
        return NULL;
    }

    json_error_t error;
    json_t *mkdeps = json_loads(mkdeps_str, 0, &error);
    if (!mkdeps || !json_is_array(mkdeps)) {
        log_warn("Failed to parse 'makedeps' string: %s", error.text);
        if (mkdeps) json_decref(mkdeps);
        return NULL;
    }

    return mkdeps;
}

const char* get_custom_source(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "download_url", 0);
    const char *source_url = repodata_lookup_str(data, p, key);

    if (!source_url) {
        log_warn("No 'download_url' found for solvable %d", p);
        return NULL;
    }

    return source_url;
}

json_t* get_custom_extras(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "extras", 0);
    const char *extras_str = repodata_lookup_str(data, p, key);

    if (!extras_str) {
        log_warn("No 'extras' found for solvable %d", p);
        return NULL;
    }

    json_error_t error;
    json_t *extras = json_loads(extras_str, 0, &error);
    if (!extras || !json_is_array(extras)) {
        log_warn("Failed to parse 'extras' string: %s", error.text);
        if (extras) json_decref(extras);
        return NULL;
    }

    return extras;
}

int get_custom_is_group(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "is_group", 0);
    const char *str = repodata_lookup_str(data, p, key);

    if (!str) {
        log_warn("No 'is_group' found for solvable %d - assuming false", p);
        return 0;
    }

    return strcmp(str, "true") == 0 ? 1 : 0;
}

int get_custom_no_package(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "no_package", 0);
    const char *str = repodata_lookup_str(data, p, key);

    if (!str) {
        log_warn("No 'no_package' found for solvable %d - assuming false", p);
        return 0;
    }

    return strcmp(str, "true") == 0 ? 1 : 0;
}

// internalize_repo
void internalize_repo(Repo *repo) {
    repo_internalize(repo);
    log_info("Repository internalized for lookup");
}

// add_package_tar_bz2 - refaktoroaná pro lokální použití (jen libsolv, bez packages; používá rekurzi)
Id add_package_tar_bz2(const char *tar_path, Pool *pool, Repo *repo) {
    if (!tar_path || !pool || !repo) {
        log_error("Invalid arguments in add_package_tar_bz2.");
        return 0;
    }

    Id p = 0; // INICIALIZOVAT NA 0
    char tmp_template[] = "/tmp/balikar_extract_XXXXXX";
    char *tmp_dir = mkdtemp(tmp_template);
    if (!tmp_dir) {
        log_error("Failed to create tmp dir: %s", strerror(errno));
        return 0;
    }

    log_debug(1, "Extracting %s to %s", tar_path, tmp_dir);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "tar xjf \"%s\" -C \"%s\" 2>/dev/null", tar_path, tmp_dir);
    int extract_ret = system(cmd);
    if (extract_ret != 0) {
        log_error("Failed to extract %s (exit %d)", tar_path, extract_ret);
        // Pokračuj na cleanup, p zůstane 0
        goto cleanup;
    }
    log_info("Extracted %s to %s successfully", tar_path, tmp_dir);

    // Načti index.json rekurzivně
    json_t* index = find_and_load_index(tmp_dir);
    if (!index) {
        log_warn("No index.json in %s – generating default", tar_path);
        char *basename = strrchr(tar_path, '/') ? strrchr(tar_path, '/') + 1 : (char *)tar_path;
        index = generate_default_metadata(basename);
        if (!index) {
            log_error("Failed to generate default for %s", basename);
            goto cleanup;
        }
    }

    // Zpracuj (jen libsolv, bez packages, fallback_source = tar_path)
    p = process_index_json(index, NULL, pool, repo, tar_path);

    cleanup:
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
    system(cmd);

    if (p != 0) {
        log_info("Added %s as solvable %d", tar_path, p);
    }

    return p; // Pokud došlo k chybě, p bude 0
}