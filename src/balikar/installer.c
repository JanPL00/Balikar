//
// Created by placeja5 on 11/20/25.
//

#include "includes/installer.h"


/**
 * Builds the database directory path for a given chroot.
 * Creates path: {chroot}/var/lib/balikar
 * 
 * @param chroot_path Root directory (e.g., "/" or "/tmp/test_root")
 * @param buffer Buffer to store the result
 * @param buffer_size Size of the buffer
 */
static void get_db_dir_path(const char *chroot_path, char *buffer, size_t buffer_size) {
    if (strcmp(chroot_path, "/") == 0) {
        snprintf(buffer, buffer_size, "/var/lib/balikar");
    } else {
        snprintf(buffer, buffer_size, "%s/var/lib/balikar", chroot_path);
    }
}

/**
 * Builds the database file path for a given chroot.
 * Creates path: {chroot}/var/lib/balikar/installed.json
 * 
 * @param chroot_path Root directory (e.g., "/" or "/tmp/test_root")
 * @param buffer Buffer to store the result
 * @param buffer_size Size of the buffer
 */
static void get_db_file_path(const char *chroot_path, char *buffer, size_t buffer_size) {
    if (strcmp(chroot_path, "/") == 0) {
        snprintf(buffer, buffer_size, "/var/lib/balikar/installed.json");
    } else {
        snprintf(buffer, buffer_size, "%s/var/lib/balikar/installed.json", chroot_path);
    }
}

/**
 * Ensures the database directory exists.
 * Creates {chroot}/var/lib/balikar if it doesn't exist.
 * 
 * @param chroot_path Root directory for the installation
 */
static int ensure_db_dir(const char *chroot_path) {
    char db_dir[PATH_MAX];
    get_db_dir_path(chroot_path, db_dir, sizeof(db_dir));
    
    struct stat st;
    if (stat(db_dir, &st) != 0) {
        // Create parent directories recursively
        char mkdir_cmd[PATH_MAX + 20];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", db_dir);
        if (system(mkdir_cmd) != 0) {
            log_error("Failed to create database directory: %s", db_dir);
            return -1;
        }
        log_debug(1, "Created database directory: %s", db_dir);
    }
    return 0;
}

/**
 * Loads the installed packages database.
 * Creates an empty database if it doesn't exist.
 * 
 * @param chroot_path Root directory for the installation
 * @return JSON object containing packages, or NULL on error
 *         Caller must json_decref() the returned object
 */
static json_t* load_installed_db(const char *chroot_path) {
    if (ensure_db_dir(chroot_path) != 0) {
        return NULL;
    }
    
    char db_path[PATH_MAX];
    get_db_file_path(chroot_path, db_path, sizeof(db_path));
    
    FILE *fp = fopen(db_path, "r");
    if (!fp) {
        // Database doesn't exist yet - create empty one
        log_debug(1, "Database doesn't exist, creating empty one");
        json_t *root = json_object();
        json_object_set_new(root, "packages", json_object());
        return root;
    }
    
    json_error_t error;
    json_t *root = json_loadf(fp, 0, &error);
    fclose(fp);
    
    if (!root) {
        log_error("Failed to parse installed database: %s", error.text);
        return NULL;
    }
    
    return root;
}

/**
 * Saves the installed packages database to disk.
 * 
 * @param root JSON object to save
 * @param chroot_path Root directory for the installation
 * @return 0 on success, -1 on error
 */
static int save_installed_db(json_t *root, const char *chroot_path) {
    if (ensure_db_dir(chroot_path) != 0) {
        return -1;
    }
    
    char db_path[PATH_MAX];
    get_db_file_path(chroot_path, db_path, sizeof(db_path));
    
    if (json_dump_file(root, db_path, JSON_INDENT(2)) != 0) {
        log_error("Failed to save installed database");
        return -1;
    }
    
    log_debug(1, "Saved installed database to %s", db_path);
    return 0;
}

/**
 * Collects all files in a directory recursively.
 * 
 * @param dir_path Directory to scan
 * @param files JSON array to append file paths to
 * @param base_len Length of base path to strip from results
 */
static void collect_files_recursive(const char *dir_path, json_t *files, size_t base_len) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        log_debug(2, "Cannot open directory: %s", dir_path);
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // Recurse into subdirectory
            collect_files_recursive(full_path, files, base_len);
        } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            // Add file or symlink to list (strip base path)
            const char *relative_path = full_path + base_len;
            json_array_append_new(files, json_string(relative_path));
        }
    }
    
    closedir(dir);
}

