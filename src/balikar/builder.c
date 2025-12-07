//
// Created by placeja5 on 11/14/25.
//

#include "includes/builder.h"

// Lokální stav
static int g_staging_enabled = 1;  // default zapnuto
static char *g_staging_dir = NULL;  // NULL = použít STAGING_DIR z common.h
static char *g_build_tmp_dir = NULL;  // NULL = použít /tmp

/**
 * Enables or disables staging during package build process.
 * When enabled, DESTDIR environment variable is set to staging directory.
 * @param enabled Non-zero to enable staging, zero to disable
 */
void set_staging_enabled(int enabled) {
    g_staging_enabled = enabled ? 1 : 0;
}

/**
 * Sets the custom staging directory path.
 * If NULL, the default STAGING_DIR from common.h will be used.
 * @param dir Path to staging directory or NULL for default
 */
void set_staging_dir(const char *dir) {
    if (g_staging_dir) free(g_staging_dir);
    g_staging_dir = dir ? strdup(dir) : NULL;
}

/**
 * Sets the custom build temporary directory path.
 * If NULL, /tmp will be used as the base for build directories.
 * @param dir Path to temporary directory or NULL for default /tmp
 */
void set_build_tmp_dir(const char *dir) {
    if (g_build_tmp_dir) free(g_build_tmp_dir);
    g_build_tmp_dir = dir ? strdup(dir) : NULL;
}

/**
 * Detects compression type of an archive by reading magic bytes.
 * Supports gzip, xz, and bzip2 compression formats.
 * @param fp File pointer to the archive (will be rewound after reading)
 * @return Compression type: 0=none/unknown, 1=xz, 2=bz2, 3=gz
 */
static int detect_compression(FILE *fp) {
	unsigned char buf[6];
	size_t r = fread(buf, 1, sizeof(buf), fp);
	rewind(fp);  // Reset file position after reading magic bytes
	if (r < 2) return 0;
	
	// Check for gzip magic bytes: 1F 8B
	if (buf[0] == 0x1F && buf[1] == 0x8B) return 3; // gz
	
	// Check for xz magic bytes: FD 37 7A 58 5A 00
	if (r >= 6 && buf[0] == 0xFD && buf[1] == 0x37 && buf[2] == 0x7A && buf[3] == 0x58 && buf[4] == 0x5A && buf[5] == 0x00) return 1; // xz
	
	// Check for bzip2 magic bytes: 'BZ'
	if (buf[0] == 'B' && buf[1] == 'Z') return 2; // bz2
	
	return 0; // unknown / plain
}

/**
 * Recursively searches for build script (build.sh) or build system files.
 * Looks for build.sh, Makefile, or configure files within directory tree.
 * @param dir Directory to search in
 * @param depth Current recursion depth (limited to 3 levels)
 * @return Allocated path to directory containing build files, or NULL if not found
 */
static char *find_build_or_source(const char *dir, int depth) {
	// Limit recursion depth to avoid excessive searching
	if (depth > 3) return NULL;
	
	// First priority: check for build.sh script in current directory
	char build_path[MAX_PATH_LEN];
	snprintf(build_path, sizeof(build_path), "%s/build.sh", dir);
	if (access(build_path, F_OK) == 0) {
		return strdup(dir);
	}
	
	// Second priority: check for Makefile or configure script
	char make_path[MAX_PATH_LEN];
	snprintf(make_path, sizeof(make_path), "%s/Makefile", dir);
	char conf_path[MAX_PATH_LEN];
	snprintf(conf_path, sizeof(conf_path), "%s/configure", dir);
	if (access(make_path, F_OK) == 0 || access(conf_path, F_OK) == 0) {
		return strdup(dir);
	}
	
	// Third priority: recursively search subdirectories
	DIR *d = opendir(dir);
	if (!d) return NULL;
	struct dirent *ent;
	char *result = NULL;
	while ((ent = readdir(d)) != NULL) {
		// Skip hidden directories
		if (ent->d_name[0] == '.') continue;
		
		char path[MAX_PATH_LEN];
		snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
		struct stat st;
		
		// Recursively check subdirectories
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			result = find_build_or_source(path, depth + 1);
			if (result) break;  // Stop on first match
		}
	}
	closedir(d);
	return result;
}

/**
 * Finds the source directory within an extracted archive.
 * First tries to locate .build, Makefile, or configure files recursively.
 * Falls back to returning the first subdirectory if no build files found.
 * @param tmp_dir Root directory of extracted archive
 * @return Allocated path to source directory, or NULL on error
 */
