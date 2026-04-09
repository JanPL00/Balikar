//
// Command: download - Download packages without installing
// Facade over solver + downloader subsystems
//

#include "../includes/command_context.h"
#include "../includes/solver.h"
#include "../includes/downloader.h"
#include "../includes/parser.h"
#include "../../utils/includes/print.h"
#include "../../utils/includes/cli_helpers.h"
#include "../../common/config.h"
#include <stdlib.h>
#include <string.h>

/**
 * Helper: Get output directory for downloads.
 * Returns allocated string - caller must free.
 */
static char* get_download_output_dir(CommandContext *ctx) {
    // Use --output-dir if specified
    if (ctx->config.output_dir_allocated) {
        return strdup(ctx->config.output_dir_allocated);
    }
    
    // Find first enabled local repository from config
    init_repo_config();
    repo_config_t *repos = get_enabled_repos();
    for (repo_config_t *r = repos; r; r = r->next) {
        if (r->enabled) {
            // Check if URL is local path (not http/https/ftp)
            if (strncmp(r->url, "http://", 7) != 0 && 
                strncmp(r->url, "https://", 8) != 0 &&
                strncmp(r->url, "ftp://", 6) != 0) {
                log_info("Using local repository from config: %s", r->url);
                return strdup(r->url);
            }
        }
    }
    
    // Fallback to ./repo
    log_info("No local repository found in config, using default: ./repo");
    return strdup("./repo");
}

/**
 * Helper: Download single package without dependencies.
 */
static int download_single_package(CommandContext *ctx, char *output_directory) {
    Pool *pool = ctx->resources.pool;
    Repo *repo = ctx->resources.repo;
    const char *pkg_name = ctx->config.package_name;
    
    // Find package in pool by name
    Id name_id = pool_str2id(pool, pkg_name, 0);
    if (!name_id) {
        log_error("Package '%s' not found in repository", pkg_name);
        return 1;
    }
    
    // Find the solvable with this name
    Solvable *s = NULL;
    Id p = 0;
    for (Id i = 2; i < pool->nsolvables; i++) {
        Solvable *candidate = pool_id2solvable(pool, i);
        if (candidate->name == name_id) {
            s = candidate;
            p = i;
            break;
        }
    }
    
    if (!s) {
        log_error("Package '%s' not found in pool", pkg_name);
        return 1;
    }
    
    const char *version = pool_id2str(pool, s->evr);
    log_info("Found package: %s-%s", pkg_name, version);
    
    // Get source URL from sources array
    Repodata *data = repo_id2repodata(repo, 1);
    const char *download_url = NULL;
    static char full_url[4096];
    
    json_t *source_array = get_custom_source_array(data, p, pool);
    
    if (source_array && json_is_array(source_array)) {
        size_t source_count = json_array_size(source_array);
        for (size_t i = 0; i < source_count; i++) {
            json_t *source_obj = json_array_get(source_array, i);
            if (!source_obj || !json_is_object(source_obj)) continue;
            
            json_t *url_json = json_object_get(source_obj, "url");
            if (!url_json || !json_is_string(url_json)) continue;
            
            const char *source_url = json_string_value(url_json);
            
            // Check if URL is absolute (has scheme)
            int is_absolute = (strncmp(source_url, "http://", 7) == 0 ||
                             strncmp(source_url, "https://", 8) == 0 ||
                             strncmp(source_url, "ftp://", 6) == 0 ||
                             strncmp(source_url, "file://", 7) == 0);
            
            if (is_absolute) {
                download_url = source_url;
                break;
            }
        }
    }
    
    if (!download_url) {
        // Fallback to location field
        const char *location = solvable_get_location(s, NULL);
        if (location) {
            const char *repo_path = command_context_get_repo_path(ctx);
            snprintf(full_url, sizeof(full_url), "%s/%s", repo_path, location);
            download_url = full_url;
        }
    }
    
    if (!download_url) {
        log_error("No download URL for %s", pkg_name);
        if (source_array) json_decref(source_array);
        return 1;
    }
    
    // Detect file extension
    const char *ext = ".tar.bz2";
    const char *last_dot = strrchr(download_url, '.');
    if (last_dot) {
        const char *last_slash = strrchr(download_url, '/');
        const char *filename = last_slash ? last_slash + 1 : download_url;
        
        if (strstr(filename, ".tar.xz")) ext = ".tar.xz";
        else if (strstr(filename, ".tar.gz")) ext = ".tar.gz";
        else if (strstr(filename, ".tar.bz2")) ext = ".tar.bz2";
        else if (strstr(filename, ".tgz")) ext = ".tgz";
        else ext = last_dot;
    }
    
    // Create output path
    char output_path[2048];
    snprintf(output_path, sizeof(output_path), "%s/%s-%s%s", 
             output_directory, pkg_name, version, ext);
    
    log_info("Downloading %s from: %s", pkg_name, download_url);
    int rc = download_url_to_file(download_url, output_path);
    
    if (source_array) json_decref(source_array);
    
    if (rc != 0) {
        log_error("Download failed for %s", pkg_name);
        return 1;
    }
    
    log_info("Successfully downloaded %s to %s", pkg_name, output_path);
    return 0;
}

/**
 * Execute 'download' command.
 * Downloads packages with or without dependencies based on --no-deps flag.
 */
