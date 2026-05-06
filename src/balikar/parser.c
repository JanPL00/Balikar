#include "includes/parser.h"

/**
 * Ořízne mezery na začátku a konci řetězce.
 *
 * Vytvoří nový řetězec bez mezer na obou koncích.
 * Volající musí vrácený řetězec uvolnit.
 *
 * @param str Vstupní řetězec
 * @return Nový oříznutý řetězec nebo NULL při chybě
 */
static char* trim_str(const char* str) {
    if (!str) return NULL;

    // Přeskoč úvodní bílé znaky
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return strdup("");  // Prázdný řetězec

    // Najdi konec a ořízni koncové bílé znaky
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Vytvoř nový řetězec s oříznutým obsahem
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

/**
 * Načte JSON ze souboru bez logování chyb.
 *
 * Tiše vrátí NULL pokud soubor neexistuje nebo obsahuje chyby parsování.
 * Používá se pro volitelné kontroly souborů, kde absence je normální.
 *
 * @param filepath Cesta k JSON souboru
 * @return Parsovaný JSON objekt nebo NULL
 */
static json_t* load_json_quiet(const char* filepath) {
    json_error_t error;
    json_t* root = json_load_file(filepath, 0, &error);
    return root;  // NULL při selhání, bez logování
}

/**
 * Načte JSON ze souboru s logováním chyb.
 *
 * Parsuje JSON soubor a loguje chyby při selhání načtení.
 * Volající musí vrácený JSON objekt uvolnit pomocí json_decref.
 *
 * @param filepath Cesta k JSON souboru
 * @return Parsovaný JSON objekt nebo NULL při chybě
 */
json_t* load_json(const char* filepath) {
    json_error_t error;
    json_t* root = json_load_file(filepath, 0, &error);
    if (!root) {
        log_error("Error loading JSON from %s: %s", filepath, error.text);
        return NULL;
    }
    return root;
}

/**
 * Rekurzivně najde a načte první soubor index.json v adresáři.
 *
 * Prohledá adresářový strom pro index.json, kontroluje kořenový adresář
 * i podadresář info/. Používá se pro struktury tar.bz2 s vnořenými podadresáři.
 *
 * @param dir_path Adresář k prohledání
 * @return Parsovaný JSON objekt nebo NULL pokud nenalezeno
 */
static json_t* find_and_load_index(const char* dir_path) {
    if (!dir_path) return NULL;

    DIR* d = opendir(dir_path);
    if (!d) {
        log_debug(2, "Cannot open dir for index search: %s", dir_path);
        return NULL;
    }

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        // Přeskoč . a ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[2048];  // Zvětšeno pro bezpečnost
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Nejprve zkus přímo v tomto podadresáři (vzor root/info)
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

            // Pokud nenalezeno, pokračuj v rekurzivním vyhledávání
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

/**
 * Načte JSON z adresářové struktury.
 *
 * Nejprve zkusí najít index.json v kořenovém adresáři, pak v podadresáři info/.
 * Používá se pro balíky jako adresář (ne tar archivy).
 *
 * @param dir_path Cesta k adresáři k prohledání
 * @return Parsovaný JSON objekt nebo NULL pokud nenalezeno
 */
static json_t* load_json_from_directory(const char* dir_path) {
    char index_path[2048];

    // Nejprve zkus kořenový adresář (tiše – bez logování chyb)
    snprintf(index_path, sizeof(index_path), "%s/index.json", dir_path);
    json_t* index = load_json_quiet(index_path);

    if (!index) {
        // Zkus podadresář info/
        snprintf(index_path, sizeof(index_path), "%s/info/index.json", dir_path);
        index = load_json_quiet(index_path);
    }

    return index;
}

/**
 * Doplní výchozí pole do JSON metadat balíku pokud chybí.
 *
 * Zajistí, že povinná pole (build_number, license, md5, sha256, size, subdir, timestamp)
 * jsou přítomna v metadatech balíku s rozumnými výchozími hodnotami.
 *
 * @param index JSON objekt k doplnění výchozími hodnotami
 */
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

/**
 * Najde solvable podle názvu a volitelně podle verze.
 *
 * Prohledá repozitář pro balík odpovídající zadanému názvu a verzi.
 * Pokud je version NULL, vrátí první shodu pouze podle názvu.
 * Používá se pro párování solvables po repo_add_conda.
 *
 * @param pool Pool balíků
 * @param repo Repozitář k prohledání
 * @param name Název balíku k nalezení
 * @param version Verze balíku ke shodě (NULL pro libovolnou verzi)
 * @return ID solvable nebo 0 pokud nenalezeno
 */
static Id find_solvable_by_name_version(Pool *pool, Repo *repo, const char *name, const char *version) {
    if (!pool || !repo || !name) return 0;

    Id name_id = pool_str2id(pool, name, 0);
    if (!name_id) return 0;

    Solvable *s;
    Id p;
    FOR_REPO_SOLVABLES(repo, p, s) {
        if (s->name == name_id) {
            // Pokud není zadána verze, vrať první shodu
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

/**
 * Přidá vlastní metadata pole do repodata pro solvable.
 *
 * Extrahuje vlastní pole z JSON balíku (makedeps, pole source, download_url,
 * extras, is_group, no_package) a uloží je do libsolv repodata pro pozdější načtení.
 * Obsahuje záložní zpracování pro starší formáty source.
 *
 * @param index JSON metadata balíku
 * @param data Repodata, do které se pole přidávají
 * @param p ID solvable
 * @param pool Pool balíků
 * @param fallback_source Záložní URL ke stažení pokud není v JSON
 */
static void add_custom_fields_to_repodata(json_t *index, Repodata *data, Id p, Pool *pool, const char* fallback_source) {
    if (!index || !data) return;

    // Ulož makedeps jako JSON řetězec pro řešení závislostí při sestavení
    json_t *mkdeps = json_object_get(index, "mkdeps");
    if (mkdeps && json_is_array(mkdeps)) {
        char *str = json_dumps(mkdeps, JSON_COMPACT);
        if (str) {
            repodata_set_str(data, p, pool_str2id(pool, "makedeps", 1), str);
            free(str);
        }
    }

    // Ulož pole zdrojů (nový formát) jako JSON řetězec pro podporu více zdrojů
    json_t *source_array = json_object_get(index, "source");
    if (source_array && json_is_array(source_array)) {
        char *str = json_dumps(source_array, JSON_COMPACT);
        if (str) {
            repodata_set_str(data, p, pool_str2id(pool, "source", 1), str);
            free(str);
            log_debug(1, "Set source array for solvable %d", p);
        }
    }

    // Ulož download_url s řetězcem zálohy: explicitní URL -> starý zdroj -> záloha
    json_t *download_url_json = json_object_get(index, "download_url");
    int source_set = 0;
    if (download_url_json && json_is_string(download_url_json)) {
        const char* download_val = json_string_value(download_url_json);
        if (download_val && download_val[0] != '\0') {
            repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), download_val);
            source_set = 1;
        }
    }
    // Zpětná kompatibilita: podpora starého formátu kde zdroj byl řetězec místo pole
    if (!source_set && source_array && json_is_string(source_array)) {
        const char* source_val = json_string_value(source_array);
        if (source_val && source_val[0] != '\0') {
            repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), source_val);
            source_set = 1;
        }
    }
    if (!source_set && fallback_source) {
        repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), fallback_source);
        log_info("Set fallback download_url: %s", fallback_source);
    }

    // Doplňky
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

    // Vytvoř hlubokou kopii pro konverzi depends -> requires
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
    json_decref(index_copy); // index_copy je nyní vlastněno tmp_packages

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

        // repo_add_conda měl přidat requires z JSON, ale pokud nefungovalo,
        // přidej je ručně z původního pole depends
        json_t *original_depends = json_object_get(index, "depends");
        if (original_depends && json_is_array(original_depends)) {
            size_t num_deps = json_array_size(original_depends);
            if (num_deps > 0) {
                // Použij libsolv Queue pro ID závislostí
                Queue q;
                queue_init(&q);

                for (size_t i = 0; i < num_deps; i++) {
                    json_t *dep_json = json_array_get(original_depends, i);
                    if (json_is_string(dep_json)) {
                        const char *dep_str = json_string_value(dep_json);
                        // Vytvoř ID pro název závislosti
                        Id dep_name_id = pool_str2id(pool, dep_str, 1);
                        queue_push(&q, dep_name_id);
                    }
                }

                if (q.count > 0) {
                    // Přidej pole závislostí z fronty do repodata
                    repodata_set_idarray(data, p, SOLVABLE_REQUIRES, &q);
                    log_info("Manually set %d requires for solvable %d", q.count, p);
                }

                // Uvolni paměť fronty
                queue_free(&q);
            }
        }

        // Zajisti že solvable poskytuje sám sebe (self-provide) – kritické pro solver
        // Přidej do repodata před internalize
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