/**
 * Collects file manifest from staging directory.
 * Returns paths relative to staging dir (e.g., /usr/bin/gzip).
 * 
 * @param staging_dir Path to staging directory
 * @return JSON array of file paths, or NULL on error
 *         Caller must json_decref() the returned object
 */
static json_t* collect_file_manifest(const char *staging_dir) {
    json_t *files = json_array();
    if (!files) {
        log_error("Failed to create file manifest array");
        return NULL;
    }
    
    // Strip trailing slash for consistent path handling
    char staging_clean[PATH_MAX];
    snprintf(staging_clean, sizeof(staging_clean), "%s", staging_dir);
    size_t len = strlen(staging_clean);
    if (len > 0 && staging_clean[len - 1] == '/') {
        staging_clean[len - 1] = '\0';
        len--;
    }
    
    collect_files_recursive(staging_clean, files, len);
    
    size_t file_count = json_array_size(files);
    log_debug(1, "Collected %zu files from staging directory", file_count);
    
    return files;
}

/**
 * Finds and executes .install/install.sh script if it exists.
 * Searches recursively up to depth 3 (similar to find_source_path in builder).
 * 
 * @param staging_dir Path to staging directory
 * @param chroot_path Path to chroot (passed as env variable)
 * @return 0 if script executed or doesn't exist, -1 on script failure
 */
static int execute_install_script(const char *staging_dir, const char *chroot_path) {
    // Try common locations for .install/install.sh
    const char *search_paths[] = {
        "%s/.install/install.sh",
        "%s/*/. install/install.sh",
        "%s/*/*/.install/install.sh",
        NULL
    };
    
    char script_path[PATH_MAX];
    int found = 0;
    
    // Simple search - check direct path first
    snprintf(script_path, sizeof(script_path), "%s/.install/install.sh", staging_dir);
    if (access(script_path, X_OK) == 0) {
        found = 1;
    }
    
    if (!found) {
        log_debug(1, "No .install/install.sh script found (optional)");
        return 0;  // Not an error - script is optional
    }
    
    log_info("Executing post-install script: %s", script_path);
    
    // Set environment variables for the script
    setenv("DESTDIR", chroot_path, 1);
    setenv("CHROOT", chroot_path, 1);
    
    // Execute the script
    char cmd[PATH_MAX + 50];
    snprintf(cmd, sizeof(cmd), "bash -e '%s' > /dev/null 2>&1", script_path);
    
    int rc = system(cmd);
    if (rc != 0) {
        log_warn("Post-install script failed with code %d", rc);
        
        // Ask user if they want to continue despite script failure
        if (!user_confirm("Continue installation despite script failure?", 1)) {
            return -1;
        }
    } else {
        log_debug(1, "Post-install script executed successfully");
    }
    
    return 0;
}

/**
 * Validates chroot environment structure.
 * Checks for basic directories like /usr, /lib, /bin.
 * Warns if structure looks suspicious.
 */
int validate_chroot(const char *chroot_path) {
    struct stat st;
    
    // Check if chroot path exists
    if (stat(chroot_path, &st) != 0) {
        log_error("Chroot path does not exist: %s", chroot_path);
        return 0;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        log_error("Chroot path is not a directory: %s", chroot_path);
        return 0;
    }
    
    // Check for basic FHS directories
    const char *required_dirs[] = {"/usr", "/lib", "/bin", NULL};
    int warnings = 0;
    
    for (int i = 0; required_dirs[i] != NULL; i++) {
        char dir_path[PATH_MAX];
        snprintf(dir_path, sizeof(dir_path), "%s%s", chroot_path, required_dirs[i]);
        
        if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            log_warn("Chroot missing standard directory: %s", required_dirs[i]);
            warnings++;
        }
    }
    
    if (warnings > 0) {
        log_warn("Chroot structure looks incomplete (%d missing directories)", warnings);
        log_warn("Installing into: %s", chroot_path);
        
        if (!user_confirm("Continue installation?", 1)) {
            return 0;
        }
    }
    
    return 1;
}

/**
 * Merges staging directory to chroot using cp -a.
 * Preserves permissions, timestamps, and symbolic links.
 * 
 * @param staging_dir Path to staging directory
 * @param chroot_path Path to chroot environment
 * @return 0 on success, -1 on error
 */
