// Created by placeja5 on 11/14/25.
//

#include "includes/downloader.h"
#include <ctype.h>

// Helper: Curl write callback
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

// Helper: Progress callback for curl download
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp;
    (void)ultotal;
    (void)ulnow;
    
    // Spinner pro neznámou velikost
    if (dltotal <= 0) {
        static int spin_idx = 0;
        char spinner[] = "|/-\\";
        double dl_mb = dlnow / 1024.0 / 1024.0;
        fprintf(stderr, "\rDownloading... %c (%.1fMB)", spinner[spin_idx], dl_mb);
        spin_idx = (spin_idx + 1) % 4;
        fflush(stderr);
        return 0;
    }
    
    int percent = (int)((100.0 * dlnow) / dltotal);
    int bar_width = 50;
    int filled = (bar_width * dlnow) / dltotal;
    
    // Formátování velikostí
    double dl_mb = dlnow / 1024.0 / 1024.0;
    double total_mb = dltotal / 1024.0 / 1024.0;
    
    // Progress bar: [=====>     ] 45% (5.2MB/11.5MB)
    fprintf(stderr, "\r[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) fprintf(stderr, "=");
        else if (i == filled) fprintf(stderr, ">");
        else fprintf(stderr, " ");
    }
    fprintf(stderr, "] %3d%% (%.1fMB/%.1fMB)", percent, dl_mb, total_mb);
    fflush(stderr);
    
    if (dlnow >= dltotal) {
        fprintf(stderr, "\n");  // Nový řádek po dokončení
    }
    
    return 0;
}

// Helper: Compute MD5 of file and return hex string (caller must free)
static char* compute_file_md5(const char *file_path) {
    if (!file_path) return NULL;
    
    FILE *fp = fopen(file_path, "rb");
    if (!fp) return NULL;
    
    MD5_CTX md5;
    MD5_Init(&md5);
    unsigned char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        MD5_Update(&md5, buffer, bytes_read);
    }
    fclose(fp);
    
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5);
    
    char *computed_md5 = malloc(MD5_DIGEST_LENGTH * 2 + 1);
    if (!computed_md5) return NULL;
    
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        snprintf(computed_md5 + i * 2, 3, "%02x", hash[i]);
    }
    computed_md5[MD5_DIGEST_LENGTH * 2] = '\0';
    
    return computed_md5;
}

// Helper: Compute SHA256 of file and return hex string (caller must free)
static char* compute_file_sha256(const char *file_path) {
    if (!file_path) return NULL;
    
    FILE *fp = fopen(file_path, "rb");
    if (!fp) return NULL;
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    unsigned char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        SHA256_Update(&sha256, buffer, bytes_read);
    }
    fclose(fp);
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    
    char *computed_sha = malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    if (!computed_sha) return NULL;
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(computed_sha + i * 2, 3, "%02x", hash[i]);
    }
    computed_sha[SHA256_DIGEST_LENGTH * 2] = '\0';
    
    return computed_sha;
}

// Helper: Compute hash of file (MD5 or SHA256) and verify (return 1 if matches expected)
static int verify_hash_from_file(const char *file_path, const char *expected_hash) {
    if (!expected_hash) return 1;  // Skip if no hash in metadata
    
    size_t hash_len = strlen(expected_hash);
    
    if (hash_len == 32) {
        // MD5 hash (128 bits = 32 hex chars)
        char *computed_md5 = compute_file_md5(file_path);
        if (!computed_md5) return 0;
        
        int match = (strcmp(computed_md5, expected_hash) == 0);
        if (!match) {
            log_warn("MD5 mismatch for %s: expected %s, got %s", file_path, expected_hash, computed_md5);
        } else {
            log_debug(1, "MD5 verified: %s", computed_md5);
        }
        free(computed_md5);
        return match;
    } else if (hash_len == 64) {
        // SHA256 hash (256 bits = 64 hex chars)
        char *computed_sha = compute_file_sha256(file_path);
        if (!computed_sha) return 0;
        
        int match = (strcmp(computed_sha, expected_hash) == 0);
        if (!match) {
            log_warn("SHA256 mismatch for %s: expected %s, got %s", file_path, expected_hash, computed_sha);
        } else {
            log_debug(1, "SHA256 verified: %s", computed_sha);
        }
        free(computed_sha);
        return match;
    } else {
        log_warn("Invalid hash length (%zu chars) - expected 32 (MD5) or 64 (SHA256): %s", 
                 hash_len, expected_hash);
        return 1;  // Skip validation but warn about bad metadata
    }
}

