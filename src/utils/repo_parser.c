//
// Created by placeja5 on 11/4/25.
//

#include "includes/repo_parser.h"

// Function to load JSON from file
json_t* load_json(const char* filepath) {
    json_error_t error;
    json_t* root = json_load_file(filepath, 0, &error);
    if (!root) {
        fprintf(stderr, "Error loading JSON from %s: %s\n", filepath, error.text);
        return NULL;
    }
    return root;
}

// Function to generate package key: "name-version-build.tar.bz2"
char* generate_package_key(json_t* index) {
    const char* name = json_string_value(json_object_get(index, "name"));
    const char* version = json_string_value(json_object_get(index, "version"));
    const char* build = json_string_value(json_object_get(index, "build"));
    if (!name || !version || !build) {
        return NULL;
    }
    size_t len = strlen(name) + strlen(version) + strlen(build) + 12;  // + for -, .tar.bz2, null
    char* key = malloc(len);
    if (key) {
        snprintf(key, len, "%s-%s-%s.tar.bz2", name, version, build);
    }
    return key;
}

// Recursive function to traverse directory and collect index.json files
void collect_packages(const char* root_path, json_t* packages, Pool *pool, Repo *repo) {
    DIR* dir = opendir(root_path);
    if (!dir) {
        log_error("Error opening directory %s\n", root_path);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char subpath[1024];
            snprintf(subpath, sizeof(subpath), "%s/%s", root_path, entry->d_name);
            collect_packages(subpath, packages, pool, repo);
        } else if (strcmp(entry->d_name, "index.json") == 0) {
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", root_path, entry->d_name);
            json_t* index = load_json(filepath);
            if (index) {
                // Add required fields if missing (defaults for compatibility)
                if (!json_object_get(index, "build_number")) {
                    json_object_set_nocheck(index, "build_number", json_integer(0));
                }
                if (!json_object_get(index, "license")) {
                    json_object_set_nocheck(index, "license", json_string("unknown"));
                }
                if (!json_object_get(index, "md5")) {
                    json_object_set_nocheck(index, "md5", json_string(""));  // Placeholder; compute if needed
                }
                if (!json_object_get(index, "sha256")) {
                    json_object_set_nocheck(index, "sha256", json_string(""));  // Placeholder
                }
                if (!json_object_get(index, "size")) {
                    json_object_set_nocheck(index, "size", json_integer(0));  // Placeholder
                }
                if (!json_object_get(index, "subdir")) {
                    json_object_set_nocheck(index, "subdir", json_string("noarch"));
                }
                if (!json_object_get(index, "timestamp")) {
                    time_t now = time(NULL);
                    json_object_set_nocheck(index, "timestamp", json_integer((json_int_t)now * 1000));  // ms
                }

                if (pool && repo) {  // Pokud pool/repo poskytnuty (z load_repository v parser.c)
                    Id p = repo_add_solvable(repo);  // Získej offset
                    if (p == 0) {  // Chyba? (0 je typicky neplatný Id)
                        log_error("Error during handing out the solvable.\n");
                        json_decref(index);
                        continue;
                    }

                    Solvable *s = pool->solvables + p;  // Získej pointer

                    if (s) {
                        // Nastav základní pole z index.json
                        const char *name = json_string_value(json_object_get(index, "name"));
                        if (name) s->name = pool_str2id(pool, name, 1);

                        const char *version = json_string_value(json_object_get(index, "version"));
                        if (version) {
                            const char *build = json_string_value(json_object_get(index, "build"));
                            char *evr = NULL;
                            if (build) {
                                asprintf(&evr, "%s-%s", version, build);
                            } else {
                                evr = strdup(version);
                            }
                            if (evr) {
                                s->evr = pool_str2id(pool, evr, 1);
                                free(evr);  // Cleanup
                            }
                        }

                        // SPECIFICKY PRO "ARCH": Načti a nastav do Solvable
                        json_t *json_arch = json_object_get(index, "arch");
                        if (json_arch && json_is_string(json_arch)) {
                            const char *arch_str = json_string_value(json_arch);
                            s->arch = pool_str2id(pool, arch_str, 1);  // SPRÁVNÝ ZPŮSOB - přímé nastavení
                            log_debug(1, "Set arch for %s: %s", name ? name : "unknown", arch_str);
                        } else {
                            // Fallback na get_pool_arch
                            const char *default_arch = get_pool_arch(pool);
                            s->arch = pool_str2id(pool, default_arch, 1);
                            log_warn("No 'arch' in index.json for %s; using default: %s", name ? name : "unknown", default_arch);
                        }

                        // Přidej Repodata pro další metadata
                        Repodata *data = repo_add_repodata(repo, 0);
                        if (data) {
                            // Další custom pole (pro náš LFS/LLD workflow)
                            json_t *mkdeps = json_object_get(index, "mkdeps");
                            if (mkdeps && json_is_array(mkdeps)) {
                                // Ulož jako string v repodata (nelze ukládat json_t přímo)
                                char *mkdeps_str = json_dumps(mkdeps, JSON_COMPACT);
                                if (mkdeps_str) {
                                    repodata_set_str(data, p, pool_str2id(pool, "makedeps", 1), mkdeps_str);
                                    free(mkdeps_str);
                                }
                                log_info("Loaded mkdeps for %s: %d items", name ? name : "unknown", (int)json_array_size(mkdeps));
                            }

                            json_t *source = json_object_get(index, "source");
                            if (source && json_is_string(source)) {
                                // URL pro download
                                repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), json_string_value(source));
                            }

                            json_t *extras = json_object_get(index, "extras");
                            if (extras && json_is_array(extras)) {
                                // Ulož jako string v repodata
                                char *extras_str = json_dumps(extras, JSON_COMPACT);
                                if (extras_str) {
                                    repodata_set_str(data, p, pool_str2id(pool, "extras", 1), extras_str);
                                    free(extras_str);
                                }
                            }

                            // Přidej další standardní metadata
                            const char *license = json_string_value(json_object_get(index, "license"));
                            if (license) {
                                repodata_set_str(data, p, SOLVABLE_LICENSE, license);
                            }

                            const char *summary = json_string_value(json_object_get(index, "summary"));
                            if (summary) {
                                repodata_set_str(data, p, SOLVABLE_SUMMARY, summary);
                            }
                        }

                        // POZOR: NEPŘIDÁVEJ znovu - s už je v repo díky repo_add_solvable()
                        // repo_add_solvable(repo, s);  // TOTO ODSTRAN - způsobuje duplikaci

                    } else {
                        log_warn("Failed to create Solvable for %s", filepath);
                    }
                }

                char* key = generate_package_key(index);
                if (key) {
                    json_object_set_nocheck(packages, key, index);
                    free(key);  // SPRÁVNĚ - memory je uvolněna
                } else {
                    json_decref(index);  // Uvolni index pokud key selhal
                }
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
    json_object_set_nocheck(info, "subdir", json_string("noarch"));  // Adjust as needed
    json_object_set_nocheck(info, "platform", json_string("linux"));
    json_object_set_nocheck(info, "arch", json_string("x86_64"));
    json_object_set_nocheck(repodata, "info", info);

    json_t* packages = json_object();
    collect_packages(repo_path, packages, pool, repo);
    json_object_set_nocheck(repodata, "packages", packages);

    // Optional: Add "packages.conda" if using .conda format; here assuming .tar.bz2 only
    json_t* packages_conda = json_object();
    json_object_set_nocheck(repodata, "packages.conda", packages_conda);

    // Write to repodata.json
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/repodata.json", repo_path);
    if (json_dump_file(repodata, output_path, JSON_INDENT(4)) != 0) {
        log_error("Error writing repodata.json\n");
        json_decref(repodata);
        return 1;
    }

    json_decref(repodata);
    printf("repodata.json generated at %s\n", output_path);
    return 0;
}

