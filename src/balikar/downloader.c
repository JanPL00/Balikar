// Created by placeja5 on 11/14/25.
//

#include "includes/downloader.h"

// Helper: Curl write callback
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
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

// Helper: Compute SHA256 hash of file (return 1 if matches expected)
static int verify_hash_from_file(const char *file_path, const char *expected_sha) {
    if (!expected_sha) return 1;  // Skip if no sha in metadata
    
    char *computed_sha = compute_file_sha256(file_path);
    if (!computed_sha) return 0;
    
    int match = (strcmp(computed_sha, expected_sha) == 0);
    if (!match) {
        log_warn("SHA256 mismatch for %s: expected %s, got %s", file_path, expected_sha, computed_sha);
    }
    free(computed_sha);
    return match;
}

FILE *download_package(Solvable *s, const char *loc, Pool *pool, Repodata *data) {
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
    char cache_path[2048];
    const char *ext = strrchr(loc, '.') ? strrchr(loc, '.') : ".tar.xz";
    snprintf(cache_path, sizeof(cache_path), "%s/%s-%s%s", cache_dir, pkg_name, version, ext);

    // Získej sha z metadata (custom nebo standard)
    // Vypočítej Id p ze Solvable pointer
    Id p = s - pool->solvables;
    Id sha_key = pool_str2id(pool, "sha256", 0);
    const char *sha = repodata_lookup_str(data, p, sha_key);

    // Check cache + verify
    if (access(cache_path, F_OK) == 0 && verify_hash_from_file(cache_path, sha)) {
        log_info("Using cached file: %s", cache_path);
        free(cache_dir);
        return fopen(cache_path, "r");
    }

    // Vytvoř tmp session dir v cache
    char tmp_dir[2048];
    snprintf(tmp_dir, sizeof(tmp_dir), "%s/%sXXXXXX", cache_dir, TMP_SESSION_PREFIX);
    if (!mkdtemp(tmp_dir)) {
        log_error("Failed to create tmp session dir.");
        free(cache_dir);
        return NULL;
    }

    // Download nebo copy
    int is_local = (loc[0] == '/' || strncmp(loc, "./", 2) == 0 || strncmp(loc, "../", 3) == 0);
    if (is_local) {
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", loc, cache_path);
        if (system(cmd) != 0) {
            log_error("Failed to copy local file to cache: %s", loc);
            rmdir(tmp_dir);  // Cleanup
            free(cache_dir);
            return NULL;
        }
        // Verify local file hash if metadata provides sha
        if (sha && !verify_hash_from_file(cache_path, sha)) {
            log_error("Local file SHA mismatch: %s", cache_path);
            remove(cache_path);
            rmdir(tmp_dir);
            free(cache_dir);
            return NULL;
        }
    } else {
        FILE *fp = fopen(cache_path, "wb");
        if (!fp) {
            log_error("Failed to open %s for writing.", cache_path);
            rmdir(tmp_dir);
            free(cache_dir);
            return NULL;
        }

        CURL *curl = curl_easy_init();
        if (!curl) {
            fclose(fp);
            remove(cache_path);
            rmdir(tmp_dir);
            free(cache_dir);
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, loc);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        // Retry: curl_easy_setopt(curl, CURLOPT_RETRY, 3L);  // Pokud existuje v libcurl

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);

        if (res != CURLE_OK) {
            remove(cache_path);
            rmdir(tmp_dir);
            log_error("Download failed: %s (%s)", curl_easy_strerror(res), loc);
            free(cache_dir);
            return NULL;
        }

        // Verify po downloadu if SHA provided
        if (sha && !verify_hash_from_file(cache_path, sha)) {
            remove(cache_path);
            rmdir(tmp_dir);
            free(cache_dir);
            return NULL;
        }
        
        // Compute and store SHA for remote files (for future verification)
        char *computed_sha = compute_file_sha256(cache_path);
        if (computed_sha) {
            log_info("Computed SHA256 for remote file %s: %s", loc, computed_sha);
            
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

    log_info("Downloaded/cached %s to %s", loc, cache_path);

    // Vrať fopen pro builder (z cache)
    FILE *fp = fopen(cache_path, "rb");
    free(cache_dir);
    return fp;
}

// Funkce pro extras (patches array)
void download_extras(json_t *extras, const char *pkg_name, const char *tmp_dir) {
    if (!extras || !json_is_array(extras)) return;

    char extras_subdir[2048];
    snprintf(extras_subdir, sizeof(extras_subdir), "%s/extras/%s", CACHE_DIR, pkg_name);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", extras_subdir);
    system(cmd);

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