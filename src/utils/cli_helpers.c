#include "includes/cli_helpers.h"

#include <stdarg.h>       // Pro va_list v případných rozšířeních (zatím nepoužito)

/**
 * Zobrazí informace o použití programu a ukončí ho.
 *
 * Ukáže všechny dostupné příkazy, volby a příklady použití.
 * Volá se když uživatel požaduje nápovědu nebo zadá neplatné argumenty.
 *
 * @param exit_code Exit kód při ukončení (0 pro úspěch, nenulový pro chybu)
 */
void print_usage(int exit_code) {
    fprintf(stderr, "Usage: balikar [OPTIONS] COMMAND [ARGS]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  install <pkg>         Install package with deps resolution\n");
    fprintf(stderr, "  build <pkg>           Build from source (with mkdeps)\n");
    fprintf(stderr, "  resolve <pkg>         Resolve dependencies only (dry-run)\n");
    fprintf(stderr, "  download <pkg>        Download packages without installing\n");
    fprintf(stderr, "  remove <pkg>          Remove installed package\n");
    fprintf(stderr, "  autoremove            Remove orphaned dependencies\n");
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
    fprintf(stderr, "  --root=PATH           Set target root for install/remove (default: /)\n");
    fprintf(stderr, "  --no-staging          Disable staging, install directly (stable mode)\n");
    fprintf(stderr, "  --dry-run             Simulate without changes\n");
    fprintf(stderr, "  --no-deps             Download without dependencies\n");
    fprintf(stderr, "  --repo=PATH           Override repository path\n");
    fprintf(stderr, "  --output-dir=PATH     Output directory for download command\n");
    fprintf(stderr, "  --local-package=PATH  Install from local tar.bz2 (out of repo)\n");
    fprintf(stderr, "  --pkg=NAME            Package name for clean-cache command\n");
    fprintf(stderr, "  -h, --help            Show this help\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  balikar install glibc\n");
    fprintf(stderr, "  balikar build gcc --staging-dir=/tmp/stage\n");
    fprintf(stderr, "  balikar download nmap --output-dir=/tmp/downloads\n");
    fprintf(stderr, "  balikar download gzip --no-deps\n");
    fprintf(stderr, "  balikar repo list\n");
    fprintf(stderr, "  balikar repo add myrepo https://example.com/repo\n");
    fprintf(stderr, "  balikar clean-cache --pkg=glibc\n");
    fprintf(stderr, "  balikar install --local-package=/path/to/gzip.tar.bz2\n");
    exit(exit_code);
}

/**
 * Vyzve uživatele k potvrzení ano/ne.
 *
 * Přijímá 'y', 'n' nebo Enter pro výchozí hodnotu. 'q' přeruší program.
 * Opakuje se dokud není přijat platný vstup.
 *
 * @param prompt Otázka k zobrazení uživateli
 * @param default_yes Výchozí odpověď při stisknutí Enter (1=ano, 0=ne)
 * @return 1 pro ano, 0 pro ne (při přerušení/chybě nevrátí)
 */
int user_confirm(const char *prompt, int default_yes) {
    char inbuf[128], *ip;
    char default_char = default_yes ? 'y' : 'n';

    for (;;) {
        // Zobraz výzvu s indikací výchozí volby
        printf("%s [%c]: ", prompt, default_char);
        fflush(stdout);
        
        *inbuf = 0;
        
        // Načti vstup od uživatele
        if (!(ip = fgets(inbuf, sizeof(inbuf), stdin))) {
            log_error("Input error: EOF or interrupt. Aborting.");
            exit(1);  // Fatální chyba – nelze pokračovat bez vstupu uživatele
        }
        
        // Přeskoč úvodní bílé znaky
        while (*ip == ' ' || *ip == '\t') ip++;
        
        // Prázdný vstup (jen Enter) – použij výchozí
        if (*ip == '\n' || *ip == 0) {
            return default_yes;
        }
        
        // 'q' nebo 'Q' – uživatel chce přerušit
        if (*ip == 'q' || *ip == 'Q') {
            printf("\n");
            log_error("User aborted.");
            exit(1);
        }
        
        // 'y' nebo 'Y' – kladná odpověď
        if (*ip == 'y' || *ip == 'Y') {
            printf("\n");
            return 1;
        }
        
        // 'n' nebo 'N' – záporná odpověď
        if (*ip == 'n' || *ip == 'N') {
            printf("\n");
            return 0;
        }
        
        // Neplatný vstup – zeptej se znovu
        printf("Please enter 'y', 'n', or '%c' (default).\n", default_char);
    }
}