int cmd_download_execute(CommandContext *ctx) {
    if (!ctx || !ctx->config.package_name) {
        log_error("Package name required for download command");
        return 1;
    }
    
    // Ensure pool and repo are loaded
    Pool *pool = command_context_ensure_pool(ctx);
    Repo *repo = command_context_ensure_repo(ctx);
    if (!pool || !repo) {
        log_error("Failed to initialize pool/repo for download");
        return 1;
    }
    
    const char *pkg_name = ctx->config.package_name;
    log_info("Starting download workflow for package: %s", pkg_name);
    
    if (ctx->config.no_deps) {
        log_info("Downloading without dependencies (--no-deps)");
    }
    
    // Get output directory
    char *output_directory = get_download_output_dir(ctx);
    if (!output_directory) {
        log_error("Failed to determine output directory");
        return 1;
    }
    
    log_info("Output directory: %s", output_directory);
    
    // Handle --no-deps: download only specified package
    if (ctx->config.no_deps) {
        int rc = download_single_package(ctx, output_directory);
        free(output_directory);
        return rc;
    }
    
    // Download with dependencies - create solver
    Solver *solv = ctx->subsystems.create_solver(pool);
    if (!solv) {
        log_error("Failed to create solver");
        free(output_directory);
        return 1;
    }
    
    // Solve dependencies (use "/" as root - downloads are independent)
    Queue *solution = ctx->subsystems.solve_dependencies(
        solv, pkg_name, pool, repo, "/", get_custom_mkdeps
    );
    
    if (!solution) {
        log_error("Dependency resolution failed for %s", pkg_name);
        solver_free(solv);
        free(output_directory);
        return 1;
    }
    
    // Create transaction
    Transaction *trans = solver_create_transaction(solv);
    if (!trans) {
        log_error("Failed to create transaction");
        queue_free(solution);
        solver_free(solv);
        free(output_directory);
        return 1;
    }
    
    // Display transaction
    if (trans->steps.count == 0) {
        log_info("No packages to download - package may already be resolved");
    } else {
        display_transaction(trans);
    }
    
    // Confirm with user
    if (!user_confirm("Proceed with download?", 1)) {
        log_info("Download cancelled by user");
        transaction_free(trans);
        queue_free(solution);
        solver_free(solv);
        free(output_directory);
        return 0;
    }
    
    // Download all packages in transaction
    log_info("Downloading packages to: %s", output_directory);
    transaction_order(trans, 0);
    
    int download_count = 0;
    int exit_code = 0;
    
    for (int i = 0; i < trans->steps.count; i++) {
        Id p = trans->steps.elements[i];
        Id type = transaction_type(trans, p, SOLVER_TRANSACTION_RPM_ONLY);
        
        if (type != SOLVER_TRANSACTION_INSTALL && 
            type != SOLVER_TRANSACTION_MULTIINSTALL) {
            continue;
        }
        
        Solvable *s = pool_id2solvable(pool, p);
        const char *sname = pool_id2str(pool, s->name);
        const char *version = pool_id2str(pool, s->evr);
        
        // Get source URL
        Repodata *data = repo_id2repodata(repo, 1);
        const char *download_url = NULL;
        static char full_url_buf[4096];
        
        json_t *source_array = get_custom_source_array(data, p, pool);
        
        if (source_array && json_is_array(source_array)) {
            size_t source_count = json_array_size(source_array);
            for (size_t j = 0; j < source_count; j++) {
                json_t *source_obj = json_array_get(source_array, j);
                if (!source_obj || !json_is_object(source_obj)) continue;
                
                json_t *url_json = json_object_get(source_obj, "url");
                if (!url_json || !json_is_string(url_json)) continue;
                
                const char *source_url = json_string_value(url_json);
                
                int is_absolute = (strncmp(source_url, "http://", 7) == 0 ||
                                 strncmp(source_url, "https://", 8) == 0 ||
                                 strncmp(source_url, "ftp://", 6) == 0 ||
                                 strncmp(source_url, "file://", 7) == 0);
                
                if (is_absolute) {
                    download_url = source_url;
                    break;
                }
            }
        }
        
        if (!download_url) {
            const char *location = solvable_get_location(s, NULL);
            if (location) {
                const char *repo_path = command_context_get_repo_path(ctx);
                snprintf(full_url_buf, sizeof(full_url_buf), "%s/%s", repo_path, location);
                download_url = full_url_buf;
            }
        }
        
        if (!download_url) {
            log_warn("No download URL for %s, skipping", sname);
            if (source_array) json_decref(source_array);
            continue;
        }
        
        // Detect extension
        const char *ext = ".tar.bz2";
        const char *last_dot = strrchr(download_url, '.');
        if (last_dot) {
            const char *last_slash = strrchr(download_url, '/');
            const char *filename = last_slash ? last_slash + 1 : download_url;
            
            if (strstr(filename, ".tar.xz")) ext = ".tar.xz";
            else if (strstr(filename, ".tar.gz")) ext = ".tar.gz";
            else if (strstr(filename, ".tar.bz2")) ext = ".tar.bz2";
            else if (strstr(filename, ".tgz")) ext = ".tgz";
            else ext = last_dot;
        }
        
        // Create output path
        char output_path[2048];
        snprintf(output_path, sizeof(output_path), "%s/%s-%s%s", 
                 output_directory, sname, version, ext);
        
        log_info("Downloading %s from %s", sname, download_url);
        int rc = download_url_to_file(download_url, output_path);
        
        if (source_array) json_decref(source_array);
        
        if (rc != 0) {
            log_error("Download failed for %s", sname);
            exit_code = 1;
            break;
        }
        
        download_count++;
        log_info("Successfully downloaded %s", sname);
    }
    
    if (exit_code == 0) {
        log_info("Download complete: %d packages downloaded to %s", 
                 download_count, output_directory);
    }
    
    transaction_free(trans);
    queue_free(solution);
    solver_free(solv);
    free(output_directory);
    
    return exit_code;
}
