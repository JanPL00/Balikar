//
// Created by placeja5 on 11/5/25.
//

#include "includes/print.h"
#include "../includes/string_helpers.h"

/**
 * Calculates and displays disk usage changes for a transaction.
 * Shows how much disk space and inodes will be used/freed in key directories.
 * Useful for checking if sufficient disk space is available before installation.
 * @param trans Transaction to analyze
 */
void print_disk_usage_changes(Transaction *trans) {
    if (!trans || !trans->pool) {
        log_error("Invalid transaction or pool in print_disk_usage_changes.");
        return;
    }

    DUChanges duc[4];  // Monitor 4 key directories
    int i;

    // Configure paths to monitor
    // These represent key directories in LFS/staging setup
    memset(duc, 0, sizeof(duc));
    duc[0].path = "/";                   // Root filesystem (chroot path)
    duc[1].path = STAGING_DIR "/usr";    // Staging /usr directory
    duc[2].path = "/sbin";               // System binaries
    duc[3].path = "/etc";                // Configuration files

    // Calculate disk usage changes using libsolv
    transaction_calc_duchanges(trans, duc, 4);

    // Display changes for paths that have modifications
    for (i = 0; i < 4; i++) {
        if (duc[i].kbytes || duc[i].files) {
            printf("Disk usage changes for %s: %lld KB, %lld inodes\n",
                   duc[i].path, duc[i].kbytes, duc[i].files);
        }
    }
}

/**
 * Displays a summary of transaction steps to the user.
 * Shows which packages will be installed, erased, or modified.
 * Transaction is ordered by dependencies before display.
 * @param trans Transaction to display
 */
void display_transaction(Transaction *trans) {
    if (!trans || !trans->pool) {
        log_error("Invalid transaction or pool in display_transaction.");
        return;
    }

    Pool *pool = trans->pool;
    int i;

    printf("Transaction summary:\n\n");

    // Order transaction steps by dependencies
    // Ensures packages are processed in correct order
    transaction_order(trans, 0);

    // Display each transaction step
    for (i = 0; i < trans->steps.count; i++) {
        Id p = trans->steps.elements[i];
        // Get transaction type (install, erase, etc.)
        Id type = transaction_type(trans, p, SOLVER_TRANSACTION_RPM_ONLY);

        // Format package name for display
        char *solv_str = solvable_to_string(pool, p);
        
        // Display action based on transaction type
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

    // Display summary statistics
    printf("\nTotal steps: %d\n", trans->steps.count);
}