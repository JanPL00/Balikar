//
// Implementace kontextu příkazu
//

#include "includes/command_context.h"
#include "includes/parser.h"
#include "includes/solver.h"
#include "includes/downloader.h"
#include "includes/builder.h"
#include "includes/installer.h"
#include "../common/config.h"
#include <stdlib.h>
#include <string.h>

/**
 * Vytvoří nový kontext příkazu s výchozími hodnotami.
 *
 * Všechny ukazatele inicializovány na NULL, příznaky na 0.
 */
CommandContext* command_context_create(void) {
    CommandContext *ctx = calloc(1, sizeof(CommandContext));
    if (!ctx) {
        log_error("Failed to allocate command context");
        return NULL;
    }
    
    // Všechna pole nulována pomocí calloc
    // Explicitně nastav exit_code na 0 pro přehlednost
    ctx->exit_code = 0;
    
    return ctx;
}

/**
 * Inicializuje fasádu subsystému s reálnými implementacemi.
 *
 * Výchozí zapojení – lze nahradit mocky pro testování.
 */
void command_context_init_subsystems(CommandContext *ctx) {
    if (!ctx) return;
    
    // Subsystém parseru
    ctx->subsystems.load_repository = load_repository;
    ctx->subsystems.generate_repodata = generate_repodata;
    
    // Subsystém solveru
    ctx->subsystems.create_solver = create_solver;
    ctx->subsystems.solve_dependencies = solve_dependencies;
    
    // Subsystém downloaderu
    ctx->subsystems.download_package = download_package;
    ctx->subsystems.download_extras = download_extras;
    
    // Subsystém builderu
    ctx->subsystems.commit_element = commit_element;
    
    // Subsystém installeru
    ctx->subsystems.install_package_with_reason = install_package_with_reason;
    ctx->subsystems.erase_package = erase_package;
    ctx->subsystems.is_package_installed = is_package_installed;
}

/**
 * Parsuje argumenty příkazové řádky do konfigurace kontextu.
 *
 * Používá getopt_long pro standardní parsování argumentů.
 */
int command_context_parse_args(CommandContext *ctx, int argc, char **argv) {
    if (!ctx) return -1;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"debug", no_argument, 0, 'd'},
        {"dry-run", no_argument, 0, 'D'},
        {"no-deps", no_argument, 0, 'N'},
        {"staging-dir", required_argument, 0, 's'},
        {"root", required_argument, 0, 'R'},
        {"build-tmp-dir", required_argument, 0, 't'},
        {"repo", required_argument, 0, 'r'},
        {"local-package", required_argument, 0, 'l'},
        {"pkg", required_argument, 0, 'p'},
        {"output-dir", required_argument, 0, 'o'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hd", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(0);
                break;
            case 'd':
                ctx->config.debug_level++;
                break;
            case 'D':
                ctx->config.dry_run = 1;
                break;
            case 'N':
                ctx->config.no_deps = 1;
                break;
            case 's':
                ctx->config.staging_dir_allocated = strdup(optarg);
                break;
            case 'R':
                ctx->config.root_path_allocated = strdup(optarg);
                break;
            case 't':
                ctx->config.build_tmp_dir_allocated = strdup(optarg);
                break;
            case 'r':
                ctx->config.repo_path_allocated = strdup(optarg);
                break;
            case 'l':
                ctx->config.local_package_allocated = strdup(optarg);
                break;
            case 'p':
                ctx->config.clean_cache_pkg_allocated = strdup(optarg);
                break;
            case 'o':
                ctx->config.output_dir_allocated = strdup(optarg);
                break;
            default:
                print_usage(1);
        }
    }
    
    // Parsuj poziční argumenty: příkaz a název balíku
    if (optind < argc) {
        ctx->config.command = argv[optind++];  // Ukazuje na argv – bez alokace
    }
    
    if (optind < argc) {
        ctx->config.package_name = argv[optind];  // Ukazuje na argv – bez alokace
    }
    
    // Nastav globální úroveň ladění pro logovací funkce
    DEBUG_LEVEL = ctx->config.debug_level;
    
    // Nastav výchozí cestu repozitáře pokud není zadána
    if (!ctx->config.repo_path_allocated) {
        init_repo_config();
        ctx->config.repo_path_allocated = strdup(get_primary_repo_url());
    }
    
    return 0;
}

/**
 * Lazy-inicializuje pool pokud ještě není inicializován.
 *
 * Alokuje jen při prvním požadavku.
 */
Pool* command_context_ensure_pool(CommandContext *ctx) {
    if (!ctx) return NULL;
    
    if (!ctx->resources.pool) {
        ctx->resources.pool = pool_create();
        if (!ctx->resources.pool) {
            log_error("Failed to create pool");
            return NULL;
        }
        log_debug(2, "Pool created (lazy-init)");
    }
    
    return ctx->resources.pool;
}

/**
 * Lazy-inicializuje repozitář pokud ještě není inicializován.
 *
 * Vyžaduje aby pool existoval jako první.
 */
Repo* command_context_ensure_repo(CommandContext *ctx) {
    if (!ctx) return NULL;
    
    // Nejprve zajisti existenci poolu
    Pool *pool = command_context_ensure_pool(ctx);
    if (!pool) return NULL;
    
    if (!ctx->resources.repo) {
        const char *repo_path = command_context_get_repo_path(ctx);
        log_info("Using repository: %s", repo_path);
        
        ctx->resources.repo = ctx->subsystems.load_repository(pool, repo_path);
        if (!ctx->resources.repo) {
            log_error("Failed to load repository from %s", repo_path);
            return NULL;
        }
        log_debug(2, "Repository loaded (lazy-init)");
    }
    
    return ctx->resources.repo;
}

/**
 * Zničí kontext a uvolní všechny alokované prostředky.
 *
 * Uvolňuje v opačném pořadí alokace.
 */
void command_context_destroy(CommandContext *ctx) {
    if (!ctx) return;
    
    // Uvolni runtime prostředky (pokud byly načteny)
    if (ctx->resources.repo) {
        repo_free(ctx->resources.repo, 1);
        ctx->resources.repo = NULL;
    }
    
    if (ctx->resources.pool) {
        pool_free(ctx->resources.pool);
        ctx->resources.pool = NULL;
    }
    
    // Uvolni alokované řetězce CLI konfigurace
    free(ctx->config.staging_dir_allocated);
    free(ctx->config.root_path_allocated);
    free(ctx->config.build_tmp_dir_allocated);
    free(ctx->config.repo_path_allocated);
    free(ctx->config.local_package_allocated);
    free(ctx->config.clean_cache_pkg_allocated);
    free(ctx->config.output_dir_allocated);
    
    // Uvolni samotný kontext
    free(ctx);
}

/**
 * Vrátí staging adresář (z konfigurace nebo výchozí).
 */
const char* command_context_get_staging_dir(CommandContext *ctx) {
    if (!ctx) return STAGING_DIR;
    return ctx->config.staging_dir_allocated ? 
           ctx->config.staging_dir_allocated : STAGING_DIR;
}

/**
 * Vrátí kořenovou cestu (z konfigurace nebo výchozí "/").
 */
const char* command_context_get_root_path(CommandContext *ctx) {
    if (!ctx) return "/";
    return ctx->config.root_path_allocated ? 
           ctx->config.root_path_allocated : "/";
}

/**
 * Vrátí cestu k repozitáři (z konfigurace).
 */
const char* command_context_get_repo_path(CommandContext *ctx) {
    if (!ctx || !ctx->config.repo_path_allocated) return NULL;
    return ctx->config.repo_path_allocated;
}
