//
// Created by placeja5 on 11/4/25.
//

#include "includes/cli_helpers.h"

#include <stdarg.h>       // For va_list in potential extensions (not used yet)

void print_usage(int exit_code) {
    fprintf(stderr, "Usage: pm [OPTIONS] COMMAND [ARGS]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  install <pkg>    Install package with deps resolution\n");
    fprintf(stderr, "  build <pkg>      Build from source (with mkdeps)\n");
    fprintf(stderr, "  resolve <pkg>    Resolve dependencies only (dry-run)\n");
    fprintf(stderr, "  erase <pkg>      Erase installed package\n");
    fprintf(stderr, "  repos            List enabled repositories\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d               Increase debug level\n");
    fprintf(stderr, "  --staging-dir=PATH  Set staging directory (default: %s)\n", STAGING_DIR);
    fprintf(stderr, "  --chroot-path=PATH  Set chroot path (default: %s)\n", CHROOT_PATH);
    fprintf(stderr, "  --no-staging     Disable staging, install directly (stable mode)\n");
    fprintf(stderr, "  --dry-run       Simulate without changes\n");
    fprintf(stderr, "  --local-package=PATH  Install from local tar.bz2 (out of repo)\n");
    fprintf(stderr, "  -h, --help      Show this help\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  pm install glibc\n");
    fprintf(stderr, "  pm build gcc --staging-dir=/tmp/stage\n");
    fprintf(stderr, "  pm install --local-package=/path/to/gzip.tar.bz2\n");
    exit(exit_code);
}

int user_confirm(const char *prompt, int default_yes) {
    char inbuf[128], *ip;
    char default_char = default_yes ? 'y' : 'n';

    for (;;) {
        printf("%s [%c]: ", prompt, default_char);  // Show default
        fflush(stdout);
        *inbuf = 0;
        if (!(ip = fgets(inbuf, sizeof(inbuf), stdin))) {
            log_error("Input error: EOF or interrupt. Aborting.");
            exit(1);  // Fatal – use log_error from common (implement later)
        }
        while (*ip == ' ' || *ip == '\t') ip++;  // Skip whitespace
        if (*ip == '\n' || *ip == 0) {
            return default_yes;  // Enter = default
        }
        if (*ip == 'q' || *ip == 'Q') {
            log_error("User aborted.");
            exit(1);
        }
        if (*ip == 'y' || *ip == 'Y') return 1;
        if (*ip == 'n' || *ip == 'N') return 0;
        // Invalid – loop again
        printf("Please enter 'y', 'n', or '%c' (default).\n", default_char);
    }
}