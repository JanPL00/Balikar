//
// Created by placeja5 on 11/4/25.
//

#include "includes/cli_helpers.h"

#include <stdarg.h>       // For va_list in potential extensions (not used yet)

/**
 * Displays program usage information and exits.
 * Shows all available commands, options, and usage examples.
 * Called when user requests help or provides invalid arguments.
 * @param exit_code Exit code to use when terminating (0 for success, non-zero for error)
 */
void print_usage(int exit_code) {
    fprintf(stderr, "Usage: balikar [OPTIONS] COMMAND [ARGS]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  install <pkg>         Install package with deps resolution\n");
    fprintf(stderr, "  build <pkg>           Build from source (with mkdeps)\n");
    fprintf(stderr, "  resolve <pkg>         Resolve dependencies only (dry-run)\n");
    fprintf(stderr, "  erase <pkg>           Erase installed package\n");
    fprintf(stderr, "  show <pkg>            Show package information\n");
    fprintf(stderr, "  repo list             List configured repositories\n");
    fprintf(stderr, "  repo add <name> <url> Add new repository\n");
    fprintf(stderr, "  repo remove <name>    Remove repository\n");
    fprintf(stderr, "  repo enable <name>    Enable repository\n");
    fprintf(stderr, "  repo disable <name>   Disable repository\n");
    fprintf(stderr, "  repo-update           Regenerate repodata.json from repository\n");
    fprintf(stderr, "  clean-cache           Clear download cache\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d                    Increase debug level\n");
    fprintf(stderr, "  --staging-dir=PATH    Set staging directory (default: %s)\n", STAGING_DIR);
    fprintf(stderr, "  --chroot-path=PATH    Set chroot path (default: %s)\n", CHROOT_PATH);
    fprintf(stderr, "  --no-staging          Disable staging, install directly (stable mode)\n");
    fprintf(stderr, "  --dry-run             Simulate without changes\n");
    fprintf(stderr, "  --repo=PATH           Override repository path\n");
    fprintf(stderr, "  --local-package=PATH  Install from local tar.bz2 (out of repo)\n");
    fprintf(stderr, "  --pkg=NAME            Package name for clean-cache command\n");
    fprintf(stderr, "  -h, --help            Show this help\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  balikar install glibc\n");
    fprintf(stderr, "  balikar build gcc --staging-dir=/tmp/stage\n");
    fprintf(stderr, "  balikar repo list\n");
    fprintf(stderr, "  balikar repo add myrepo https://example.com/repo\n");
    fprintf(stderr, "  balikar clean-cache --pkg=glibc\n");
    fprintf(stderr, "  balikar install --local-package=/path/to/gzip.tar.bz2\n");
    exit(exit_code);
}

/**
 * Prompts user for yes/no confirmation.
 * Accepts 'y', 'n', or Enter for default. 'q' aborts the program.
 * Loops until valid input is received.
 * @param prompt Question to display to user
 * @param default_yes Default answer if user presses Enter (1=yes, 0=no)
 * @return 1 for yes, 0 for no (never returns on abort/error)
 */
int user_confirm(const char *prompt, int default_yes) {
    char inbuf[128], *ip;
    char default_char = default_yes ? 'y' : 'n';

    for (;;) {
        // Display prompt with default indication
        printf("%s [%c]: ", prompt, default_char);
        fflush(stdout);
        
        *inbuf = 0;
        
        // Read user input
        if (!(ip = fgets(inbuf, sizeof(inbuf), stdin))) {
            log_error("Input error: EOF or interrupt. Aborting.");
            exit(1);  // Fatal error - can't continue without user input
        }
        
        // Skip leading whitespace
        while (*ip == ' ' || *ip == '\t') ip++;
        
        // Empty input (just Enter) - use default
        if (*ip == '\n' || *ip == 0) {
            return default_yes;
        }
        
        // 'q' or 'Q' - user wants to abort
        if (*ip == 'q' || *ip == 'Q') {
            printf("\n");
            log_error("User aborted.");
            exit(1);
        }
        
        // 'y' or 'Y' - affirmative answer
        if (*ip == 'y' || *ip == 'Y') {
            printf("\n");
            return 1;
        }
        
        // 'n' or 'N' - negative answer
        if (*ip == 'n' || *ip == 'N') {
            printf("\n");
            return 0;
        }
        
        // Invalid input - prompt again
        printf("Please enter 'y', 'n', or '%c' (default).\n", default_char);
    }
}