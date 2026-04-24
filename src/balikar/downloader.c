// Created by placeja5 on 11/14/25.
//

#include "includes/downloader.h"

/**
 * CURL write callback for downloading files.
 * 
 * Standard libcurl callback that writes downloaded data to a file stream.
 * 
 * @param ptr Pointer to downloaded data
 * @param size Size of each data element
 * @param nmemb Number of elements
 * @param stream FILE* to write to
 * @return Number of bytes written
 */
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

/**
 * Progress callback for CURL downloads.
 * 
 * Displays progress bar or spinner during download. Shows percentage,
 * downloaded/total size in MB, and visual progress indicator.
 * 
 * @param clientp Client data pointer (unused)
 * @param dltotal Total download size
 * @param dlnow Current downloaded size
 * @param ultotal Total upload size (unused)
 * @param ulnow Current upload size (unused)
 * @return 0 to continue, non-zero to abort
 */
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp;
    (void)ultotal;
    (void)ulnow;
    
    // Show spinner when total size is unknown
    if (dltotal <= 0) {
        static int spin_idx = 0;
        char spinner[] = "|/-\\";
        double dl_mb = dlnow / 1024.0 / 1024.0;
        fprintf(stderr, "\rDownloading... %c (%.1fMB)", spinner[spin_idx], dl_mb);
        spin_idx = (spin_idx + 1) % 4;
        fflush(stderr);
        return 0;
    }
    
    // Calculate progress percentage and bar fill
    int percent = (int)((100.0 * dlnow) / dltotal);
    int bar_width = 50;
    int filled = (bar_width * dlnow) / dltotal;
    
    // Format sizes in megabytes for display
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
    
    // Only print newline once at 100%, not on every callback after completion
    static int completed = 0;
    if (dlnow >= dltotal && dltotal > 0 && !completed) {
        fprintf(stderr, "\n");  // New line after completion
        completed = 1;
    } else if (dlnow < dltotal) {
        completed = 0;  // Reset for next download
    }
    
    return 0;
}



/**
 * Extract repository archive and check for embedded source files.
 * 
 * This function enables packages to include source archives directly in the
 * repository archive (e.g., gzip-1.14.tar.bz2 contains gzip-1.14/gzip-1.14.tar.xz).
 * 
 * Process:
 * 1. Extract repository archive to temporary directory
 * 2. Read index.json to get source[0].url
 * 3. Check if source URL is a local filename (no ://)
 * 4. If local file exists in extracted archive, copy to cache
 * 
 * @param repo_archive_path Path to repository archive in cache
 * @param pkg_name Package name
 * @param version Package version
 * @param target_cache_path Where to copy the embedded source
 * @return 1 if embedded source found and copied, 0 otherwise
 */
static int extract_and_check_embedded_source(const char *repo_archive_path,
                                             const char *pkg_name,
                                             const char *version,
                                             const char *target_cache_path) {
    if (!repo_archive_path || !pkg_name || !version || !target_cache_path) {
        return 0;
    }
    
    // Check if repository archive exists
    if (access(repo_archive_path, F_OK) != 0) {
        return 0;
    }
    
    // Create temporary extraction directory
    char tmp_extract[2048];
    snprintf(tmp_extract, sizeof(tmp_extract), "%s/balikar_extract_%s_%d", 
             CACHE_DIR, pkg_name, (int)getpid());
    
    char mkdir_cmd[2560];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", tmp_extract);
    if (system(mkdir_cmd) != 0) {
        return 0;
    }
    
    // Extract repository archive
    char extract_cmd[4096];
    snprintf(extract_cmd, sizeof(extract_cmd), "tar -xjf '%s' -C '%s' 2>/dev/null",
             repo_archive_path, tmp_extract);
    
    int result = 0;
    if (system(extract_cmd) == 0) {
        // Look for index.json in extracted package directory
        char index_path[2560];
        snprintf(index_path, sizeof(index_path), "%s/%s-%s/index.json",
                tmp_extract, pkg_name, version);
        
        if (access(index_path, F_OK) == 0) {
            // Parse index.json to get source array
            json_error_t error;
            json_t *index_root = json_load_file(index_path, 0, &error);
            
            if (index_root) {
                json_t *source_array = json_object_get(index_root, "source");
                
                if (source_array && json_is_array(source_array) && json_array_size(source_array) > 0) {
                    json_t *first_source = json_array_get(source_array, 0);
                    
                    if (first_source && json_is_object(first_source)) {
                        json_t *url_json = json_object_get(first_source, "url");
                        
                        if (url_json && json_is_string(url_json)) {
                            const char *source_file = json_string_value(url_json);
                            
                            // Check if it's a local filename (not a URL with ://)
                            if (!strstr(source_file, "://")) {
                                char embedded_path[4096];
                                snprintf(embedded_path, sizeof(embedded_path), "%s/%s-%s/%s",
                                        tmp_extract, pkg_name, version, source_file);
                                
                                if (access(embedded_path, F_OK) == 0) {
                                    // Copy embedded source to cache
                                    char copy_cmd[8192];
                                    snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s' '%s'",
                                            embedded_path, target_cache_path);
                                    
                                    if (system(copy_cmd) == 0) {
                                        log_info("Using embedded source from repository archive: %s", source_file);
                                        result = 1;
                                    }
                                }
                            }
                        }
                    }
                }
                json_decref(index_root);
            }
        }
    }
    
    // Cleanup temporary extraction directory
    char rm_cmd[2560];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf '%s'", tmp_extract);
    system(rm_cmd);
    
    return result;
}

