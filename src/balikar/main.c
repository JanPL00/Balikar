//
//
#include "includes/command_context.h"
#include "includes/command_registry.h"
#include "../common/common.h"

/**
 * Vstupní bod aplikace Balikar.
 *
 * Refaktorováno pomocí Command Pattern + Dispatcher pro modulární zpracování příkazů.
 *
 * Workflow:
 * 1. Vytvoření kontextu příkazu (obsahuje CLI konfiguraci + lazy resources)
 * 2. Parsování argumentů příkazové řádky do kontextu
 * 3. Dispatch na příslušný handler příkazu
 * 4. Úklid a vrácení exit kódu
 *
 * @param argc Počet argumentů
 * @param argv Vektor argumentů
 * @return Exit kód: 0 při úspěchu, chybový kód při selhání
 */
int main(int argc, char **argv) {
    
    // Krok 1: Vytvoř kontext příkazu
    CommandContext *ctx = command_context_create();
    if (!ctx) {
        log_error("Failed to create command context.");
        return 1;
    }
    
    // Krok 1.5: Inicializuj fasády subsystémů
    command_context_init_subsystems(ctx);
    
    // Krok 2: Parsuj argumenty příkazové řádky
    // Naplní konfiguraci CLI a nastaví DEBUG_LEVEL
    if (command_context_parse_args(ctx, argc, argv) != 0) {
        // Chyba při parsování nebo byl vyžádán --help
        command_context_destroy(ctx);
        return 1;
    }
    
    // Krok 3: Předej ke zpracovači příkazu
    // Vyhledej příkaz, validuj a spusť
    int exit_code = command_dispatch(ctx);
    
    // Krok 4: Úklid
    command_context_destroy(ctx);
    
    return exit_code;
}