char *find_source_path(const char *tmp_dir) {
	if (!tmp_dir) return NULL;
	
	// Strategy 1: Try recursive search for build files
	char *found = find_build_or_source(tmp_dir, 0);
	if (found) return found;
	
	// Strategy 2: Fallback to first subdirectory
	// (common pattern: archives contain a top-level directory)
	DIR *d = opendir(tmp_dir);
	if (!d) return strdup(tmp_dir);
	
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		// Skip hidden entries
		if (ent->d_name[0] == '.') continue;
		
		char path[MAX_PATH_LEN];
		snprintf(path, sizeof(path), "%s/%s", tmp_dir, ent->d_name);
		struct stat st;
		
		// Return first directory found
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			closedir(d);
			return strdup(path);
		}
	}
	closedir(d);
	
	// Strategy 3: Last resort - use tmp_dir itself
	return strdup(tmp_dir);
}

/**
 * Applies extra files (patches, custom build scripts) to the source directory.
 * Copies files from cache/extras/<pkg_name>/ to the build directory.
 * build.sh scripts are copied to source_dir, other files to parent directory.
 * @param extras JSON array of extra file names to apply
 * @param pkg_name Package name (used to locate extras directory)
 * @param source_dir Source directory where extras should be applied
 */
void apply_extras(json_t *extras, const char *pkg_name, const char *source_dir) {
	if (!extras || !json_is_array(extras) || !pkg_name || !source_dir) return;
	
	// Locate the extras directory in cache
	char *cache_dir = get_cache_dir();
	if (!cache_dir) return;
	char patch_root[2048];
	snprintf(patch_root, sizeof(patch_root), "%s/extras/%s", cache_dir, pkg_name);
	DIR *d = opendir(patch_root);
	if (!d) { free(cache_dir); return; }
	
	// Get parent directory of source_dir for copying non-build files
	char parent_dir[4096];
	snprintf(parent_dir, sizeof(parent_dir), "%s/..", source_dir);
	
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;

		// Check if file should be applied:
		// 1. Always copy build.sh script
		// 2. Copy file if listed in extras JSON array
		int found = 0;
		if (strcmp(ent->d_name, "build.sh") == 0) {
			found = 1;  // build.sh always copied
		} else {
			// Check if filename is in the extras list
			size_t i; json_t *item;
			json_array_foreach(extras, i, item) {
				if (json_is_string(item) && strstr(json_string_value(item), ent->d_name)) { 
					found = 1; 
					break; 
				}
			}
		}
		if (!found) continue;
		
		char extra_file[4096];
		snprintf(extra_file, sizeof(extra_file), "%s/%s", patch_root, ent->d_name);
		
		// Determine destination:
		// - build.sh script goes to source_dir (where it will be executed)
		// - Other files (patches, configs) go to parent directory
		char dest_path[4096];
		if (strcmp(ent->d_name, "build.sh") == 0) {
			snprintf(dest_path, sizeof(dest_path), "%s/%s", source_dir, ent->d_name);
		} else {
			snprintf(dest_path, sizeof(dest_path), "%s/%s", parent_dir, ent->d_name);
		}
		
		char cmd[8192];
		snprintf(cmd, sizeof(cmd), "cp -a '%s' '%s'", extra_file, dest_path);
		log_debug(1, "Copying from '%s' to '%s'", extra_file, dest_path);
		log_debug(1, "Running: %s", cmd);
		int rc = system(cmd);
		log_debug(1, "Copy command returned: %d", rc);
		if (rc == 0) {
			log_info("Copied extra file: %s -> %s", ent->d_name, 
			         strcmp(ent->d_name, "build.sh") == 0 ? source_dir : parent_dir);
			// Verify file was actually copied
			struct stat st;
			if (stat(dest_path, &st) == 0) {
				log_debug(1, "Verified copy: %s (size: %ld)", dest_path, st.st_size);
			} else {
				log_warn("Copy reported success but file doesn't exist: %s", dest_path);
			}
		} else {
			log_warn("Copy failed (%d): %s", rc, ent->d_name);
		}
	}
	closedir(d);
	free(cache_dir);
}

/**
 * Executes a build callback function in a chroot environment.
 * Forks a child process, chroots to specified directory, and runs the callback.
 * Useful for isolated builds with controlled filesystem access.
 * @param root Root directory for chroot
 * @param func Callback function to execute in chroot
 * @param data User data to pass to callback
 * @return Exit status of callback, or -1 on error
 */