/**
 * Generate default package metadata from filename.
 * 
 * Parses filename (name-version-build.tar.bz2 format) to extract package
 * information and creates minimal metadata when index.json is missing.
 * Used as fallback for packages without proper metadata.
 * 
 * @param filename Package filename to parse
 * @return JSON object with default metadata or NULL on error
 */
json_t* generate_default_metadata(const char* filename) {
    if (!filename) return NULL;

    json_t* index = json_object();
    if (!index) return NULL;

    // Získej basename
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;

    // Extrahuj název souboru bez cesty a přípony .tar.bz2
    char name_buf[256] = {0};
    strncpy(name_buf, basename, sizeof(name_buf) - 1);
    char *ext = strstr(name_buf, ".tar.bz2");
    if (ext) *ext = '\0';

    // Parsuj: name-version-build (najdi POSLEDNÍ dvě pomlčky)
    char *last_dash = strrchr(name_buf, '-');
    char *second_last_dash = NULL;
    if (last_dash) {
        *last_dash = '\0';
        second_last_dash = strrchr(name_buf, '-');
        *last_dash = '-';  // Dočasně obnov
    }

    char name[256] = {0}, version[256] = {0}, build[256] = {0};

    if (second_last_dash && last_dash) {
        // Nalezeno name-version-build
        size_t name_len = second_last_dash - name_buf;
        strncpy(name, name_buf, name_len);
        name[name_len] = '\0';

        size_t version_len = last_dash - second_last_dash - 1;
        strncpy(version, second_last_dash + 1, version_len);
        version[version_len] = '\0';

        strcpy(build, last_dash + 1);
    } else if (last_dash) {
        // Pouze name-version
        size_t name_len = last_dash - name_buf;
        strncpy(name, name_buf, name_len);
        name[name_len] = '\0';
        strcpy(version, last_dash + 1);
        strcpy(build, "0");
    } else {
        // Pouze name
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

/**
 * Rekurzivně shromáždí balíky z adresářové struktury.
 *
 * Prochází adresářový strom a hledá balíky (adresáře s index.json
 * nebo archivy .tar.bz2). Zpracuje každý balík a přidá do repozitáře.
 * Pokud je to možné, vypočítá SHA256 pro balíky.
 *
 * @param current_path Aktuálně zpracovávaná cesta adresáře
 * @param packages JSON objekt pro shromáždění metadat balíků
 * @param pool Pool balíků
 * @param repo Repozitář pro přidání balíků
 */
static void collect_packages_recursive(const char* current_path, json_t* packages, Pool *pool, Repo *repo) {
    DIR* dir = opendir(current_path);
    if (!dir) {
        log_debug(1, "Cannot open directory: %s", current_path);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Přeskoč . a ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Zkus načíst index.json z tohoto adresáře
            json_t* index = load_json_from_directory(full_path);
            if (index) {
                // Nalezen balík s index.json
                log_info("Found package at: %s", full_path);
                process_index_json(index, packages, pool, repo, NULL);
                json_decref(index);
            } else {
                // Rekurzivně prohledej podadresáře
                collect_packages_recursive(full_path, packages, pool, repo);
            }
        } else if (entry->d_type == DT_REG && strstr(entry->d_name, ".tar.bz2")) {
            // Zpracuj soubor .tar.bz2
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

            json_t* index = find_and_load_index(tmp_dir);
            if (!index) {
                log_warn("No index.json in %s – generating default", full_path);
                index = generate_default_metadata(entry->d_name);
            }

            process_index_json(index, packages, pool, repo, full_path);  // fallback_source = záložní zdroj

            json_decref(index);

            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
            system(cmd);
        }
    }
    closedir(dir);
}

/**
 * Shromáždí balíky z cesty repozitáře.
 *
 * Obalová funkce spouštějící rekurzivní shromažďování balíků z kořene repozitáře.
 * Používá se při generování repodata pro zjištění všech balíků.
 *
 * @param repo_path Kořenová cesta repozitáře
 * @param packages JSON objekt pro shromáždění metadat balíků
 * @param pool Pool balíků (může být NULL pro shromažďování pouze metadat)
 * @param repo Repozitář (může být NULL pro shromažďování pouze metadat)
 */
void collect_packages(const char* repo_path, json_t* packages, Pool *pool, Repo *repo) {
    log_info("Collecting packages from: %s", repo_path);
    collect_packages_recursive(repo_path, packages, pool, repo);
}

/**
 * Generate repodata.json file for repository.
 * 
 * Main function that creates repository metadata file by collecting all packages
 * and writing consolidated metadata. Used for local repository management.
 * File creation only - adding to pool/repo is handled separately.
 * 
 * @param repo_path Repository root path
 * @return 0 on success, non-zero on error
 */
int generate_repodata(const char* repo_path) {
    json_t* repodata = json_object();
    if (!repodata) return 1;

    json_t* info = json_object();
    json_object_set_nocheck(info, "subdir", json_string("noarch"));
    json_object_set_nocheck(info, "platform", json_string("linux"));
    json_object_set_nocheck(info, "arch", json_string("x86_64"));
    json_object_set_nocheck(repodata, "info", info);

    json_t* packages = json_object();
    collect_packages(repo_path, packages, NULL, NULL);  // Pouze pro balíky, bez pool/repo
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

/**
 * Zkontroluje zda jsou soubory index.json novější než repodata.json.
 *
 * Rekurzivně kontroluje časy změn pro zjištění zda je nutná regenerace
 * metadat repozitáře. Používá se pro přírůstkové aktualizace.
 *
 * @param repo_path Cesta repozitáře ke kontrole
 * @param repodata_mtime Čas změny aktuálního repodata.json
 * @return 1 pokud byly nalezeny novější soubory, 0 jinak
 */
static int check_index_newer_than_repodata(const char *repo_path, time_t repodata_mtime) {
    DIR *dir = opendir(repo_path);
    if (!dir) return 0;
    
    struct dirent *entry;
    int found_newer = 0;
    
    while ((entry = readdir(dir)) != NULL && !found_newer) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", repo_path, entry->d_name);
        
        if (entry->d_type == DT_DIR) {
            // Zkontroluj zda je index.json v tomto adresáři
            char index_paths[3][2048];
            snprintf(index_paths[0], sizeof(index_paths[0]), "%s/index.json", full_path);
            snprintf(index_paths[1], sizeof(index_paths[1]), "%s/info/index.json", full_path);
            
            for (int i = 0; i < 2; i++) {
                struct stat index_st;
                if (stat(index_paths[i], &index_st) == 0) {
                    if (index_st.st_mtime > repodata_mtime) {
                        log_info("Found newer index.json: %s", index_paths[i]);
                        found_newer = 1;
                        break;
                    }
                }
            }
            
            // Rekurzivně zkontroluj podadresáře
            if (!found_newer && check_index_newer_than_repodata(full_path, repodata_mtime)) {
                found_newer = 1;
            }
        }
    }
    
    closedir(dir);
    return found_newer;
}

/**
 * Download repodata.json from remote repository URL.
 * 
 * Uses libcurl to fetch repository metadata from HTTP/HTTPS URL.
 * Includes error handling and progress indication.
 * 
 * @param repo_url Base repository URL
 * @param dest_path Destination file path for downloaded metadata
 * @return 0 on success, -1 on error
 */
static int download_repodata(const char *repo_url, const char *dest_path) {
    char url[4096];
    snprintf(url, sizeof(url), "%s/repodata.json", repo_url);
    
    log_info("Downloading repodata.json from: %s", url);
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        log_error("Failed to initialize curl");
        return -1;
    }
    
    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        log_error("Failed to open %s for writing", dest_path);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        log_error("Failed to download repodata.json: %s", curl_easy_strerror(res));
        unlink(dest_path);
        return -1;
    }
    
    log_info("Successfully downloaded repodata.json");
    return 0;
}