/**
 * Download package and return file pointer.
 * 
 * Main download function that:
 * 1. Creates cache directory
 * 2. Checks for cached file (no hash verification)
 * 3. **NEW**: Checks for embedded source in repository archive
 * 4. Downloads from URL or copies from local path
 * 
 * Supports both local files and HTTP/HTTPS URLs with automatic fallback
 * to local repository structure. Includes progress indication.
 * 
 * @param s Solvable for package to download
 * @param loc Location string (may be overridden by metadata)
 * @param pool Package pool
 * @param data Repodata containing custom metadata
 * @param repo_path Repository base path for local fallback
 * @return FILE pointer to downloaded/cached package, or NULL on error
 */
FILE *download_package(Solvable *s, const char *loc, Pool *pool, json_t *source_array, const char *repo_path) {
    if (!s || !loc) {
        log_error("Invalid arguments in download_package.");
        return NULL;
    }

    char *cache_dir = CACHE_DIR;  // strdup if modification needed
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", cache_dir);
    if (system(cmd) != 0) {
        log_error("Failed to create cache dir: %s", cache_dir);
        free(cache_dir);
        return NULL;
    }

    char *pkg_name = pool_id2str(pool, s->name);
    char *version = pool_id2str(pool, s->evr);
    
    // Get solvable ID from Solvable pointer
    Id p = s - pool->solvables;
    
    // Source array is passed injected from main.c
    size_t source_count = 0;
    if (source_array && json_is_array(source_array)) {
        source_count = json_array_size(source_array);
    }
    
    // Detect file extension from first URL for cache path
    const char *first_url = loc;  // fallback
    if (source_count > 0) {
        json_t *first_source = json_array_get(source_array, 0);
        if (first_source && json_is_object(first_source)) {
            json_t *url_json = json_object_get(first_source, "url");
            if (url_json && json_is_string(url_json)) {
                first_url = json_string_value(url_json);
            }
        }
    }
    
    char cache_path[2048];
    
    // Detect file extension including .tar.* formats
    const char *ext = ".tar.xz";  // default
    const char *last_dot = strrchr(first_url, '.');
    if (last_dot) {
        const char *last_slash = strrchr(first_url, '/');
        const char *filename = last_slash ? last_slash + 1 : first_url;
        
        // Check for .tar.{xz,gz,bz2} formats
        if (strstr(filename, ".tar.xz")) ext = ".tar.xz";
        else if (strstr(filename, ".tar.gz")) ext = ".tar.gz";
        else if (strstr(filename, ".tar.bz2")) ext = ".tar.bz2";
        else if (strstr(filename, ".tgz")) ext = ".tgz";
        else ext = last_dot;  // other extension
    }
    snprintf(cache_path, sizeof(cache_path), "%s/%s-%s%s", cache_dir, pkg_name, version, ext);

    // Check cache (no verification)
    if (access(cache_path, F_OK) == 0) {
        log_info("Using cached file (no verification): %s", cache_path);
        free(cache_dir);
        if (source_array) json_decref(source_array);
        return fopen(cache_path, "r");
    }

    // NEW: Check for embedded source in repository archive BEFORE downloading from remote
    // This allows packages to include source files directly (e.g., gzip-1.14.tar.bz2 contains gzip-1.14.tar.xz)
    if (loc && strstr(loc, "://")) {  // loc is a remote repository URL
        // Construct repository archive path (always .tar.bz2)
        char repo_archive_path[2048];
        snprintf(repo_archive_path, sizeof(repo_archive_path), "%s/%s-%s.tar.bz2", 
                cache_dir, pkg_name, version);
        
        // Download repository archive if not cached
        if (access(repo_archive_path, F_OK) != 0) {
            log_debug(1, "Downloading repository archive: %s", loc);
            if (download_url_to_file(loc, repo_archive_path) != 0) {
                log_warn("Failed to download repository archive");
                return NULL;
            }
        }
        
        // Check for embedded source in repository archive
        if (extract_and_check_embedded_source(repo_archive_path, pkg_name, version, cache_path)) {
            // Embedded source was found and copied to cache
            free(cache_dir);
            if (source_array) json_decref(source_array);
            return fopen(cache_path, "r");
        }
    }

    // Try each URL in source array until one succeeds
    int download_success = 0;
    for (size_t url_idx = 0; url_idx < source_count && !download_success; url_idx++) {
        json_t *source_obj = json_array_get(source_array, url_idx);
        if (!source_obj || !json_is_object(source_obj)) continue;
        
        json_t *url_json = json_object_get(source_obj, "url");
        if (!url_json || !json_is_string(url_json)) continue;
        
        const char *actual_url = json_string_value(url_json);
        log_info("Trying source[%zu].url: %s", url_idx, actual_url);
        
        // Determine if source is local file or remote URL
        int is_remote = (strncmp(actual_url, "http://", 7) == 0 || 
                        strncmp(actual_url, "https://", 8) == 0 ||
                        strncmp(actual_url, "ftp://", 6) == 0 || 
                        strncmp(actual_url, "file://", 7) == 0);
        int is_local = !is_remote;
        
        if (is_local) {
            // Try multiple possible local paths for the source file
            char full_local_path[2048];
            int found_local = 0;
            
            // If it's an absolute path, use it directly
            if (actual_url[0] == '/') {
                snprintf(full_local_path, sizeof(full_local_path), "%s", actual_url);
                if (access(full_local_path, F_OK) == 0) {
                    found_local = 1;
                }
            }
            // If it's a ./ or ../ relative path, try relative to repo_path (if local)
            else if (strncmp(actual_url, "./", 2) == 0 || strncmp(actual_url, "../", 3) == 0) {
                if (repo_path && repo_path[0] == '/') {
                    // Only works if repo_path is local (not a URL)
                    snprintf(full_local_path, sizeof(full_local_path), "%s/%s", repo_path, actual_url);
                    if (access(full_local_path, F_OK) == 0) {
                        found_local = 1;
                    }
                }
            }
            // Otherwise it's a relative filename - try multiple locations:
            else {
                // 1. Try relative to repo_path (if it's a local path, not URL)
                if (repo_path && repo_path[0] == '/' && pkg_name && version) {
                    char first_letter = tolower(pkg_name[0]);
                    snprintf(full_local_path, sizeof(full_local_path), "%s/%c/%s/%s-%s/%s",
                            repo_path, first_letter, pkg_name, pkg_name, version, actual_url);
                    if (access(full_local_path, F_OK) == 0) {
                        found_local = 1;
                        log_debug(1, "Found local source via repo_path: %s", full_local_path);
                    }
                }
                
                // 2. Try common local repository locations if not found yet
                if (!found_local && pkg_name && version) {
                    const char *local_repo_paths[] = {
                        "/home/placeja5/Balikar/balikar-repository",
                        "./repo",
                        "../balikar-repository",
                        NULL
                    };
                    
                    for (int i = 0; local_repo_paths[i] != NULL && !found_local; i++) {
                        char first_letter = tolower(pkg_name[0]);
                        snprintf(full_local_path, sizeof(full_local_path), "%s/%c/%s/%s-%s/%s",
                                local_repo_paths[i], first_letter, pkg_name, pkg_name, version, actual_url);
                        if (access(full_local_path, F_OK) == 0) {
                            found_local = 1;
                            log_info("Found local source in %s: %s", local_repo_paths[i], actual_url);
                        }
                    }
                }
                
                // 3. Try as-is (current directory)
                if (!found_local) {
                    snprintf(full_local_path, sizeof(full_local_path), "%s", actual_url);
                    if (access(full_local_path, F_OK) == 0) {
                        found_local = 1;
                    }
                }
            }
            
            // If no local file found, skip to next source
            if (!found_local) {
                log_info("Local source file not found for: %s (trying next source)", actual_url);
                continue;  // Try next source
            }
            
            // Try local file copy
            char cmd_local[2048];
            snprintf(cmd_local, sizeof(cmd_local), "cp \"%s\" \"%s\"", full_local_path, cache_path);
            if (system(cmd_local) == 0) {
                log_info("Copied local file to cache: %s -> %s", full_local_path, cache_path);
                download_success = 1;
            } else {
                log_warn("Failed to copy from source[%zu]: %s", url_idx, full_local_path);
            }
        } else {
            // Try remote download
            FILE *fp = fopen(cache_path, "wb");
            if (!fp) {
                log_warn("Failed to open %s for writing, trying next source", cache_path);
                continue;
            }

            CURL *curl = curl_easy_init();
            if (!curl) {
                fclose(fp);
                log_warn("Failed to initialize curl for source[%zu], trying next", url_idx);
                continue;
            }

            log_info("Downloading from: %s", actual_url);
            
            // Configure CURL options for download
            curl_easy_setopt(curl, CURLOPT_URL, actual_url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
            
            // Progress bar callback
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

            CURLcode res = curl_easy_perform(curl);
            printf("\n");
            fflush(stdout);
            
            curl_easy_cleanup(curl);
            fclose(fp);
            
            if (res == CURLE_OK) {
                log_info("Successfully downloaded from source[%zu]: %s", url_idx, actual_url);
                download_success = 1;
            } else {
                log_warn("Download failed from source[%zu]: %s (%s)", url_idx, actual_url, curl_easy_strerror(res));
                remove(cache_path);  // Remove partial download
            }
        }
    }
    
    // If no source URL worked, try local fallback from repository structure
    if (!download_success && repo_path) {
        char local_attempt[2048];
        const char *pkg_first_char = pkg_name ? pkg_name : "unknown";
        snprintf(local_attempt, sizeof(local_attempt), "%s/%c/%s/%s-%s.tar.bz2", 
                 repo_path, tolower(pkg_first_char[0]), pkg_name, pkg_name, version);
        
        if (access(local_attempt, F_OK) == 0) {
            log_info("Found local repository fallback: %s", local_attempt);
            char cmd_fallback[2048];
            snprintf(cmd_fallback, sizeof(cmd_fallback), "cp \"%s\" \"%s\"", local_attempt, cache_path);
            if (system(cmd_fallback) == 0) {
                log_info("Copied local fallback to cache: %s", cache_path);
                download_success = 1;
            }
        } else {
            log_debug(2, "Local fallback not found: %s", local_attempt);
        }
    }
    
    // Cleanup and return result
    if (source_array) json_decref(source_array);
    
    if (!download_success) {
        log_error("All download sources failed for %s", pkg_name);
        free(cache_dir);
        return NULL;
    }

    log_info("Package cached at: %s", cache_path);

    // Return fopen for builder (from cache)
    FILE *fp = fopen(cache_path, "rb");
    free(cache_dir);
    return fp;
}

/**
 * Try to download .build script from repository archive.
 * 
 * Attempts to fetch package's repository archive (e.g., g/glibc/glibc-2.42.tar.bz2)
 * and extract the .build script to extras cache. This provides build instructions
 * when not included in package metadata.
 * 
 * @param pkg_name Package name
 * @param version Package version
 * @param extras_subdir Directory to extract .build script to
 * @param repo_base_url Base URL of repository
 */
static void try_download_build_script(const char *pkg_name, const char *version, const char *extras_subdir, const char *repo_base_url) {
    if (!pkg_name || !version || !extras_subdir || !repo_base_url) {
        log_debug(2, "try_download_build_script: missing params - pkg=%s, ver=%s, extras=%s, repo=%s",
                  pkg_name ? pkg_name : "NULL", version ? version : "NULL", 
                  extras_subdir ? extras_subdir : "NULL", repo_base_url ? repo_base_url : "NULL");
        return;
    }
    
    log_info("Trying to download build.sh script for %s-%s from repository", pkg_name, version);
    
    char build_path[2048];
    snprintf(build_path, sizeof(build_path), "%s/build.sh", extras_subdir);
    
    // Skip if build.sh script already cached
    if (access(build_path, F_OK) == 0) {
        log_info("Build script already cached: %s", build_path);
        return;
    }
    
    // Construct repository archive URL using standard structure
    char first_letter = tolower(pkg_name[0]);
    char repo_archive_url[2048];
    snprintf(repo_archive_url, sizeof(repo_archive_url), "%s/%c/%s/%s-%s.tar.bz2", 
             repo_base_url, first_letter, pkg_name, pkg_name, version);
    
    log_info("Repository archive URL: %s", repo_archive_url);
    
    // Download repository archive to temporary file
    char temp_archive[2048];
    snprintf(temp_archive, sizeof(temp_archive), "%s/repo_archive.tar.bz2", extras_subdir);
    
    // Check if it's a local path
    int is_local = (repo_archive_url[0] == '/' || strncmp(repo_archive_url, "./", 2) == 0 || 
                    strncmp(repo_archive_url, "../", 3) == 0 ||
                    (strncmp(repo_archive_url, "http://", 7) != 0 && 
                     strncmp(repo_archive_url, "https://", 8) != 0 &&
                     strncmp(repo_archive_url, "file://", 7) != 0));
    
    if (is_local) {
        // Local file - copy it
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", repo_archive_url, temp_archive);
        if (system(cmd) != 0) {
            log_debug(2, "Failed to copy local repository archive: %s", repo_archive_url);
            return;
        }
    } else {
        // Download via HTTP
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
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); // Prefer IPv4
        
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
    
    // Extract build.sh from archive
    // Use exact path instead of --wildcards for compatibility with busybox tar (Alpine)
    char extract_cmd[4096];
    snprintf(extract_cmd, sizeof(extract_cmd), 
             "tar -xjf \"%s\" -C \"%s\" --strip-components=1 \"%s-%s/build.sh\" 2>/dev/null",
             temp_archive, extras_subdir, pkg_name, version);
    
    log_info("Extracting build.sh from archive to %s", extras_subdir);
    int extract_result = system(extract_cmd);
    if (extract_result == 0) {
        chmod(build_path, 0755);
        log_info("Extracted build.sh script from repository archive");
    } else {
        log_warn("Failed to extract build.sh from repository archive (exit code: %d)", extract_result);
    }
    
    // Remove temporary archive
    remove(temp_archive);
}

/**
 * Download extra files (patches, additional files) for a package.
 * 
 * Downloads extras defined in package metadata, plus attempts to download
 * .build script from repository archive. Files are cached in extras/<pkg_name>/
 * for use during build process.
 * 
 * @param extras JSON array of extra file URLs
 * @param pkg_name Package name
 * @param version Package version
 * @param repo_base_url Base URL of repository
 * @param tmp_dir Temporary directory (unused)
 */
void download_extras(json_t *extras, const char *pkg_name, const char *version, const char *repo_base_url, const char *tmp_dir) {
    if (!pkg_name) return;

    char extras_subdir[2048];
    snprintf(extras_subdir, sizeof(extras_subdir), "%s/extras/%s", CACHE_DIR, pkg_name);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", extras_subdir);
    system(cmd);
    
    // Automatically try to download build.sh script from repository
    log_info("download_extras called: pkg=%s, version=%s, repo_base_url=%s", 
             pkg_name, version ? version : "NULL", repo_base_url ? repo_base_url : "NULL");
    if (version && repo_base_url) {
        log_info("Attempting to download build.sh script from repository...");
        try_download_build_script(pkg_name, version, extras_subdir, repo_base_url);
    } else {
        log_warn("Skipping build.sh download: version=%s, repo_base_url=%s", 
                 version ? "OK" : "NULL", repo_base_url ? "OK" : "NULL");
    }

    // Download remaining extras from JSON array
    if (!extras || !json_is_array(extras)) return;

    size_t i;
    json_t *item;
    json_array_foreach(extras, i, item) {
        if (!json_is_string(item)) continue;

        const char *extra_url = json_string_value(item);
        char extra_path[2048];
        const char *extra_name = strrchr(extra_url, '/') ? strrchr(extra_url, '/') + 1 : "extra_file";
        snprintf(extra_path, sizeof(extra_path), "%s/%s", extras_subdir, extra_name);

        // Similar logic to download_package, but without Solvable (just URL)
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
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minute timeout
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L); // 30s connect timeout
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); // Prefer IPv4
            
            // Progress bar for extras
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

/**
 * Download package to specified output directory with repository structure.
 * 
 * Downloads package and creates repository-style directory structure:
 * output_dir/<first_letter>/<pkg_name>/<pkg_name>-<version>/
 * 
 * Uses the same multi-source fallback mechanism as download_package.
 * 
 * @param s Solvable for package to download
 * @param loc Location string (may be overridden by metadata)
 * @param pool Package pool
 * @param data Repodata containing custom metadata
 * @param repo_path Repository base path for local fallback
 * @param output_dir Base output directory for downloads
 * @return 0 on success, -1 on error
 */
int download_package_to_directory(Solvable *s, const char *loc, Pool *pool, json_t *source_array, 
                                  const char *repo_path, const char *output_dir) {
    if (!s || !loc || !output_dir) {
        log_error("Invalid arguments in download_package_to_directory.");
        return -1;
    }

    const char *pkg_name = pool_id2str(pool, s->name);
    const char *version = pool_id2str(pool, s->evr);
    
    // Get solvable ID from Solvable pointer
    Id p = s - pool->solvables;
    
    // Source array is passed injected from main.c
    size_t source_count = 0;
    if (source_array && json_is_array(source_array)) {
        source_count = json_array_size(source_array);
    }
    
    // Ensure the output directory exists
    char cmd[2048];
    char first_letter = pkg_name[0];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", output_dir);
    if (system(cmd) != 0) {
        log_error("Failed to create output directory: %s", output_dir);
        if (source_array) json_decref(source_array);
        return -1;
    }
    
    // Determine filename and extension
    const char *first_url = loc;  // fallback
    if (source_count > 0) {
        json_t *first_source = json_array_get(source_array, 0);
        if (first_source && json_is_object(first_source)) {
            json_t *url_json = json_object_get(first_source, "url");
            if (url_json && json_is_string(url_json)) {
                first_url = json_string_value(url_json);
            }
        }
    }
    
    // Detect file extension
    const char *ext = ".tar.xz";  // default
    const char *last_dot = strrchr(first_url, '.');
    if (last_dot) {
        const char *last_slash = strrchr(first_url, '/');
        const char *filename = last_slash ? last_slash + 1 : first_url;
        
        if (strstr(filename, ".tar.xz")) ext = ".tar.xz";
        else if (strstr(filename, ".tar.gz")) ext = ".tar.gz";
        else if (strstr(filename, ".tar.bz2")) ext = ".tar.bz2";
        else if (strstr(filename, ".tgz")) ext = ".tgz";
        else ext = last_dot;
    }
    
    char output_path[2048];
    snprintf(output_path, sizeof(output_path), "%s/%s-%s%s", output_dir, pkg_name, version, ext);
    
    // Check if file already exists
    if (access(output_path, F_OK) == 0) {
        log_info("Package already exists: %s", output_path);
        if (source_array) json_decref(source_array);
        return 0;
    }
    
    // Try each source URL
    int download_success = 0;
    
    if (source_count > 0) {
        for (size_t i = 0; i < source_count; i++) {
            json_t *source_obj = json_array_get(source_array, i);
            if (!source_obj || !json_is_object(source_obj)) continue;
            
            json_t *url_json = json_object_get(source_obj, "url");
            if (!url_json || !json_is_string(url_json)) continue;
            
            const char *source_url = json_string_value(url_json);
            log_info("Trying source[%zu].url: %s", i, source_url);
            
            // Detect if URL is remote or local
            int has_url_scheme = (strncmp(source_url, "http://", 7) == 0 ||
                                 strncmp(source_url, "https://", 8) == 0 ||
                                 strncmp(source_url, "ftp://", 6) == 0 ||
                                 strncmp(source_url, "file://", 7) == 0);
            int is_remote = has_url_scheme;
            int is_local = !is_remote;
            
            if (is_local) {
                // Try local source with multiple search paths
                char local_path[2048] = {0};
                int found_local = 0;
                
                // Try multiple possible locations
                const char *search_paths[] = {
                    source_url,  // Absolute or relative path as-is
                    NULL,        // Repository structure path (filled below)
                };
                
                char repo_structure_path[2048];
                if (repo_path && repo_path[0] == '/') {
                    snprintf(repo_structure_path, sizeof(repo_structure_path), 
                            "%s/%c/%s/%s-%s/%s",
                            repo_path, first_letter, pkg_name, pkg_name, version, source_url);
                    search_paths[1] = repo_structure_path;
                }

                for (size_t j = 0; j < sizeof(search_paths) / sizeof(search_paths[0]); j++) {
                    if (!search_paths[j]) continue;
                    
                    if (access(search_paths[j], F_OK) == 0) {
                        strncpy(local_path, search_paths[j], sizeof(local_path) - 1);
                        found_local = 1;
                        log_info("Found local source: %s", local_path);
                        break;
                    }
                }
                
                if (found_local) {
                    char cp_cmd[4096];
                    snprintf(cp_cmd, sizeof(cp_cmd), "cp \"%s\" \"%s\"", local_path, output_path);
                    if (system(cp_cmd) == 0) {
                        log_info("Copied local file to: %s", output_path);
                        download_success = 1;
                        break;
                    } else {
                        log_warn("Failed to copy local file from %s", local_path);
                    }
                } else {
                    log_debug(2, "Local source file not found: %s", source_url);
                }
            } else {
                // Remote download
                FILE *fp = fopen(output_path, "wb");
                if (!fp) {
                    log_error("Cannot write to %s", output_path);
                    continue;
                }
                
                CURL *curl = curl_easy_init();
                if (!curl) {
                    fclose(fp);
                    continue;
                }
                
                log_info("Downloading from: %s", source_url);
                curl_easy_setopt(curl, CURLOPT_URL, source_url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
                fclose(fp);
                
                if (res == CURLE_OK) {
                    log_info("Successfully downloaded from source[%zu]: %s", i, source_url);
                    download_success = 1;
                    break;
                } else {
                    log_warn("Failed to download from source[%zu]: %s (error: %s)", 
                            i, source_url, curl_easy_strerror(res));
                    remove(output_path);
                }
            }
        }
    }
    
    if (source_array) json_decref(source_array);
    
    if (!download_success) {
        log_error("Failed to download package %s from any source.", pkg_name);
        return -1;
    }
    
    log_info("Package saved to: %s", output_path);
    return 0;
}

/**
 * Download file from URL to specified path.
 * Simple direct download without complex source logic - used by download command.
 * Supports both HTTP/HTTPS URLs and local file paths.
 * @param url Source URL or local file path
 * @param output_path Destination file path
 * @return 0 on success, -1 on error
 */
int download_url_to_file(const char *url, const char *output_path) {
    if (!url || !output_path) {
        log_error("Invalid arguments to download_url_to_file");
        return -1;
    }
    
    // Check if file already exists
    if (access(output_path, F_OK) == 0) {
        log_info("File already exists: %s", output_path);
        return 0;
    }
    
    // Determine if URL is remote or local
    int is_remote = (strncmp(url, "http://", 7) == 0 || 
                    strncmp(url, "https://", 8) == 0 ||
                    strncmp(url, "ftp://", 6) == 0);
    
    if (is_remote) {
        // Remote download via CURL
        FILE *fp = fopen(output_path, "wb");
        if (!fp) {
            log_error("Cannot write to %s", output_path);
            return -1;
        }
        
        CURL *curl = curl_easy_init();
        if (!curl) {
            fclose(fp);
            log_error("Failed to initialize CURL");
            return -1;
        }
        
        log_info("Downloading from: %s", url);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        
        if (res != CURLE_OK) {
            log_error("Download failed: %s", curl_easy_strerror(res));
            remove(output_path);
            return -1;
        }
        
        log_info("Successfully downloaded to: %s", output_path);
        return 0;
    } else {
        // Local file copy
        if (access(url, F_OK) != 0) {
            log_error("Local file not found: %s", url);
            return -1;
        }
        
        char cp_cmd[4096];
        snprintf(cp_cmd, sizeof(cp_cmd), "cp \"%s\" \"%s\"", url, output_path);
        if (system(cp_cmd) != 0) {
            log_error("Failed to copy local file: %s", url);
            return -1;
        }
        
        log_info("Copied local file to: %s", output_path);
        return 0;
    }
}