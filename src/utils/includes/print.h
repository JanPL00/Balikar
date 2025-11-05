//
// Created by placeja5 on 11/5/25.
//

#ifndef IMPL_PRINT_H
#define IMPL_PRINT_H

#include "../../common/common.h"  // Pro Pool, Transaction, DUChanges

// ========================================
// PRINT PROTOTYPES
// ========================================

/**
 * Print changes in disk usage for the transaction.
 * Adapted from showdiskusagechanges in solv.c (SUSE-specific).
 * Calculates DU changes for specified paths (e.g., "/", "/usr") and prints them.
 * In our project, adapt for LFS chroot and $P staging estimation.
 * @param trans The Transaction instance.
 */
void print_disk_usage_changes(Transaction *trans);

/**
 * Display a summary of the transaction (installs, erases, etc.).
 * Adapted from transaction print in solv.c main (~901-950).
 * Prints each step with type (install/erase) and solvable details.
 * @param trans The Transaction instance.
 */
void display_transaction(Transaction *trans);

#endif //IMPL_PRINT_H