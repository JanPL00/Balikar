#include "common.h"

// Implementace globálních utilit
// Poskytuje logovací funkce a pomocníky pro architekturu používané v celém projektu

// Globální proměnná úrovně ladění (nastavena z main.c pomocí příznaku -d)
int DEBUG_LEVEL = 0;

# ifndef UNIT_TESTS

// ANSI kódy barev (používány pouze při výstupu na terminál)
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_CYAN    "\033[1;36m"

/**
 * Zapíše chybovou zprávu do stderr a ukončí program.
 *
 * Používá se pro fatální chyby, ze kterých nelze zotavit.
 *
 * @param fmt Formátovací řetězec ve stylu printf
 * @param ... Variabilní argumenty formátovacího řetězce
 */
void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (isatty(fileno(stderr))) {
        fprintf(stderr, COLOR_RED "[ERROR]" COLOR_RESET " ");
    } else {
        fprintf(stderr, "[ERROR] ");
    }
    vfprintf(stderr, fmt, args);  // Vypiš formátovanou zprávu
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);  // Fatální chyba – ukonči program
}

/**
 * Zapíše ladicí zprávu do stderr pokud úroveň splňuje práh.
 *
 * Tiskne pouze pokud level >= DEBUG_LEVEL (ovládáno příznakem -d).
 *
 * @param level Úroveň ladicí podrobnosti (1=základní, 2=podrobný atd.)
 * @param fmt Formátovací řetězec ve stylu printf
 * @param ... Variabilní argumenty formátovacího řetězce
 */
void log_debug(int level, const char *fmt, ...) {
    // Přeskoč pokud je požadovaná úroveň ladění vyšší než nastavený DEBUG_LEVEL
    // např. pokud DEBUG_LEVEL=0 (bez ladění), přeskoč všechny ladicí zprávy (level 1, 2, ...)
    if (DEBUG_LEVEL < level) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    if (isatty(fileno(stderr))) {
        fprintf(stderr, COLOR_CYAN "[DEBUG %d]" COLOR_RESET " ", level);
    } else {
        fprintf(stderr, "[DEBUG %d] ", level);
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);  // Okamžitý výstup pro ladění v reálném čase
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (isatty(fileno(stdout))) {
        fprintf(stdout, COLOR_GREEN "[INFO]" COLOR_RESET " ");
    } else {
        fprintf(stdout, "[INFO] ");
    }
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
    fflush(stdout);
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (isatty(fileno(stderr))) {
        fprintf(stderr, COLOR_YELLOW "[WARN]" COLOR_RESET " ");
    } else {
        fprintf(stderr, "[WARN] ");
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
}

#endif