int run_with_chroot(const char *root, build_callback func, void *data) {
	if (!root || !func) return -1;
	
	// Fork to create isolated process for chroot
	pid_t pid = fork();
	if (pid < 0) {
		log_error("Fork failed for chroot isolation");
		return -1;
	}
	
	if (pid == 0) {
		// Child process: perform chroot and execute callback
		if (chroot(root) != 0) {
			log_error("Chroot failed: %s", strerror(errno));
			exit(1);
		}
		
		// Change to root directory within chroot
		if (chdir("/") != 0) {
			log_error("Chdir after chroot failed");
			exit(1);
		}
		
		// Execute callback and exit with its return code
		int rc = func(data);
		exit(rc);
	}
	
	// Parent process: wait for child to complete
	int status;
	if (waitpid(pid, &status, 0) < 0) {
		log_error("Waitpid failed");
		return -1;
	}
	
	// Return child's exit status
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return -1;
}

/**
 * Extracts an archive file to a directory.
 * Automatically detects compression type and uses appropriate tar flags.
 * Supports plain tar, gzip, xz, and bzip2 compressed archives.
 * @param fp File pointer to archive (will be read from start)
 * @param tmp_dir Target directory for extraction
 * @return 0 on success, -1 on error
 */
static int extract_archive(FILE *fp, const char *tmp_dir) {
	if (!fp || !tmp_dir) return -1;
	
	rewind(fp);
	// Detect compression format by magic bytes
	int comp = detect_compression(fp);
	
	// Select appropriate tar flags based on compression
	const char *tarflag = "xf"; // plain tar
	if (comp == 1) tarflag = "xJf"; // xz
	else if (comp == 2) tarflag = "xjf"; // bz2
	else if (comp == 3) tarflag = "xzf"; // gz
	
	// Open tar process with pipe for stdin
	char cmd[4096];
	snprintf(cmd, sizeof(cmd), "tar %s - -C '%s'", tarflag, tmp_dir);
	FILE *tar = popen(cmd, "w");
	if (!tar) return -1;
	
	// Stream archive data to tar process
	unsigned char buf[8192];
	size_t r;
	while ((r = fread(buf, 1, sizeof(buf), fp)) > 0) {
		if (fwrite(buf, 1, r, tar) != r) {
			pclose(tar);
			return -1;
		}
	}
	
	// Wait for tar to complete
	int status = pclose(tar);
	if (status != 0) return -1;
	
	// Check if this is a balikar package with nested source archive
	// Look for index.json in first subdirectory
	DIR *d = opendir(tmp_dir);
	if (d) {
		struct dirent *ent;
		char first_subdir[2048] = {0};
		
		// Find first subdirectory
		while ((ent = readdir(d)) != NULL) {
			if (ent->d_name[0] == '.') continue;
			
			char test_path[2048];
			snprintf(test_path, sizeof(test_path), "%s/%s", tmp_dir, ent->d_name);
			
			struct stat st;
			if (stat(test_path, &st) == 0 && S_ISDIR(st.st_mode)) {
				strncpy(first_subdir, test_path, sizeof(first_subdir) - 1);
				break;
			}
		}
		closedir(d);
		
		// Check for index.json
		if (first_subdir[0]) {
			char index_json_path[2560];
			snprintf(index_json_path, sizeof(index_json_path), "%s/index.json", first_subdir);
			
			if (access(index_json_path, F_OK) == 0) {
				log_debug(1, "Found index.json, checking for nested source archive");
				
				// Load index.json
				json_error_t error;
				json_t *index_root = json_load_file(index_json_path, 0, &error);
				if (index_root) {
					json_t *source_array = json_object_get(index_root, "source");
					if (source_array && json_is_array(source_array) && json_array_size(source_array) > 0) {
						json_t *first_source = json_array_get(source_array, 0);
						json_t *url_obj = json_object_get(first_source, "url");
						
						if (url_obj && json_is_string(url_obj)) {
							const char *source_file = json_string_value(url_obj);
							char nested_tar_path[2560];
							int needs_download = 0;
							
							// Check if it's a remote URL (contains ://)
							if (strstr(source_file, "://") != NULL) {
								// It's a remote URL - need to download it
								// Extract filename from URL
								const char *last_slash = strrchr(source_file, '/');
								const char *filename = last_slash ? last_slash + 1 : source_file;
								snprintf(nested_tar_path, sizeof(nested_tar_path), "%s/%s", first_subdir, filename);
								
								log_info("Downloading source data from: %s", source_file);
								
								// Download using downloader function
								if (download_url_to_file(source_file, nested_tar_path) == 0) {
									log_debug(1, "Source data downloaded successfully to: %s", nested_tar_path);
								} else {
									log_warn("Failed to download source data from: %s", source_file);
									json_decref(index_root);
									return 0;  // Continue without failing
								}
							} else {
								// It's a relative path - check if file exists locally
								snprintf(nested_tar_path, sizeof(nested_tar_path), "%s/%s", first_subdir, source_file);
								
								if (access(nested_tar_path, F_OK) != 0) {
									log_debug(1, "Local source file not found: %s", source_file);
									json_decref(index_root);
									return 0;  // Continue without failing
								}
							}
							
							// Now extract the nested archive (whether downloaded or local)
							if (access(nested_tar_path, F_OK) == 0) {
								log_info("Extracting source data: %s", nested_tar_path);
								
								// Extract nested tar to the same directory
								char extract_cmd[4096];
								snprintf(extract_cmd, sizeof(extract_cmd), "tar -xf '%s' -C '%s' 2>/dev/null", nested_tar_path, first_subdir);
								
								int extract_status = system(extract_cmd);
								if (extract_status == 0) {
									log_debug(1, "Source data extracted successfully");
								} else {
									log_warn("Failed to extract source data (status=%d)", extract_status);
								}
							}
						}
					}
					json_decref(index_root);
				}
			}
		}
	}
	
	return 0;
}