// Helper: read expected SHA256 from sidecar file if present
static char* read_sha256_sidecar(const char *cache_path) {
    if (!cache_path) return NULL;
    char sidecar[2048];
    snprintf(sidecar, sizeof(sidecar), "%s.sha256", cache_path);
    FILE *fp = fopen(sidecar, "r");
    if (!fp) return NULL;
    char buf[128] = {0};
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t')) buf[--n] = '\0';
    char *sha = malloc(n + 1);
    if (!sha) return NULL;
    memcpy(sha, buf, n + 1);
    return sha;
}

FILE *download_package(Solvable *s, const char *loc, Pool *pool, Repodata *data, const char *repo_path) {
    if (!s || !loc) {
        log_error("Invalid arguments in download_package.");
        return NULL;
    }

    char *cache_dir = CACHE_DIR;  // strdup pokud potřeba modifikovat
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", cache_dir);
    if (system(cmd) != 0) {
        log_error("Failed to create cache dir: %s", cache_dir);
        free(cache_dir);
        return NULL;
    }

    char *pkg_name = pool_id2str(pool, s->name);
    char *version = pool_id2str(pool, s->evr);
    
    // Získej Id p ze Solvable pointer
    Id p = s - pool->solvables;
    
    // Preferuj source[] array (nový formát) před download_url
    const char *actual_url = loc;  // fallback
    const char *sha = NULL;
    
    // Zkus načíst source array
    json_t *source_array = get_custom_source_array(data, p, pool);
    if (source_array && json_array_size(source_array) > 0) {
        json_t *first_source = json_array_get(source_array, 0);
        if (first_source && json_is_object(first_source)) {
            json_t *url_json = json_object_get(first_source, "url");
            json_t *sha_json = json_object_get(first_source, "sha256");
            
            if (url_json && json_is_string(url_json)) {
                actual_url = json_string_value(url_json);
                log_info("Using source[0].url: %s", actual_url);
            }
            if (sha_json && json_is_string(sha_json)) {
                sha = json_string_value(sha_json);
                log_info("Using source[0].sha256: %s", sha);
            }
        }
    }
    
    // Fallback na standardní sha256 z metadata
    if (!sha) {
        Id sha_key = pool_str2id(pool, "sha256", 0);
        sha = repodata_lookup_str(data, p, sha_key);
    }
    
    char cache_path[2048];
    
    // Detekce extension včetně .tar.* formátů
    const char *ext = ".tar.xz";  // default
    const char *last_dot = strrchr(actual_url, '.');
    if (last_dot) {
        const char *last_slash = strrchr(actual_url, '/');
        const char *filename = last_slash ? last_slash + 1 : actual_url;
        
        // Zkontroluj .tar.{xz,gz,bz2} formáty
        if (strstr(filename, ".tar.xz")) ext = ".tar.xz";
        else if (strstr(filename, ".tar.gz")) ext = ".tar.gz";
        else if (strstr(filename, ".tar.bz2")) ext = ".tar.bz2";
        else if (strstr(filename, ".tgz")) ext = ".tgz";
        else ext = last_dot;  // jiná extension
    }
    snprintf(cache_path, sizeof(cache_path), "%s/%s-%s%s", cache_dir, pkg_name, version, ext);

    // If metadata lacks sha, try sidecar for validation
    char *sidecar_sha = NULL;
    if (!sha) sidecar_sha = read_sha256_sidecar(cache_path);

    // Check cache + verify (prefer metadata sha, fallback to sidecar sha)
    if (access(cache_path, F_OK) == 0) {
        if (verify_hash_from_file(cache_path, sha ? sha : sidecar_sha)) {
            log_info("Using cached file: %s", cache_path);
            free(cache_dir);
            if (sidecar_sha) free(sidecar_sha);
            return fopen(cache_path, "r");
        } else {
            // Verifikace selhala - smaž poškozený/prázdný soubor
            log_warn("Cached file verification failed, removing: %s", cache_path);
            remove(cache_path);
            // Smaž i sidecar pokud existuje
            char sha_sidecar[2048];
            snprintf(sha_sidecar, sizeof(sha_sidecar), "%s.sha256", cache_path);
            remove(sha_sidecar);
        }
    }
    if (sidecar_sha) { free(sidecar_sha); sidecar_sha = NULL; }

    // Download nebo copy
    // Check if it's a local path (absolute, relative, or not a URL)
    // URLs typically start with http://, https://, ftp://, etc.
    int is_local = (actual_url[0] == '/' || strncmp(actual_url, "./", 2) == 0 || strncmp(actual_url, "../", 3) == 0 ||
                    (strncmp(actual_url, "http://", 7) != 0 && strncmp(actual_url, "https://", 8) != 0 &&
                     strncmp(actual_url, "ftp://", 6) != 0 && strncmp(actual_url, "file://", 7) != 0));
    
    // FALLBACK: Try to find local equivalent if URL is remote
    // Before attempting HTTP download, try to find the file locally in repo_path or cache
    if (!is_local && repo_path) {
        // Try to construct local path from repo structure
        // Example: for glibc-2.42, try ../balikar-repository/g/glibc/glibc-2.42.tar.bz2
        char local_attempt[2048];
        const char *pkg_first_char = pkg_name ? pkg_name : "unknown";
        snprintf(local_attempt, sizeof(local_attempt), "%s/%c/%s/%s-%s.tar.bz2", 
                 repo_path, tolower(pkg_first_char[0]), pkg_name, pkg_name, version);
        
        if (access(local_attempt, F_OK) == 0) {
            log_debug(1, "Found local fallback for %s: %s", pkg_name, local_attempt);
            actual_url = local_attempt;
            is_local = 1;
        } else {
            log_debug(2, "Local fallback not found: %s", local_attempt);
        }
    }
    
    if (is_local) {
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", actual_url, cache_path);
        if (system(cmd) != 0) {
            log_error("Failed to copy local file to cache: %s", actual_url);
            if (source_array) json_decref(source_array);
            free(cache_dir);
            return NULL;
        }
        // Verify local file hash if metadata provides sha
        if (sha && !verify_hash_from_file(cache_path, sha)) {
            log_error("Local file SHA mismatch: %s", cache_path);
            remove(cache_path);
            if (source_array) json_decref(source_array);
            free(cache_dir);
            return NULL;
        }
    } else {
        FILE *fp = fopen(cache_path, "wb");
        if (!fp) {
            log_error("Failed to open %s for writing.", cache_path);
            if (source_array) json_decref(source_array);
            free(cache_dir);
            return NULL;
        }

        CURL *curl = curl_easy_init();
        if (!curl) {
            fclose(fp);
            remove(cache_path);
            if (source_array) json_decref(source_array);
            free(cache_dir);
            return NULL;
        }

        log_info("Downloading from: %s", actual_url);
        
        curl_easy_setopt(curl, CURLOPT_URL, actual_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minut timeout
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // 10s connect timeout (rychlejší detekce nedostupného serveru)
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); // Preferuj IPv4 kvůli problémům s IPv6
        
        // Progress bar callback
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        CURLcode res = curl_easy_perform(curl);
        printf("\n");  // Ukončit progress řádek před výpisem chyby
        fflush(stdout);
        
        // Enhanced diagnostics for connection failures
        if (res != CURLE_OK) {
            if (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_COULDNT_CONNECT) {
                log_error("Connection failed to: %s", actual_url);
                log_error("  Error: %s (code %d)", curl_easy_strerror(res), res);
                
                // Get effective URL in case of redirects
                char *effective_url = NULL;
                curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
                if (effective_url) {
                    log_error("  Effective URL: %s", effective_url);
                }
                
                // Get curl version info
                curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);
                log_error("  libcurl version: %s", ver->version);
            } else {
                log_error("Download failed: %s (%s)", curl_easy_strerror(res), actual_url);
            }
            
            curl_easy_cleanup(curl);
            fclose(fp);
            remove(cache_path);
            if (source_array) json_decref(source_array);
            free(cache_dir);
            return NULL;
        }
        
        curl_easy_cleanup(curl);
        fclose(fp);

        // Verify po downloadu if SHA provided
        if (sha && !verify_hash_from_file(cache_path, sha)) {
            remove(cache_path);
            if (source_array) json_decref(source_array);
            free(cache_dir);
            return NULL;
        }
        
        // Compute and store SHA for remote files (for future verification)
        char *computed_sha = compute_file_sha256(cache_path);
        if (computed_sha) {
            log_info("Computed SHA256 for remote file %s: %s", actual_url, computed_sha);
            
            // Store SHA in sidecar file for future cache validation
            char sha_sidecar[2048];
            snprintf(sha_sidecar, sizeof(sha_sidecar), "%s.sha256", cache_path);
            FILE *sha_fp = fopen(sha_sidecar, "w");
            if (sha_fp) {
                fprintf(sha_fp, "%s\n", computed_sha);
                fclose(sha_fp);
                log_debug(1, "Stored SHA256 in sidecar: %s", sha_sidecar);
            }
            free(computed_sha);
        }
    }

    log_info("Downloaded/cached %s to %s", actual_url, cache_path);

    // Cleanup JSON if allocated
    if (source_array) json_decref(source_array);

    // Vrať fopen pro builder (z cache)
    FILE *fp = fopen(cache_path, "rb");
    free(cache_dir);
    return fp;
}