/**
 * Zkontroluje zda je cesta vzdálená URL.
 *
 * Jednoduché ověření předpon http:// nebo https://.
 *
 * @param path Cesta nebo URL ke kontrole
 * @return 1 pokud je vzdálená URL, 0 pokud je lokální cesta
 */
static int is_remote_url(const char *path) {
    return (strncmp(path, "http://", 7) == 0 || 
            strncmp(path, "https://", 8) == 0);
}

/**
 * Load repository into pool.
 * 
 * Main repository loading function that:
 * 1. Sets system architecture
 * 2. Downloads/generates repodata.json as needed
 * 3. Calls repo_add_conda to parse packages
 * 4. Adds custom metadata fields
 * 5. Ensures all solvables have self-provides
 * 6. Creates whatprovides index
 * 
 * Handles both local and remote repositories.
 * 
 * @param pool Package pool
 * @param repo_path Repository path or URL
 * @return Repository object or NULL on error
 */
Repo *load_repository(Pool *pool, const char *repo_path) {
    if (!pool || !repo_path) return NULL;

    // Nastav arch před načtením jakýchkoliv repozitářů
    set_system_arch(pool);

    char repodata_path[2048];
    int is_remote = is_remote_url(repo_path);
    
    if (is_remote) {
        // Vzdálený repozitář: stáhni do umístění cache
        snprintf(repodata_path, sizeof(repodata_path), "/var/cache/balikar/downloads/repodata.json");
        
        // Stáhni repodata.json pouze pokud neexistuje nebo je starší než 1 hodinu.
        // Příkazy jako remove/autoremove nepotřebují čerstvou kopii – pouze používají
        // pool/repo pro vyhledávání v grafu závislostí, který se nemění často.
        int needs_download = 1;
        struct stat cache_st;
        if (stat(repodata_path, &cache_st) == 0) {
            time_t age = time(NULL) - cache_st.st_mtime;
            if (age < 3600) {
                log_debug(1, "Using cached repodata.json (age: %lds < 1h)", (long)age);
                needs_download = 0;
            }
        }
        
        if (needs_download) {
            if (download_repodata(repo_path, repodata_path) != 0) {
                // Pokud stahování selže ale cache existuje, použij záložní verzi z cache
                if (access(repodata_path, F_OK) == 0) {
                    log_warn("Download failed, using existing cached repodata.json");
                } else {
                    log_error("Failed to download repodata.json from remote repository");
                    return NULL;
                }
            }
        }
    } else {
        // Lokální repozitář
        snprintf(repodata_path, sizeof(repodata_path), "%s/repodata.json", repo_path);
        
        struct stat st;
        int needs_regen = 0;
        
        if (stat(repodata_path, &st) != 0) {
            log_info("No repodata.json – generating...");
            needs_regen = 1;
        } else {
            // Zkontroluj zda je nějaký index.json novější než repodata.json
            if (check_index_newer_than_repodata(repo_path, st.st_mtime)) {
                log_info("Detected changes in index.json files – regenerating repodata.json...");
                needs_regen = 1;
            }
        }
        
        if (needs_regen) {
            if (generate_repodata(repo_path) != 0) return NULL;
        }
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

    // Načti JSON znovu pro přidání vlastních polí která repo_add_conda ignoruje
    json_error_t error;
    json_t *root = json_load_file(repodata_path, 0, &error);
    if (root) {
        json_t *packages = json_object_get(root, "packages");
        if (packages && json_is_object(packages)) {
            // Použij hlavní repodata z repo (vytvořenou repo_add_conda)
            // Je nutné přidat pole před internalize
            Repodata *data = repo_id2repodata(repo, 1);  // ID 1 je typicky hlavní repodata
            if (!data) {
                log_warn("No repodata found, cannot add custom fields");
                json_decref(root);
            } else {
            
            // Iteruj přes všechny solvable a přidej vlastní pole
            Id p;
            Solvable *s;
            FOR_REPO_SOLVABLES(repo, p, s) {
                if (!s->name) continue;
                
                const char *name = pool_id2str(pool, s->name);
                const char *evr = pool_id2str(pool, s->evr);
                
                                log_debug(2, "Looking for custom fields for solvable %d: %s-%s", p, name, evr);
                
                // Najdi odpovídající balík v JSON
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
                        
                        // Přidej pole zdrojů (nový formát)
                        json_t *source_array = json_object_get(pkg_json, "source");
                        if (source_array && json_is_array(source_array)) {
                            char *str = json_dumps(source_array, JSON_COMPACT);
                            if (str) {
                                repodata_set_str(data, p, pool_str2id(pool, "source", 1), str);
                                free(str);
                                log_debug(1, "Set source array for %s", name);
                            }
                        }
                        
                        // Přidej download_url (pro zpětnou kompatibilitu nebo samostatné použití)
                        json_t *download_url = json_object_get(pkg_json, "download_url");
                        if (download_url && json_is_string(download_url)) {
                            const char *download_val = json_string_value(download_url);
                            log_debug(1, "Set download_url for %s to: %s", name, download_val);
                            repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), download_val);
                        }
                        // Starý formát: zdroj jako řetězec (starý formát)
                        else if (source_array && json_is_string(source_array)) {
                            const char *source_val = json_string_value(source_array);
                            repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), source_val);
                            log_debug(1, "Set download_url (legacy) for %s to: %s", name, source_val);
                        }
                        
                        // Přidej mkdeps
                
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
                        
                        // Přidej extras
                        json_t *extras = json_object_get(pkg_json, "extras");
                        if (extras && json_is_array(extras)) {
                            char *str = json_dumps(extras, JSON_COMPACT);
                            if (str) {
                                repodata_set_str(data, p, pool_str2id(pool, "extras", 1), str);
                                free(str);
                            }
                        }
                        
                        // Přidej is_group
                        json_t *is_group = json_object_get(pkg_json, "is_group");
                        if (is_group && json_is_boolean(is_group)) {
                            repodata_set_str(data, p, pool_str2id(pool, "is_group", 1),
                                           json_is_true(is_group) ? "true" : "false");
                        }
                        
                        // Přidej no_package
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
    
    // Zajisti že všechny solvable mají self-provides (kritické pro solver)
    // Musí být provedeno PO internalize ale PŘED pool_createwhatprovides
    // repo_add_conda někdy nastavuje provides na neplatné hodnoty (jako 1), proto přepiš
    Id p;
    Solvable *s;
    FOR_REPO_SOLVABLES(repo, p, s) {
        if (s->name) {
            // Vždy nastav self-provide, i pokud provides není nula (může být neplatné)
            s->provides = repo_addid_dep(repo, 0, s->name, 0);
            log_debug(1, "Set self-provide for solvable %d (%s)", p, pool_id2str(pool, s->name));
            
            // Oprav arch pokud není správně nastaveno – použij noarch pro kompatibilitu
            // balíky noarch by měly být instalovatelné na jakékoliv architektuře
            if (!s->arch || s->arch == 0 || s->arch == 1) {
                s->arch = pool_str2id(pool, "noarch", 1);
                log_debug(1, "Set arch for solvable %d to noarch", p);
            }
        }
    }
    
    // Vytvoř index whatprovides PO přidání všech provides
    pool_createwhatprovides(pool);

    log_info("Repository loaded from %s", repodata_path);
    return repo;
}

/**
 * Vyhledá vlastní metadata makedeps z repodata.
 *
 * Načte závislosti pro sestavení uložené jako JSON řetězec v repodata.
 * Volající musí vrácené JSON pole uvolnit pomocí decref.
 *
 * @param data Repodata obsahující metadata
 * @param p ID solvable
 * @param pool Pool balíků
 * @return JSON pole makedeps nebo NULL pokud nenalezeno
 */
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

/**
 * Vyhledá vlastní metadata URL ke stažení z repodata.
 *
 * Načte pole download_url uložené v repodata.
 * Řetězec vlastní libsolv – neuvolňovat.
 *
 * @param data Repodata obsahující metadata
 * @param p ID solvable
 * @param pool Pool balíků
 * @return Řetězec URL ke stažení nebo NULL pokud nenalezeno
 */
const char* get_custom_source(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "download_url", 0);
    const char *source_url = repodata_lookup_str(data, p, key);

    if (!source_url) {
        log_warn("No 'download_url' found for solvable %d", p);
        return NULL;
    }

    return source_url;
}