static int merge_staging_to_chroot(const char *staging_dir, const char *chroot_path) {
    log_info("Merging staging directory to chroot...");
    log_debug(1, "From: %s", staging_dir);
    log_debug(1, "To: %s", chroot_path);
    
    // Validate inputs
    struct stat st;
    if (stat(staging_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_error("Staging directory does not exist or is not a directory: %s", staging_dir);
        return -1;
    }
    
    if (stat(chroot_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_error("Chroot path does not exist or is not a directory: %s", chroot_path);
        return -1;
    }
    
    // Check if staging dir has any content
    DIR *dir = opendir(staging_dir);
    if (!dir) {
        log_error("Cannot open staging directory: %s", staging_dir);
        return -1;
    }
    
    int has_content = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            has_content = 1;
            break;
        }
    }
    closedir(dir);
    
    if (!has_content) {
        log_warn("Staging directory is empty: %s", staging_dir);
        return -1;
    }
    
    // Execute merge: cp -a staging/* chroot/
    // -a = archive mode (preserve permissions, timestamps, symlinks, recursive)
    char cmd[PATH_MAX * 2 + 50];
    snprintf(cmd, sizeof(cmd), "cp -a '%s'/* '%s'/ 2>&1", staging_dir, chroot_path);
    
    log_debug(2, "Executing: %s", cmd);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        log_error("Failed to execute cp command");
        return -1;
    }
    
    // Capture any output from cp (errors/warnings)
    char output[1024];
    int has_errors = 0;
    while (fgets(output, sizeof(output), fp) != NULL) {
        // Remove trailing newline
        size_t len = strlen(output);
        if (len > 0 && output[len-1] == '\n') {
            output[len-1] = '\0';
        }
        log_warn("cp: %s", output);
        has_errors = 1;
    }
    
    int rc = pclose(fp);
    if (rc != 0 || has_errors) {
        log_error("Merge failed (cp returned %d)", rc);
        return -1;
    }
    
    log_info("Successfully merged staging to chroot");
    return 0;
}

/**
 * Adds a package to the installed packages database.
 * 
 * @param pkg_name Package name
 * @param pkg_version Package version
 * @param files JSON array of installed files
 * @param chroot_path Root directory for the installation
 * @return 0 on success, -1 on error
 */
static int add_installed_package(const char *pkg_name, const char *pkg_version, json_t *files, const char *chroot_path) {
    json_t *db = load_installed_db(chroot_path);
    if (!db) {
        return -1;
    }
    
    json_t *packages = json_object_get(db, "packages");
    if (!packages) {
        log_error("Invalid database format: missing 'packages' key");
        json_decref(db);
        return -1;
    }
    
    // Create package entry
    json_t *pkg_entry = json_object();
    json_object_set_new(pkg_entry, "name", json_string(pkg_name));
    json_object_set_new(pkg_entry, "version", json_string(pkg_version));
    
    // Add timestamp
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    json_object_set_new(pkg_entry, "install_date", json_string(timestamp));
    
    // Add file manifest
    json_object_set_new(pkg_entry, "files", json_deep_copy(files));
    
    // Add/update package in database
    char pkg_key[256];
    snprintf(pkg_key, sizeof(pkg_key), "%s-%s", pkg_name, pkg_version);
    json_object_set_new(packages, pkg_key, pkg_entry);
    
    // Save database
    int result = save_installed_db(db, chroot_path);
    json_decref(db);
    
    if (result == 0) {
        log_debug(1, "Added package to database: %s", pkg_key);
    }
    
    return result;
}

/**
 * Main installation function.
 * Installs a package from staging directory to chroot environment.
 */
int install_package(const char *pkg_name, const char *pkg_version,
                    const char *staging_dir, const char *chroot_path) {
    if (!pkg_name || !pkg_version || !staging_dir || !chroot_path) {
        log_error("Invalid arguments to install_package");
        return -1;
    }
    
    log_info("Installing package: %s-%s", pkg_name, pkg_version);
    log_debug(1, "Staging: %s", staging_dir);
    log_debug(1, "Chroot: %s", chroot_path);
    
    // Step 1: Validate chroot environment
    if (!validate_chroot(chroot_path)) {
        log_error("Chroot validation failed");
        return -1;
    }
    
    // Step 2: Collect file manifest before merge
    json_t *files = collect_file_manifest(staging_dir);
    if (!files) {
        log_error("Failed to collect file manifest");
        return -1;
    }
    
    size_t file_count = json_array_size(files);
    if (file_count == 0) {
        log_warn("No files to install for package: %s", pkg_name);
        json_decref(files);
        return -1;
    }
    
    log_info("Installing %zu files...", file_count);
    
    // Step 3: Merge staging to chroot
    if (merge_staging_to_chroot(staging_dir, chroot_path) != 0) {
        log_error("Failed to merge staging to chroot");
        json_decref(files);
        return -1;
    }
    
    // Step 4: Execute post-install script (if exists)
    if (execute_install_script(staging_dir, chroot_path) != 0) {
        log_error("Post-install script failed");
        json_decref(files);
        return -1;
    }
    
    // Step 5: Update installed packages database
    if (add_installed_package(pkg_name, pkg_version, files, chroot_path) != 0) {
        log_error("Failed to update installed packages database");
        json_decref(files);
        return -1;
    }
    
    json_decref(files);
    log_info("Successfully installed package: %s-%s", pkg_name, pkg_version);
    return 0;
}

