#include "includes/parser.h"

// Helper: Compute SHA256 hash of file and return hex string (caller must free)
static char* compute_file_sha256(const char* file_path) {
    if (!file_path) return NULL;
    
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        log_debug(2, "Cannot open file for SHA256: %s", file_path);
        return NULL;
    }
    
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        SHA256_Update(&ctx, buffer, n);
    }
    fclose(fp);
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);
    
    char *hex = malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    if (!hex) return NULL;
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    hex[SHA256_DIGEST_LENGTH * 2] = '\0';
    
    return hex;
}

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

// Helper: Load JSON quietly (no error logging for missing files)
static json_t* load_json_quiet(const char* filepath) {
    json_error_t error;
    json_t* root = json_load_file(filepath, 0, &error);
    return root;  // NULL if failed, no logging
}

// Function to load JSON from file (with error logging)
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
        log_debug(2, "Cannot open dir for index search: %s", dir_path);
        return NULL;
    }

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[2048];  // Zvětšeno pro bezpečnost
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Nejdřív zkus přímo v tomto podadresáři (root/info pattern)
            char index_path[2048];
            snprintf(index_path, sizeof(index_path), "%s/index.json", full_path);
            json_t* index = load_json_quiet(index_path);
            if (!index) {
                snprintf(index_path, sizeof(index_path), "%s/info/index.json", full_path);
                index = load_json_quiet(index_path);
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
            json_t* index = load_json_quiet(full_path);
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
    char index_path[2048];

    // Try root directory first (quietly - no error logging)
    snprintf(index_path, sizeof(index_path), "%s/index.json", dir_path);
    json_t* index = load_json_quiet(index_path);

    if (!index) {
        // Try info/ subdirectory
        snprintf(index_path, sizeof(index_path), "%s/info/index.json", dir_path);
        index = load_json_quiet(index_path);
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
// Opravená funkce: lépe matchuj solvable
static Id find_solvable_by_name_version(Pool *pool, Repo *repo, const char *name, const char *version) {
    if (!pool || !repo || !name) return 0;

    Id name_id = pool_str2id(pool, name, 0);
    if (!name_id) return 0;

    Solvable *s;
    Id p;
    FOR_REPO_SOLVABLES(repo, p, s) {
        if (s->name == name_id) {
            // Pokud není specifikována verze, vrať první match
            if (!version) return p;

            // Jinak zkontroluj EVR
            const char *evr_str = pool_id2str(pool, s->evr);
            if (evr_str && strcmp(evr_str, version) == 0) {
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

    // Source/download_url
    json_t *source = json_object_get(index, "source");
    int source_set = 0;
    if (source && json_is_string(source)) {
        const char* source_val = json_string_value(source);
        if (source_val && source_val[0] != '\0') {
            repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), source_val);
            source_set = 1;
        }
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
        repodata_set_str(data, p, pool_str2id(pool, "is_group", 1),
                        json_is_true(is_group) ? "true" : "false");
    } else {
        repodata_set_str(data, p, pool_str2id(pool, "is_group", 1), "false");
    }

    // no_package
    json_t *no_package = json_object_get(index, "no_package");
    if (no_package && json_is_boolean(no_package)) {
        repodata_set_str(data, p, pool_str2id(pool, "no_package", 1),
                        json_is_true(no_package) ? "true" : "false");
    } else {
        repodata_set_str(data, p, pool_str2id(pool, "no_package", 1), "false");
    }

}

// Process index JSON: Vytvoř tmp repodata.json pro repo_add_conda, přidej custom
// Přidej processingu depends -> requires konverzi
static Id process_index_json(json_t *index, json_t *packages, Pool *pool, Repo *repo, const char *fallback_source) {
    if (!index) return 0;

    add_default_fields(index);

    if (packages) {
        const char *name = json_string_value(json_object_get(index, "name"));
        const char *version = json_string_value(json_object_get(index, "version"));
        const char *build = json_string_value(json_object_get(index, "build"));
        char pkg_key[2048];
        snprintf(pkg_key, sizeof(pkg_key), "%s-%s-%s.tar.bz2", name ? name : "unknown", version ? version : "0", build ? build : "0");
        json_object_set(packages, pkg_key, index);
    }

    if (!pool || !repo) return 0;

    const char *name = json_string_value(json_object_get(index, "name"));
    const char *version = json_string_value(json_object_get(index, "version"));
    const char *build = json_string_value(json_object_get(index, "build"));

    if (!name) {
        log_error("Missing 'name' in index.json");
        return 0;
    }

    json_t *index_copy = json_deep_copy(index);
    json_t *depends = json_object_get(index_copy, "depends");

    if (depends && json_is_array(depends)) {
        json_object_del(index_copy, "requires");
        json_t *requires = json_array();
        size_t i;
        json_t *dep;
        json_array_foreach(depends, i, dep) {
            if (json_is_string(dep)) {
                const char *dep_str = json_string_value(dep);
                if (dep_str && strlen(dep_str) > 0) {
                    json_array_append(requires, dep);
                }
            }
        }
        json_object_set(index_copy, "requires", requires);
        json_decref(requires);
        log_info("Converted %zu depends to requires for %s", json_array_size(depends), name);
    }

    json_t *tmp_repodata = json_object();
    json_t *tmp_packages = json_object();

    char dummy_key[2048];
    snprintf(dummy_key, sizeof(dummy_key), "%s-%s-%s.tar.bz2", name, version ? version : "0", build ? build : "0");
    json_object_set(tmp_packages, dummy_key, index_copy);
    json_object_set(tmp_repodata, "packages", tmp_packages);
    json_decref(index_copy); // index_copy je nyní vlastněn tmp_packages

    char tmp_path[2048];
    strcpy(tmp_path, "/tmp/tmp_repodata_XXXXXX.json");
    int fd = mkstemps(tmp_path, 5);
    if (fd == -1) {
        log_error("Failed to create tmp repodata file");
        json_decref(tmp_repodata);
        return 0;
    }

    FILE *tmp_fp = fdopen(fd, "w");
    if (json_dumpf(tmp_repodata, tmp_fp, JSON_COMPACT) != 0) {
        log_error("Failed to dump tmp repodata");
        fclose(tmp_fp);
        unlink(tmp_path);
        json_decref(tmp_repodata);
        return 0;
    }
    fclose(tmp_fp);
    json_decref(tmp_repodata);

    tmp_fp = fopen(tmp_path, "r");
    if (!tmp_fp) {
        log_error("Failed to reopen tmp repodata");
        unlink(tmp_path);
        return 0;
    }

    repo_add_conda(repo, tmp_fp, 0);
    fclose(tmp_fp);
    unlink(tmp_path);

    Id p = find_solvable_by_name_version(pool, repo, name, version);
    if (p) {
        Solvable *s = pool_id2solvable(pool, p);
        Repodata *data = repo_last_repodata(repo);

        // repo_add_conda should have added requires from the JSON, but if it didn't work,
        // manually add them from the original depends array
        json_t *original_depends = json_object_get(index, "depends");
        if (original_depends && json_is_array(original_depends)) {
            size_t num_deps = json_array_size(original_depends);
            if (num_deps > 0) {
                // Use libsolv Queue for dependency IDs
                Queue q;
                queue_init(&q);

                for (size_t i = 0; i < num_deps; i++) {
                    json_t *dep_json = json_array_get(original_depends, i);
                    if (json_is_string(dep_json)) {
                        const char *dep_str = json_string_value(dep_json);
                        // Create ID for dependency name
                        Id dep_name_id = pool_str2id(pool, dep_str, 1);
                        queue_push(&q, dep_name_id);
                    }
                }

                if (q.count > 0) {
                    // Add dependency array from queue to repodata
                    repodata_set_idarray(data, p, SOLVABLE_REQUIRES, &q);
                    log_info("Manually set %d requires for solvable %d", q.count, p);
                }

                // Free queue memory
                queue_free(&q);
            }
        }

        // Ensure solvable provides itself (self-provide) - critical for solver
        // Add to repodata before internalize
        repodata_add_idarray(data, p, SOLVABLE_PROVIDES, s->name);
        log_debug(1, "Added self-provide for solvable %d (%s)", p, name);

        add_custom_fields_to_repodata(index, data, p, pool, fallback_source);
        repodata_internalize(data);
        log_info("Found and customized solvable %d for %s", p, name);

        if (s->requires) {
            const char *req_str = pool_dep2str(pool, s->requires);
            log_debug(1, "Solvable %d requires: '%s'", p, req_str ? req_str : "(null)");
        } else {
            log_warn("Solvable %d has no requires set", p);
        }
    } else {
        log_warn("Solvable not found for %s-%s", name, version ? version : "0");
    }

    return p;
}

// Generate default metadata from filename if index.json missing
json_t* generate_default_metadata(const char* filename) {
    if (!filename) return NULL;

    json_t* index = json_object();
    if (!index) return NULL;

    // Get basename
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;

    // Copy and remove .tar.bz2 extension
    char name_buf[256] = {0};
    strncpy(name_buf, basename, sizeof(name_buf) - 1);
    char *ext = strstr(name_buf, ".tar.bz2");
    if (ext) *ext = '\0';

    // Parse: name-version-build (find LAST two dashes)
    char *last_dash = strrchr(name_buf, '-');
    char *second_last_dash = NULL;
    if (last_dash) {
        *last_dash = '\0';
        second_last_dash = strrchr(name_buf, '-');
        *last_dash = '-';  // Restore temporarily
    }

    char name[256] = {0}, version[256] = {0}, build[256] = {0};

    if (second_last_dash && last_dash) {
        // Found name-version-build
        size_t name_len = second_last_dash - name_buf;
        strncpy(name, name_buf, name_len);
        name[name_len] = '\0';

        size_t version_len = last_dash - second_last_dash - 1;
        strncpy(version, second_last_dash + 1, version_len);
        version[version_len] = '\0';

        strcpy(build, last_dash + 1);
    } else if (last_dash) {
        // Only name-version
        size_t name_len = last_dash - name_buf;
        strncpy(name, name_buf, name_len);
        name[name_len] = '\0';
        strcpy(version, last_dash + 1);
        strcpy(build, "0");
    } else {
        // Just name
        strcpy(name, name_buf);
        strcpy(version, "0");
        strcpy(build, "0");
    }

    json_object_set_new(index, "name", json_string(name));
    json_object_set_new(index, "version", json_string(version));
    json_object_set_new(index, "build", json_string(build));
    json_object_set_new(index, "arch", json_string("noarch"));
    json_object_set_new(index, "depends", json_array());

    add_default_fields(index);
    return index;
}

// Helper: Rekurzivní procházení adresářů pro collect_packages
static void collect_packages_recursive(const char* current_path, json_t* packages, Pool *pool, Repo *repo) {
    DIR* dir = opendir(current_path);
    if (!dir) {
        log_debug(1, "Cannot open directory: %s", current_path);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Zkus načíst index.json z tohoto adresáře
            json_t* index = load_json_from_directory(full_path);
            if (index) {
                // Našli jsme balíček s index.json
                log_info("Found package at: %s", full_path);
                
                // For directory packages, try to compute SHA if there's a source tarball
                // Look for <name>-<version>.tar.* in current or parent directory
                const char *pkg_name = json_string_value(json_object_get(index, "name"));
                const char *pkg_version = json_string_value(json_object_get(index, "version"));
                
                if (pkg_name && pkg_version) {
                    // Try various patterns: name-version.tar.gz, name-version.tar.bz2, name-version-build.tar.*
                    const char *extensions[] = {".tar.gz", ".tar.bz2", ".tar.xz", NULL};
                    char tar_candidate[2048];
                    int found = 0;
                    
                    for (int i = 0; extensions[i] && !found; i++) {
                        // Try in current directory first
                        snprintf(tar_candidate, sizeof(tar_candidate), "%s/%s-%s%s", 
                                 full_path, pkg_name, pkg_version, extensions[i]);
                        if (access(tar_candidate, F_OK) == 0) {
                            found = 1;
                            break;
                        }
                        
                        // Try with build number if available
                        const char *pkg_build = json_string_value(json_object_get(index, "build"));
                        if (pkg_build) {
                            snprintf(tar_candidate, sizeof(tar_candidate), "%s/%s-%s-%s%s", 
                                     full_path, pkg_name, pkg_version, pkg_build, extensions[i]);
                            if (access(tar_candidate, F_OK) == 0) {
                                found = 1;
                                break;
                            }
                        }
                    }
                    
                    if (found) {
                        char *computed_sha = compute_file_sha256(tar_candidate);
                        if (computed_sha) {
                            json_t *existing_sha = json_object_get(index, "sha256");
                            if (!existing_sha || !json_is_string(existing_sha) || 
                                strlen(json_string_value(existing_sha)) == 0) {
                                json_object_set_new(index, "sha256", json_string(computed_sha));
                                log_info("Computed SHA256 from source tarball %s: %s", tar_candidate, computed_sha);
                            }
                            free(computed_sha);
                        }
                    }
                }
                
                process_index_json(index, packages, pool, repo, NULL);
                json_decref(index);
            } else {
                // Rekurzivně prohledej podadresáře
                collect_packages_recursive(full_path, packages, pool, repo);
            }
        } else if (entry->d_type == DT_REG && strstr(entry->d_name, ".tar.bz2")) {
            // Zpracuj .tar.bz2 soubor
            char tmp_dir[2048];
            snprintf(tmp_dir, sizeof(tmp_dir), "/tmp/balikar_extract_%d", (int)time(NULL));

            if (mkdir(tmp_dir, 0755) != 0) {
                log_error("Failed to create tmp dir for tar: %s", tmp_dir);
                continue;
            }

            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "tar xjf \"%s\" -C \"%s\" 2>/dev/null", full_path, tmp_dir);
            int extract_ret = system(cmd);
            if (extract_ret != 0) {
                log_error("Failed to extract %s (exit code %d)", full_path, extract_ret);
                snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
                system(cmd);
                continue;
            }

            // Compute SHA256 of tar file for metadata
            char *computed_sha = compute_file_sha256(full_path);
            if (computed_sha) {
                log_info("Computed SHA256 for %s: %s", full_path, computed_sha);
            }

            json_t* index = find_and_load_index(tmp_dir);
            if (!index) {
                log_warn("No index.json in %s – generating default", full_path);
                index = generate_default_metadata(entry->d_name);
            }

            // Inject computed SHA256 into index if missing
            if (computed_sha) {
                json_t *existing_sha = json_object_get(index, "sha256");
                if (!existing_sha || !json_is_string(existing_sha) || 
                    strlen(json_string_value(existing_sha)) == 0) {
                    json_object_set_new(index, "sha256", json_string(computed_sha));
                    log_info("Injected SHA256 into metadata for %s", full_path);
                }
                free(computed_sha);
            }

            process_index_json(index, packages, pool, repo, full_path);  // fallback_source = full_path

            json_decref(index);

            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
            system(cmd);
        }
    }
    closedir(dir);
}

// Collect packages from repo_path (pro generate_repodata) - wrapper pro rekurzivní funkci
void collect_packages(const char* repo_path, json_t* packages, Pool *pool, Repo *repo) {
    log_info("Collecting packages from: %s", repo_path);
    collect_packages_recursive(repo_path, packages, pool, repo);
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

    char output_path[2048];
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

    // Set arch before loading any repos
    set_system_arch(pool);

    char repodata_path[2048];
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

    // Load JSON again to add custom fields that repo_add_conda ignores
    json_error_t error;
    json_t *root = json_load_file(repodata_path, 0, &error);
    if (root) {
        json_t *packages = json_object_get(root, "packages");
        if (packages && json_is_object(packages)) {
            // Use the main repodata from repo (created by repo_add_conda)
            // We need to add fields before internalize
            Repodata *data = repo_id2repodata(repo, 1);  // ID 1 is typically the main repodata
            if (!data) {
                log_warn("No repodata found, cannot add custom fields");
                json_decref(root);
            } else {
            
            // Iterate through all solvables and add custom fields
            Id p;
            Solvable *s;
            FOR_REPO_SOLVABLES(repo, p, s) {
                if (!s->name) continue;
                
                const char *name = pool_id2str(pool, s->name);
                const char *evr = pool_id2str(pool, s->evr);
                
                                log_debug(2, "Looking for custom fields for solvable %d: %s-%s", p, name, evr);
                
                // Find matching package in JSON
                const char *key;
                json_t *pkg_json;
                                int found_match = 0;
                json_object_foreach(packages, key, pkg_json) {
                    const char *pkg_name = json_string_value(json_object_get(pkg_json, "name"));
                    const char *pkg_version = json_string_value(json_object_get(pkg_json, "version"));
                    
                                        log_debug(2, "Checking package %s: name=%s, version=%s", key, pkg_name, pkg_version);
                    
                    if (pkg_name && name && strcmp(pkg_name, name) == 0 &&
                        pkg_version && evr && strstr(evr, pkg_version)) {
                        
                                                found_match = 1;
                                                log_debug(1, "Found match for %s-%s in JSON", name, evr);
                        
                        // Add source/download_url
                        json_t *source = json_object_get(pkg_json, "source");
                        if (!source) source = json_object_get(pkg_json, "download_url");
                        if (source && json_is_string(source)) {
                            const char *source_val = json_string_value(source);
                                                        log_debug(1, "Set download_url for %s to: %s", name, source_val);
                            repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), source_val);
                        }
                        
                        // Add mkdeps
                
                                        if (!found_match) {
                                            log_debug(2, "No JSON match found for solvable %d (%s-%s)", p, name, evr);
                                        }
                        json_t *mkdeps = json_object_get(pkg_json, "mkdeps");
                        if (mkdeps && json_is_array(mkdeps)) {
                            char *str = json_dumps(mkdeps, JSON_COMPACT);
                            if (str) {
                                repodata_set_str(data, p, pool_str2id(pool, "makedeps", 1), str);
                                free(str);
                            }
                        }
                        
                        // Add extras
                        json_t *extras = json_object_get(pkg_json, "extras");
                        if (extras && json_is_array(extras)) {
                            char *str = json_dumps(extras, JSON_COMPACT);
                            if (str) {
                                repodata_set_str(data, p, pool_str2id(pool, "extras", 1), str);
                                free(str);
                            }
                        }
                        
                        // Add is_group
                        json_t *is_group = json_object_get(pkg_json, "is_group");
                        if (is_group && json_is_boolean(is_group)) {
                            repodata_set_str(data, p, pool_str2id(pool, "is_group", 1),
                                           json_is_true(is_group) ? "true" : "false");
                        }
                        
                        // Add no_package
                        json_t *no_package = json_object_get(pkg_json, "no_package");
                        if (no_package && json_is_boolean(no_package)) {
                            repodata_set_str(data, p, pool_str2id(pool, "no_package", 1),
                                           json_is_true(no_package) ? "true" : "false");
                        }
                        
                        break;
                    }
                }
            }
            }
        }
        json_decref(root);
    }
    
    repo_internalize(repo);
    
    // Ensure all solvables have self-provides (critical for solver)
    // Must be done AFTER internalize but BEFORE pool_createwhatprovides
    // repo_add_conda sometimes sets provides to invalid values (like 1), so overwrite it
    Id p;
    Solvable *s;
    FOR_REPO_SOLVABLES(repo, p, s) {
        if (s->name) {
            // Always set self-provide, even if provides is non-zero (may be invalid)
            s->provides = repo_addid_dep(repo, 0, s->name, 0);
            log_debug(1, "Set self-provide for solvable %d (%s)", p, pool_id2str(pool, s->name));
            
            // Fix arch if it's not set properly - use noarch for compatibility
            // noarch packages should be installable on any architecture
            if (!s->arch || s->arch == 0 || s->arch == 1) {
                s->arch = pool_str2id(pool, "noarch", 1);
                log_debug(1, "Set arch for solvable %d to noarch", p);
            }
        }
    }
    
    // Create whatprovides index AFTER adding all provides
    pool_createwhatprovides(pool);

    log_info("Repository loaded from %s", repodata_path);
    return repo;
}