// Funkce pro extras (patches array)
// Helper: Stáhne repository archiv a extrahuje .build skript
static void try_download_build_script(const char *pkg_name, const char *version, const char *extras_subdir, const char *repo_base_url) {
    if (!pkg_name || !version || !extras_subdir || !repo_base_url) {
        log_debug(2, "try_download_build_script: missing params - pkg=%s, ver=%s, extras=%s, repo=%s",
                  pkg_name ? pkg_name : "NULL", version ? version : "NULL", 
                  extras_subdir ? extras_subdir : "NULL", repo_base_url ? repo_base_url : "NULL");
        return;
    }
    
    log_info("Trying to download .build script for %s-%s from repository", pkg_name, version);
    
    char build_path[2048];
    snprintf(build_path, sizeof(build_path), "%s/.build", extras_subdir);
    
    // Pokud už existuje, skip
    if (access(build_path, F_OK) == 0) {
        log_info("Build script already cached: %s", build_path);
        return;
    }
    
    // Sestavit URL repository archivu: https://repo.../g/glibc/glibc-2.42.tar.bz2
    char first_letter = tolower(pkg_name[0]);
    char repo_archive_url[2048];
    snprintf(repo_archive_url, sizeof(repo_archive_url), "%s/%c/%s/%s-%s.tar.bz2", 
             repo_base_url, first_letter, pkg_name, pkg_name, version);
    
    log_info("Repository archive URL: %s", repo_archive_url);
    
    // Stáhnout repository archiv do dočasného souboru
    char temp_archive[2048];
    snprintf(temp_archive, sizeof(temp_archive), "%s/repo_archive.tar.bz2", extras_subdir);
    
    // Check if it's a local path
    int is_local = (repo_archive_url[0] == '/' || strncmp(repo_archive_url, "./", 2) == 0 || 
                    strncmp(repo_archive_url, "../", 3) == 0 ||
                    (strncmp(repo_archive_url, "http://", 7) != 0 && 
                     strncmp(repo_archive_url, "https://", 8) != 0 &&
                     strncmp(repo_archive_url, "file://", 7) != 0));
    
    if (is_local) {
        // Lokální soubor - zkopíruj
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", repo_archive_url, temp_archive);
        if (system(cmd) != 0) {
            log_debug(2, "Failed to copy local repository archive: %s", repo_archive_url);
            return;
        }
    } else {
        // Stáhnout přes HTTP
        FILE *fp = fopen(temp_archive, "wb");
        if (!fp) return;
        
        CURL *curl = curl_easy_init();
        if (!curl) {
            fclose(fp);
            return;
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, repo_archive_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); // Preferuj IPv4
        
        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);
        fclose(fp);
        
        if (res != CURLE_OK || http_code != 200) {
            remove(temp_archive);
            log_debug(2, "No repository archive found at %s (HTTP %ld)", repo_archive_url, http_code);
            return;
        }
    }
    
    // Extrahovat .build z archivu
    char extract_cmd[4096];
    snprintf(extract_cmd, sizeof(extract_cmd), 
             "tar -xjf \"%s\" -C \"%s\" --strip-components=1 \"*/.build\" 2>/dev/null || "
             "tar -xjf \"%s\" -C \"%s\" --wildcards \"*/.build\" --strip-components=1 2>/dev/null",
             temp_archive, extras_subdir, temp_archive, extras_subdir);
    
    log_info("Extracting .build from archive to %s", extras_subdir);
    int extract_result = system(extract_cmd);
    if (extract_result == 0) {
        chmod(build_path, 0755);
        log_info("Extracted .build script from repository archive");
    } else {
        log_warn("Failed to extract .build from repository archive (exit code: %d)", extract_result);
    }
    
    // Odstranit dočasný archiv
    remove(temp_archive);
}