/**
 * Vyhledá vlastní metadata pole zdrojů z repodata.
 *
 * Načte pole zdrojů (nový formát) uložené jako JSON řetězec v repodata.
 * Podporuje více zdrojových URL se součty. Volající musí provést decref.
 *
 * @param data Repodata obsahující metadata
 * @param p ID solvable
 * @param pool Pool balíků
 * @return JSON pole zdrojů nebo NULL pokud nenalezeno
 */
json_t* get_custom_source_array(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "source", 0);
    const char *source_str = repodata_lookup_str(data, p, key);

    if (!source_str) {
        log_debug(2, "No 'source' array found for solvable %d", p);
        return NULL;
    }

    json_error_t error;
    json_t *source = json_loads(source_str, 0, &error);
    if (!source || !json_is_array(source)) {
        log_warn("Failed to parse 'source' string for solvable %d: %s", p, error.text);
        if (source) json_decref(source);
        return NULL;
    }

    return source;
}

/**
 * Vyhledá vlastní metadata extras z repodata.
 *
 * Načte extras (záplaty, doplňkové soubory) uložené jako JSON řetězec.
 * Volající musí vrácené JSON pole uvolnit pomocí decref.
 *
 * @param data Repodata obsahující metadata
 * @param p ID solvable
 * @param pool Pool balíků
 * @return JSON pole extras nebo NULL pokud nenalezeno
 */
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

