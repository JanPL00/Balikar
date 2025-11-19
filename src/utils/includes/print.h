//
// Created by placeja5 on 11/5/25.
//

#ifndef IMPL_PRINT_H
#define IMPL_PRINT_H

#include "../../common/common.h"  // Pro Pool, Transaction, DUChanges

// Public print API

// Calculate and display disk usage changes for a transaction.
// Shows how much disk space and inodes will be used/freed in key directories.
// Monitors: root (/), staging /usr, /sbin, /etc.
// Useful for checking if sufficient disk space is available before installation.
// Adapted from showdiskusagechanges in solv.c (SUSE-specific).
// trans: Transaction to analyze
void print_disk_usage_changes(Transaction *trans);

// Display a summary of transaction steps to the user.
// Shows which packages will be installed, erased, or modified.
// Transaction is ordered by dependencies before display.
// Adapted from transaction print in solv.c main.
// trans: Transaction to display
void display_transaction(Transaction *trans);

#endif //IMPL_PRINT_H