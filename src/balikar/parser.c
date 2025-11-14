#include "includes/parser.h"

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

// Helper to find Solvable by name and version (pro matching po repo_add_conda)
static Id find_solvable_by_name_version(Pool *pool, Repo *repo, const char *name, const char *version) {
    if (!pool || !repo || !name || !version) return 0;

    Id name_id = pool_str2id(pool, name, 0);
    if (!name_id) return 0;

    Solvable *s;
    Id p;
    FOR_REPO_SOLVABLES(repo, p, s) {
        if (s->name == name_id) {
            const char *evr_str = pool_id2evr(pool, s->evr);
            if (evr_str && strncmp(evr_str, version, strlen(version)) == 0) {  // Přibližný match, protože evr = version-build
                return p;
            }
        }
    }
    return 0;
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

    // is_group
    json_t *is_group = json_object_get(index, "is_group");
    if (is_group && json_is_boolean(is_group)) {
        repodata_set_str(data, p, pool_str2id(pool, "is_group", 1), json_is_true(is_group) ? "true" : "false");
    }

    // no_package
    json_t *no_package = json_object_get(index, "no_package");
    if (no_package && json_is_boolean(no_package)) {
        repodata_set_str(data, p, pool_str2id(pool, "no_package", 1), json_is_true(no_package) ? "true" : "false");
    }
}

// Process index JSON: Vytvoř tmp repodata pro repo_add_conda, přidej custom
static Id process_index_json(json_t *index, json_t *packages, Pool *pool, Repo *repo, const char *fallback_source) {
    if (!index) return 0;

    add_default_fields(index);  // Přidej defaulty

    // Pro agregaci do packages (pro generate_repodata)
    if (packages) {
        const char *name = json_string_value(json_object_get(index, "name"));
        const char *version = json_string_value(json_object_get(index, "version"));
        const char *build = json_string_value(json_object_get(index, "build"));
        char pkg_key[1024];
        snprintf(pkg_key, sizeof(pkg_key), "%s-%s-%s.tar.xz", name ? name : "unknown", version ? version : "0", build ? build : "0");
        json_object_set(packages, pkg_key, index);  // Přidá reference, ne copy (OK pro generate)
    }

    if (!pool || !repo) return 0;  // Pokud no repo, jen pro generate

    // Vytvoř tmp repodata.json pro single package
    json_t *tmp_repodata = json_object();
    json_t *tmp_packages = json_object();
    const char *name = json_string_value(json_object_get(index, "name"));
    const char *version = json_string_value(json_object_get(index, "version"));
    char dummy_key[1024];
    snprintf(dummy_key, sizeof(dummy_key), "%s-%s.tar.bz2", name ? name : "dummy", version ? version : "0");
    json_object_set(tmp_packages, dummy_key, index);
    json_object_set(tmp_repodata, "packages", tmp_packages);

    char tmp_path[] = "/tmp/conda_tmp_repodata_XXXXXX.json";
    int fd = mkstemps(tmp_path, 5);  // .json suffix
    if (fd == -1) {
        log_error("Failed to create tmp repodata file");
        json_decref(tmp_repodata);
        return 0;
    }
    close(fd);

    if (json_dump_file(tmp_repodata, tmp_path, 0) != 0) {
        log_error("Failed to dump tmp repodata");
        unlink(tmp_path);
        json_decref(tmp_repodata);
        return 0;
    }
    json_decref(tmp_repodata);

    FILE *fp = fopen(tmp_path, "r");
    if (!fp) {
        log_error("Failed to open tmp repodata");
        unlink(tmp_path);
        return 0;
    }

    // Přidej přes repo_add_conda
    repo_add_conda(repo, fp, 0);  // 0 pro default flags
    fclose(fp);
    unlink(tmp_path);

    // Najdi přidaný Solvable
    Id p = find_solvable_by_name_version(pool, repo, name, version);
    if (!p) {
        log_error("Failed to find added solvable for %s-%s", name, version);
        return 0;
    }

    // Přidej custom fields
    Repodata *data = repo_last_repodata(repo);
    add_custom_fields_to_repodata(index, data, p, pool, fallback_source);

    return p;
}