/**
 * Builds and installs a package from source archive.
 * This is the main build orchestration function that:
 * 1. Extracts the archive to temporary directory
 * 2. Locates the source directory with build files
 * 3. Applies extra files (patches, custom build scripts)
 * 4. Sets up build environment (staging, MAKEFLAGS)
 * 5. Executes build script or fallback configure+make
 * 6. Handles errors and cleanup
 * @param trans Transaction context (currently unused)
 * @param data Repository data for the package
 * @param p Package ID
 * @param fp File pointer to source archive
 * @param pool Dependency resolution pool
 * @return 0 on success, -1 on error
 */
int commit_element(Transaction *trans, Repodata *data, Id p, FILE *fp, Pool *pool) {
	(void)trans; // Reserved for future use (e.g., transaction ordering)
	if (!data || !fp || !pool) return -1;

	// Extract package metadata
	int is_group = get_custom_is_group(data, p, pool);
	int no_package = get_custom_no_package(data, p, pool);
	json_t *extras = get_custom_extras(data, p, pool);

	char *pkg_name = pool_id2str(pool, pool->solvables[p].name);
	
	// Skip virtual packages (no actual files to build)
	if (no_package) {
		log_info("Skipping virtual/no_package: %s", pkg_name);
		if (fp) fclose(fp);
		if (extras) json_decref(extras);
		return 0;
	}

	// Create temporary build directory using configured tmp location
	const char *tmp_base = g_build_tmp_dir ? g_build_tmp_dir : "/tmp";
	char tmpl[512];
	snprintf(tmpl, sizeof(tmpl), "%s/balikar_build_%s_XXXXXX", tmp_base, pkg_name ? pkg_name : "pkg");
	char *build_dir = strdup(tmpl);
	if (!build_dir) return -1;
	
	// Create unique directory with mkdtemp (replaces XXXXXX with random chars)
	if (!mkdtemp(build_dir)) {
		log_error("mkdtemp failed for %s", pkg_name);
		free(build_dir);
		return -1;
	}
	log_info("Build dir: %s", build_dir);

	// Extract source archive to build directory
	if (extract_archive(fp, build_dir) != 0) {
		log_error("Archive extraction failed for %s", pkg_name);
		goto fail;
	}

	// Locate the actual source directory within extracted archive
	char *source_dir = find_source_path(build_dir);
	if (!source_dir) {
		log_error("Source dir not found for %s", pkg_name);
		goto fail;
	}
	log_info("Source dir: %s", source_dir);

	// Apply extra files (patches, custom build scripts)
	if (extras) apply_extras(extras, pkg_name, source_dir);

	// Configure build environment
	// Set DESTDIR for staging installation if enabled
	// Also create the staging directory if it doesn't exist
	if (g_staging_enabled) {
		const char *destdir = g_staging_dir ? g_staging_dir : STAGING_DIR;
		
		// Create staging directory if it doesn't exist
		char mkdir_cmd[512];
		snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", destdir);
		system(mkdir_cmd);
		
		setenv("DESTDIR", destdir, 1);
		log_debug(1, "DESTDIR set to: %s", destdir);
	} else {
		unsetenv("DESTDIR");
	}
	
	// Set MAKEFLAGS for parallel compilation
	long nproc = sysconf(_SC_NPROCESSORS_ONLN);
	if (nproc < 1) nproc = 1;
	char mf[64]; snprintf(mf, sizeof(mf), "-j%ld", nproc);
	setenv("MAKEFLAGS", mf, 1);

	// Handle group packages (data-only, no compilation needed)
	if (is_group) {
		log_info("is_group: extraction only for %s", pkg_name);
		goto done;
	}

	// Execute build process
	// Change to source directory for build
	if (chdir(source_dir) != 0) {
		log_error("Cannot chdir to %s", source_dir);
		goto fail_chdir;
	}
	
	// Determine build strategy:
	// 1. If build.sh script exists, use it (preferred)
	// 2. Otherwise use standard configure+make (fallback)
	log_debug(1, "Checking for build.sh in directory: %s", source_dir);
	char build_fullpath[4096];
	snprintf(build_fullpath, sizeof(build_fullpath), "%s/build.sh", source_dir);
	log_debug(1, "Full build.sh path would be: %s", build_fullpath);
	
	struct stat build_st;
	int have_build = access("build.sh", X_OK) == 0;
	if (stat("build.sh", &build_st) == 0) {
		log_debug(1, "build.sh exists: size=%ld, executable=%d", build_st.st_size, have_build);
	} else {
		log_debug(1, "build.sh does NOT exist in %s (errno=%d)", source_dir, errno);
	}
	
	// Prepare build command with DESTDIR explicitly set in the command string
	// This ensures DESTDIR is available even if setenv() doesn't propagate correctly
	char build_cmd[2048];
	const char *destdir = g_staging_enabled ? (g_staging_dir ? g_staging_dir : STAGING_DIR) : "";
	
	if (have_build) {
		if (g_staging_enabled) {
			snprintf(build_cmd, sizeof(build_cmd), "DESTDIR='%s' bash -e build.sh > build.log 2>&1", destdir);
		} else {
			snprintf(build_cmd, sizeof(build_cmd), "bash -e build.sh > build.log 2>&1");
		}
	} else {
		// Fallback: standard configure+make+install with DESTDIR
		if (g_staging_enabled) {
			snprintf(build_cmd, sizeof(build_cmd), "./configure --prefix=/usr >/dev/null 2>&1 && make >/dev/null 2>&1 && make DESTDIR='%s' install >/dev/null 2>&1", destdir);
		} else {
			snprintf(build_cmd, sizeof(build_cmd), "./configure --prefix=/usr >/dev/null 2>&1 && make >/dev/null 2>&1 && make install >/dev/null 2>&1");
		}
	}
	
	log_info("Running build: %s", have_build ? "build.sh" : "fallback configure+make");
	log_debug(1, "Build command: %s", build_cmd);
	
	// Execute build command
	int rc = system(build_cmd);
	if (rc != 0) {
		log_warn("Build failed (%d) for %s", rc, pkg_name);
		
		// Offer retry on failure
		if (user_confirm("Retry build? [y/N]", 0)) {
			rc = system(build_cmd);
		}
		
		if (rc != 0) {
			log_error("Build permanently failed for %s", pkg_name);
			goto fail_chdir;
		}
	}

done:
	// Successful completion - cleanup resources
	free(source_dir);
	fclose(fp);
	
	// Change directory away from build_dir before removing it
	// (some systems don't allow deleting current working directory)
	chdir("/tmp");
	
	// Remove temporary build directory
	char rmcmd[512];
	snprintf(rmcmd, sizeof(rmcmd), "rm -rf '%s'", build_dir);
	system(rmcmd);
	free(build_dir);
	if (extras) json_decref(extras);
	return 0;

fail_chdir:
	free(source_dir);
fail:
	// Error handling - cleanup resources
	fclose(fp);
	
	// Change directory away from build_dir before removing it
	chdir("/tmp");
	
	// Remove temporary build directory
	char rmcmd2[512];
	snprintf(rmcmd2, sizeof(rmcmd2), "rm -rf '%s'", build_dir);
	system(rmcmd2);
	free(build_dir);
	if (extras) json_decref(extras);
	return -1;
}