// Custom metadata lookup functions (zůstávají stejné)
json_t* get_custom_mkdeps(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "makedeps", 0);
    const char *mkdeps_str = repodata_lookup_str(data, p, key);

    if (!mkdeps_str) {
        log_debug(2, "No 'makedeps' found for solvable %d", p);
        return NULL;
    }

    json_error_t error;
    json_t *mkdeps = json_loads(mkdeps_str, 0, &error);
    if (!mkdeps || !json_is_array(mkdeps)) {
        log_warn("Failed to parse 'makedeps' string for solvable %d: %s", p, error.text);
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
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "tar xjf \"%s\" -C \"%s\" 2>/dev/null", tar_path, tmp_dir);
    int extract_ret = system(cmd);
    if (extract_ret != 0) {
        log_error("Failed to extract %s (exit %d)", tar_path, extract_ret);
        goto cleanup;
    }
    log_info("Extracted %s to %s successfully", tar_path, tmp_dir);

    // Compute SHA256 of tar file for metadata
    char *computed_sha = compute_file_sha256(tar_path);
    if (computed_sha) {
        log_info("Computed SHA256 for %s: %s", tar_path, computed_sha);
    }

    // Načti index.json rekurzivně
    json_t* index = find_and_load_index(tmp_dir);
    if (!index) {
        log_warn("No index.json in %s – generating default", tar_path);
        char *basename = strrchr(tar_path, '/') ? strrchr(tar_path, '/') + 1 : (char *)tar_path;
        index = generate_default_metadata(basename);
        if (!index) {
            if (computed_sha) free(computed_sha);
            goto cleanup;
        }
    }

    // Inject computed SHA256 into index if missing
    if (computed_sha) {
        json_t *existing_sha = json_object_get(index, "sha256");
        if (!existing_sha || !json_is_string(existing_sha) || 
            strlen(json_string_value(existing_sha)) == 0) {
            json_object_set_new(index, "sha256", json_string(computed_sha));
            log_info("Injected SHA256 into metadata for %s", tar_path);
        }
        free(computed_sha);
    }

    // Zpracuj (repo_add_conda + custom)
    p = process_index_json(index, NULL, pool, repo, tar_path);
    json_decref(index);

    if (p != 0) {
        // Finalize the repository state
        repo_internalize(repo);
        pool_createwhatprovides(pool);
        log_info("Added %s as solvable %d", tar_path, p);
    } else {
        log_error("Failed to add %s", tar_path);
    }

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