// Generate default metadata if missing
json_t* generate_default_metadata(const char* basename) {
    json_t* index = json_object();
    if (!index) return NULL;

    char name[256], version[256];
    if (sscanf(basename, "%255[^-]-%255[^-]", name, version) < 2) {
        strcpy(name, "unknown");
        strcpy(version, "0");
    }

    json_object_set_nocheck(index, "name", json_string(name));
    json_object_set_nocheck(index, "version", json_string(version));
    json_object_set_nocheck(index, "build", json_string("0"));
    json_object_set_nocheck(index, "arch", json_string("noarch"));
    json_object_set_nocheck(index, "depends", json_array());

    add_default_fields(index);

    return index;
}

// Collect packages from repo_path (pro generate_repodata)
void collect_packages(const char* repo_path, json_t* packages, Pool *pool, Repo *repo) {
    DIR* dir = opendir(repo_path);
    if (!dir) {
        log_error("Cannot open repo directory: %s", repo_path);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", repo_path, entry->d_name);

            json_t* index = load_json_from_directory(full_path);
            if (!index) continue;

            process_index_json(index, packages, pool, repo, NULL);  // fallback_source NULL pro dir
            json_decref(index);  // Protože process bere reference

        } else if (entry->d_type == DT_REG && strstr(entry->d_name, ".tar.bz2")) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", repo_path, entry->d_name);

            char tmp_dir[1024];
            snprintf(tmp_dir, sizeof(tmp_dir), "/tmp/balikar_extract_%d", (int)time(NULL));

            if (mkdir(tmp_dir, 0755) != 0) {
                log_error("Failed to create tmp dir for tar: %s", tmp_dir);
                continue;
            }

            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "tar xjf \"%s\" -C \"%s\" 2>/dev/null", full_path, tmp_dir);
            int extract_ret = system(cmd);
            if (extract_ret != 0) {
                log_error("Failed to extract %s", full_path);
                snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
                system(cmd);
                continue;
            }

            json_t* index = find_and_load_index(tmp_dir);
            if (!index) {
                log_warn("No index.json in %s – generating default", full_path);
                index = generate_default_metadata(entry->d_name);
            }

            process_index_json(index, packages, pool, repo, full_path);  // fallback_source = full_path

            json_decref(index);

            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
            system(cmd);
        }
    }
    closedir(dir);
}

// Main function to generate repodata.json (jen generate soubor, přidávání do repo odděleno)
int generate_repodata(const char* repo_path) {
    json_t* repodata = json_object();
    if (!repodata) return 1;

    json_t* info = json_object();
    json_object_set_nocheck(info, "subdir", json_string("noarch"));
    json_object_set_nocheck(info, "platform", json_string("linux"));
    json_object_set_nocheck(info, "arch", json_string("x86_64"));
    json_object_set_nocheck(repodata, "info", info);

    json_t* packages = json_object();
    collect_packages(repo_path, packages, NULL, NULL);  // Jen pro packages, no pool/repo
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

// Load repository: Generate repodata if needed, then repo_add_conda
Repo *load_repository(Pool *pool, const char *repo_path) {
    if (!pool || !repo_path) return NULL;

    char repodata_path[1024];
    snprintf(repodata_path, sizeof(repodata_path), "%s/repodata.json", repo_path);

    struct stat st;
    if (stat(repodata_path, &st) != 0) {
        log_info("No repodata.json – generating...");
        if (generate_repodata(repo_path) != 0) return NULL;
    }

    FILE *fp = fopen(repodata_path, "r");
    if (!fp) {
        log_error("Failed to open %s", repodata_path);
        return NULL;
    }

    Repo *repo = repo_create(pool, repo_path);
    if (!repo) {
        fclose(fp);
        return NULL;
    }

    repo_add_conda(repo, fp, 0);
    fclose(fp);

    repo_internalize(repo);
    pool_createwhatprovides(pool);

    log_info("Repository loaded from %s", repodata_path);
    return repo;
}

// Custom metadata lookup functions (zůstávají stejné)
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

// add_package_tar_bz2 - pro lokální tar.bz2 (používá process_index_json s repo_add_conda)
Id add_package_tar_bz2(const char *tar_path, Pool *pool, Repo *repo) {
    if (!tar_path || !pool || !repo) {
        log_error("Invalid arguments in add_package_tar_bz2.");
        return 0;
    }

    Id p = 0;
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
            goto cleanup;
        }
    }

    // Zpracuj (repo_add_conda + custom)
    p = process_index_json(index, NULL, pool, repo, tar_path);

    json_decref(index);

cleanup:
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
    system(cmd);

    if (p != 0) {
        log_info("Added %s as solvable %d", tar_path, p);
    } else {
        log_error("Failed to add %s", tar_path);
    }

    return p;
}