void download_extras(json_t *extras, const char *pkg_name, const char *version, const char *repo_base_url, const char *tmp_dir) {
    if (!pkg_name) return;

    char extras_subdir[2048];
    snprintf(extras_subdir, sizeof(extras_subdir), "%s/extras/%s", CACHE_DIR, pkg_name);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", extras_subdir);
    system(cmd);
    
    // Automaticky zkusit stáhnout .build skript z repository
    log_info("download_extras called: pkg=%s, version=%s, repo_base_url=%s", 
             pkg_name, version ? version : "NULL", repo_base_url ? repo_base_url : "NULL");
    if (version && repo_base_url) {
        log_info("Attempting to download .build script from repository...");
        try_download_build_script(pkg_name, version, extras_subdir, repo_base_url);
    } else {
        log_warn("Skipping .build download: version=%s, repo_base_url=%s", 
                 version ? "OK" : "NULL", repo_base_url ? "OK" : "NULL");
    }

    // Stáhnout ostatní extras z JSON
    if (!extras || !json_is_array(extras)) return;

    size_t i;
    json_t *item;
    json_array_foreach(extras, i, item) {
        if (!json_is_string(item)) continue;

        const char *extra_url = json_string_value(item);
        char extra_path[2048];
        const char *extra_name = strrchr(extra_url, '/') ? strrchr(extra_url, '/') + 1 : "extra_file";
        snprintf(extra_path, sizeof(extra_path), "%s/%s", extras_subdir, extra_name);

        // Podobný logic jako download_package, ale bez Solvable (jen URL)
        if (access(extra_path, F_OK) == 0) {
            log_info("Using cached extra: %s", extra_path);
            continue;
        }

        FILE *fp = fopen(extra_path, "wb");
        if (!fp) continue;

        CURL *curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, extra_url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minut timeout
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L); // 30s connect timeout
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); // Preferuj IPv4
            
            // Progress bar pro extras
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            if (res == CURLE_OK) {
                log_info("Downloaded extra %s to %s", extra_url, extra_path);
            } else {
                remove(extra_path);
            }
        }
        fclose(fp);
    }
}