/**
 * Vyhledá vlastní příznak is_group z repodata.
 *
 * Zkontroluje zda je balík meta-balík (skupina) poskytující pouze data
 * bez skutečného sestavení. Vrátí booleovskou hodnotu.
 *
 * @param data Repodata obsahující metadata
 * @param p ID solvable
 * @param pool Pool balíků
 * @return 1 pokud je skupina, 0 jinak
 */
int get_custom_is_group(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "is_group", 0);
    const char *str = repodata_lookup_str(data, p, key);

    if (!str) {
        log_warn("No 'is_group' found for solvable %d - assuming false", p);
        return 0;
    }

    return strcmp(str, "true") == 0 ? 1 : 0;
}

/**
 * Vyhledá vlastní příznak no_package z repodata.
 *
 * Zkontroluje zda je balík virtuální (žádný skutečný soubor k sestavení/instalaci).
 * Používá se pro balíky sloužící pouze jako závislosti nebo meta-balíky.
 *
 * @param data Repodata obsahující metadata
 * @param p ID solvable
 * @param pool Pool balíků
 * @return 1 pokud je virtuální, 0 jinak
 */
int get_custom_no_package(Repodata *data, Id p, Pool *pool) {
    Id key = pool_str2id(pool, "no_package", 0);
    const char *str = repodata_lookup_str(data, p, key);

    if (!str) {
        log_warn("No 'no_package' found for solvable %d - assuming false", p);
        return 0;
    }

    return strcmp(str, "true") == 0 ? 1 : 0;
}

