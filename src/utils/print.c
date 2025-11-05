//
// Created by placeja5 on 11/5/25.
//

#include "includes/print.h"
#include "../includes/string_helpers.h"

void print_disk_usage_changes(Transaction *trans) {
    if (!trans || !trans->pool) {
        log_error("Invalid transaction or pool in print_disk_usage_changes.");
        return;
    }

    DUChanges duc[4];  // Adapt for our paths; can extend for more
    int i;

    // Customize paths for our LFS/LLD setup (e.g., staging $P, chroot root)
    memset(duc, 0, sizeof(duc));
    duc[0].path = "/";                   // Root (chroot path)
    duc[1].path = STAGING_DIR "/usr";    // Staging usr (adapt from constants in common.h)
    duc[2].path = "/sbin";               // Example from solv.c
    duc[3].path = "/etc";                // Example from solv.c

    transaction_calc_duchanges(trans, duc, 4);

    for (i = 0; i < 4; i++) {
        if (duc[i].kbytes || duc[i].files) {
            printf("Disk usage changes for %s: %lld KB, %lld inodes\n",
                   duc[i].path, duc[i].kbytes, duc[i].files);
        }
    }
}

void display_transaction(Transaction *trans) {
    if (!trans || !trans->pool) {
        log_error("Invalid transaction or pool in display_transaction.");
        return;
    }

    Pool *pool = trans->pool;
    int i;

    printf("Transaction summary:\n\n");

    // Order transaction steps first (as in solv.c)
    transaction_order(trans, 0);

    for (i = 0; i < trans->steps.count; i++) {
        Id p = trans->steps.elements[i];
        Id type = transaction_type(trans, p, SOLVER_TRANSACTION_RPM_ONLY);  // Adapt if needed for non-RPM

        char *solv_str = solvable_to_string(pool, p);
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

    // Add custom for our project: Estimate total size/download
    printf("\nTotal steps: %d\n", trans->steps.count);
}