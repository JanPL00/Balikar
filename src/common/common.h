#ifndef IMPL_COMMON_H
#define IMPL_COMMON_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <time.h>
#include  <check.h>
#include <sys/stat.h>
#include <getopt.h>
#include <errno.h>

#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/solver.h>
#include <solv/solverdebug.h>
#include <solv/queue.h>
#include <solv/transaction.h>
#include <solv/selection.h>
#include <solv/poolarch.h>
#include <solv/evr.h>
#include <solv/knownid.h>
#include <ctype.h>
#include <sys/wait.h>

#include <jansson.h>

#include <solv/repo_conda.h>

#include <curl/curl.h>

#include "../utils/includes/system_helpers.h"
#include "../utils/includes/string_helpers.h"
#include "../utils/includes/cli_helpers.h"
#include "../utils/includes/print.h"

#define STAGING_DIR "/tmp/balikar_staging"
#define CHROOT_PATH "/tmp/pm-chroot"
#define MAX_RECURSION_DEPTH 10
#define REPO_METADATA "repodata.json"
#define PKG_METADATA "index.json"
#define MAX_PATH_LEN 1024

extern int DEBUG_LEVEL;

#define ERR_NO_REPO 1
#define ERR_SOLVE_FAIL 2
#define ERR_DOWNLOAD_FAIL 3
#define ERR_BUILD_FAIL 4

typedef struct {
    char *source;
    json_t *mkdeps;
    json_t *extras;
    int is_group;
    int no_package;
} CustomMeta;

void log_error(const char *fmt, ...);

void log_debug(int level, const char *fmt, ...);

void log_info(const char *fmt, ...);

void log_warn(const char *fmt, ...);

#define CACHE_DIR (get_cache_dir())

static inline char *get_cache_dir(void) {
        return strdup("/var/cache/balikar/downloads");
}
#endif