/**
 * Checks if a package is installed.
 * 
 * @param pkg_name Package name to check
 * @param chroot_path Root directory (defaults to "/" if NULL)
 */
int is_package_installed(const char *pkg_name, const char *chroot_path) {
    if (!pkg_name) {
        return -1;
    }
    
    const char *root = chroot_path ? chroot_path : "/";
    json_t *db = load_installed_db(root);
    if (!db) {
        return -1;
    }
    
    json_t *packages = json_object_get(db, "packages");
    if (!packages) {
        json_decref(db);
        return -1;
    }
    
    // Search for package by name prefix
    const char *key;
    json_t *value;
    int found = 0;
    
    json_object_foreach(packages, key, value) {
        json_t *name_obj = json_object_get(value, "name");
        if (name_obj) {
            const char *name = json_string_value(name_obj);
            if (name && strcmp(name, pkg_name) == 0) {
                found = 1;
                break;
            }
        }
    }
    
    json_decref(db);
    return found;
}

/**
 * Retrieves list of all installed packages.
 * 
 * @param chroot_path Root directory (defaults to "/" if NULL)
 */
json_t* get_installed_packages(const char *chroot_path) {
    const char *root = chroot_path ? chroot_path : "/";
    json_t *db = load_installed_db(root);
    if (!db) {
        return NULL;
    }
    
    json_t *packages = json_object_get(db, "packages");
    if (!packages) {
        json_decref(db);
        return NULL;
    }
    
    // Return a copy of packages
    json_t *result = json_deep_copy(packages);
    json_decref(db);
    
    return result;
}

/**
 * Removes an installed package.
 */
int erase_package(const char *pkg_name, const char *chroot_path) {
    if (!pkg_name || !chroot_path) {
        log_error("Invalid arguments to erase_package");
        return -1;
    }
    
    log_info("Erasing package: %s", pkg_name);
    
    // Load database
    json_t *db = load_installed_db(chroot_path);
    if (!db) {
        return -1;
    }
    
    json_t *packages = json_object_get(db, "packages");
    if (!packages) {
        json_decref(db);
        return -1;
    }
    
    // Find package entry
    const char *key;
    json_t *value;
    const char *found_key = NULL;
    json_t *found_pkg = NULL;
    
    json_object_foreach(packages, key, value) {
        json_t *name_obj = json_object_get(value, "name");
        if (name_obj) {
            const char *name = json_string_value(name_obj);
            if (name && strcmp(name, pkg_name) == 0) {
                found_key = key;
                found_pkg = value;
                break;
            }
        }
    }
    
    if (!found_pkg) {
        log_error("Package not installed: %s", pkg_name);
        json_decref(db);
        return -1;
    }
    
    // Get file manifest
    json_t *files = json_object_get(found_pkg, "files");
    if (!files || !json_is_array(files)) {
        log_error("Invalid package entry: missing file manifest");
        json_decref(db);
        return -1;
    }
    
    // Remove files
    size_t file_count = json_array_size(files);
    size_t removed = 0;
    size_t failed = 0;
    
    for (size_t i = 0; i < file_count; i++) {
        json_t *file_obj = json_array_get(files, i);
        const char *file = json_string_value(file_obj);
        if (!file) continue;
        
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s%s", chroot_path, file);
        
        if (unlink(full_path) == 0) {
            removed++;
            log_debug(2, "Removed: %s", file);
        } else {
            if (errno != ENOENT) {  // Ignore if file doesn't exist
                log_warn("Failed to remove: %s (%s)", file, strerror(errno));
                failed++;
            }
        }
    }
    
    log_info("Removed %zu files (%zu failed)", removed, failed);
    
    // Remove package from database
    json_object_del(packages, found_key);
    
    // Save database
    int result = save_installed_db(db, chroot_path);
    json_decref(db);
    
    if (result == 0) {
        log_info("Successfully erased package: %s", pkg_name);
    }
    
    return result;
}