/**
 * Internalizuje repozitář pro efektivní vyhledávání.
 *
 * Volá repo_internalize z libsolv pro vytvoření interních indexů.
 * Musí být voláno po přidání balíků a před dotazováním metadat.
 *
 * @param repo Repozitář k internalizaci
 */
void internalize_repo(Repo *repo) {
    repo_internalize(repo);
    log_info("Repository internalized for lookup");
}

/**
 * Přidá soubor balíku tar.bz2 do repozitáře.
 *
 * Rozbalí archiv balíku, načte/vygeneruje metadata, vypočítá SHA256
 * a přidá do repozitáře pomocí process_index_json. Používá se pro
 * lokální instalaci balíků.
 *
 * @param tar_path Cesta k souboru balíku .tar.bz2
 * @param pool Pool balíků
 * @param repo Repozitář pro přidání balíku
 * @return ID solvable nebo 0 při chybě
 */
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
    // Rozbal archiv tar.bz2 do dočasného adresáře
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "tar xjf \"%s\" -C \"%s\" 2>/dev/null", tar_path, tmp_dir);
    int extract_ret = system(cmd);
    if (extract_ret != 0) {
        log_error("Failed to extract %s (exit %d)", tar_path, extract_ret);
        goto cleanup;
    }
    log_info("Extracted %s to %s successfully", tar_path, tmp_dir);

    // Načti index.json rekurzivně z rozbaleného adresáře
    json_t* index = find_and_load_index(tmp_dir);
    if (!index) {
        log_warn("No index.json in %s – generating default", tar_path);
        char *basename = strrchr(tar_path, '/') ? strrchr(tar_path, '/') + 1 : (char *)tar_path;
        index = generate_default_metadata(basename);
        if (!index) {
            goto cleanup;
        }
    }

    // Zpracuj (repo_add_conda + vlastní)
    p = process_index_json(index, NULL, pool, repo, tar_path);
    json_decref(index);

    if (p != 0) {
        // Finalizuj stav repozitáře
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

/**
 * Update repository metadata file.
 * 
 * For remote repositories: downloads fresh repodata.json from URL.
 * For local repositories: regenerates repodata.json from index.json files.
 * Used to refresh repository state.
 * 
 * @param repo_path Repository path or URL
 * @return 0 on success, -1 on error
 */
int update_repository_metadata(const char *repo_path) {
    if (!repo_path) {
        log_error("Repository path is NULL");
        return -1;
    }
    
    int is_remote = is_remote_url(repo_path);
    
    if (is_remote) {
        // Vzdálený repozitář: stáhni čerstvý repodata.json
        char repodata_path[2048];
        snprintf(repodata_path, sizeof(repodata_path), "/var/cache/balikar/downloads/repodata.json");
        
        log_info("Updating remote repository metadata from: %s", repo_path);
        
        if (download_repodata(repo_path, repodata_path) != 0) {
            log_error("Failed to download repodata.json from remote repository");
            return -1;
        }
        
        log_info("Remote repository metadata updated successfully");
        log_info("Cached at: %s", repodata_path);
        return 0;
        
    } else {
        // Lokální repozitář: regeneruj ze souborů index.json
        log_info("Regenerating local repository metadata from: %s", repo_path);
        
        int rc = generate_repodata(repo_path);
        if (rc != 0) {
            log_error("Failed to generate repodata.json from local repository");
            return -1;
        }
        
        log_info("Local repository metadata updated successfully");
        return 0;
    }
}