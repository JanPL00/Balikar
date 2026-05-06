#include "includes/print.h"

/**
 * Vypočítá a zobrazí změny využití disku pro transakci.
 *
 * Ukáže kolik místa na disku a inodů bude použito/uvolněno v klíčových adresářích.
 * Slouží ke kontrole dostatku místa před instalací.
 *
 * @param trans Transakce k analýze
 */
void print_disk_usage_changes(Transaction *trans) {
    if (!trans || !trans->pool) {
        log_error("Invalid transaction or pool in print_disk_usage_changes.");
        return;
    }

    DUChanges duc[4];  // Sleduj 4 klíčové adresáře
    int i;

    // Konfiguruj cesty ke sledování
    // Představují klíčové adresáře v LFS/staging nastavení
    memset(duc, 0, sizeof(duc));
    duc[0].path = "/";                   // Kořenový souborový systém (cesta chroot)
    duc[1].path = STAGING_DIR "/usr";    // Staging adresář /usr
    duc[2].path = "/sbin";               // Systémové binárky
    duc[3].path = "/etc";                // Konfigurační soubory

    // Vypočítej změny využití disku pomocí libsolv
    transaction_calc_duchanges(trans, duc, 4);

    // Zobraz změny pro cesty, které mají úpravy
    for (i = 0; i < 4; i++) {
        if (duc[i].kbytes || duc[i].files) {
            printf("Disk usage changes for %s: %lld KB, %lld inodes\n",
                   duc[i].path, duc[i].kbytes, duc[i].files);
        }
    }
}

/**
 * Zobrazí souhrn kroků transakce uživateli.
 *
 * Ukáže které balíky budou nainstalovány, odstraněny nebo upraveny.
 * Transakce je seřazena podle závislostí před zobrazením.
 *
 * @param trans Transakce k zobrazení
 */
void display_transaction(Transaction *trans) {
    if (!trans || !trans->pool) {
        log_error("Invalid transaction or pool in display_transaction.");
        return;
    }

    Pool *pool = trans->pool;
    int i;

    printf("Transaction summary:\n\n");

    // Seřaď kroky transakce podle závislostí
    // Zajistí zpracování balíků ve správném pořadí
    transaction_order(trans, 0);

    // Zobraz každý krok transakce
    for (i = 0; i < trans->steps.count; i++) {
        Id p = trans->steps.elements[i];
        // Získej typ transakce (instalace, mazání atd.)
        Id type = transaction_type(trans, p, SOLVER_TRANSACTION_RPM_ONLY);

        // Formátuj název balíku pro zobrazení
        char *solv_str = solvable_to_string(pool, p);
        
        // Zobraz akci podle typu transakce
        switch (type) {
            case SOLVER_TRANSACTION_ERASE:
                printf("Erase: %s\n", solv_str ? solv_str : pool_solvid2str(pool, p));
                break;
            case SOLVER_TRANSACTION_INSTALL:
            case SOLVER_TRANSACTION_MULTIINSTALL:
                printf("Install: %s\n", solv_str ? solv_str : pool_solvid2str(pool, p));
                break;
            default:
                printf("Unknown operation on: %s\n", solv_str ? solv_str : pool_solvid2str(pool, p));
                break;
        }
        free(solv_str);
    }

    // Zobraz souhrnné statistiky
    printf("\nTotal steps: %d\n", trans->steps.count);
}