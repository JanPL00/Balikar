#ifndef BALIKAR_COMMAND_CONTEXT_H
#define BALIKAR_COMMAND_CONTEXT_H

#include "../../common/common.h"
#include "command.h"
#include <jansson.h>

typedef struct {

    int debug_level;
    int dry_run;
    int no_deps;

    char *staging_dir_allocated;
    char *root_path_allocated;
    char *build_tmp_dir_allocated;
    char *repo_path_allocated;
    char *output_dir_allocated;

    char *local_package_allocated;
    char *clean_cache_pkg_allocated;

    const char *command;
    const char *package_name;

} CLIConfig;

typedef struct {
    Pool *pool;
    Repo *repo;
} RuntimeResources;

typedef struct {

    Repo* (*load_repository)(Pool*, const char*);
    int (*generate_repodata)(const char*);

    Solver* (*create_solver)(Pool*);
    Queue* (*solve_dependencies)(Solver*, const char*, Pool*, Repo*, const char*,
                                  json_t* (*)(Repodata*, Id, Pool*));

    FILE* (*download_package)(Solvable*, const char*, Pool*, json_t*, const char*);
    void (*download_extras)(json_t*, const char*, const char*, const char*, const char*);

    int (*commit_element)(Transaction*, Id, FILE*, Pool*, int, int, json_t*);

    int (*install_package_with_reason)(const char*, const char*, const char*, const char*, const char*);
    int (*erase_package)(const char*, const char*);
    int (*is_package_installed)(const char*, const char*);

} SubsystemFacade;

typedef struct command_context {
    CLIConfig config;
    RuntimeResources resources;
    SubsystemFacade subsystems;

    int exit_code;

} CommandContext;

CommandContext* command_context_create(void);

void command_context_init_subsystems(CommandContext *ctx);

int command_context_parse_args(CommandContext *ctx, int argc, char **argv);

Pool* command_context_ensure_pool(CommandContext *ctx);

Repo* command_context_ensure_repo(CommandContext *ctx);

void command_context_destroy(CommandContext *ctx);

const char* command_context_get_staging_dir(CommandContext *ctx);

const char* command_context_get_root_path(CommandContext *ctx);

const char* command_context_get_repo_path(CommandContext *ctx);

#endif
