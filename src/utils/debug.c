//
// Created by placeja5 on 11/5/25.
//

#include "includes/debug.h"
#include "includes/string_helpers.h"  // Pro solvable_to_string (pro lepší výpis)

/**
 * Displays a proof/trace of solver decisions for debugging.
 * Shows why packages were installed or conflicts occurred.
 * Helps understand dependency resolution process and diagnose issues.
 * Based on libsolv's decision list mechanism.
 * @param solv Solver instance containing decisions
 * @param id Solvable ID to display proof for
 * @param flags Additional flags for decision list retrieval
 * @param lq Queue of learnt rules (optional, can be NULL)
 */
void display_proof(Solver *solv, Id id, int flags, Queue *lq) {
    if (!solv || !solv->pool) {
        log_error("Invalid solver or pool in display_proof.");
        return;
    }

    Pool *pool = solv->pool;
    Queue q, qp;
    int i, j;

    // Initialize queues for decision list processing
    queue_init(&q);
    queue_init(&qp);
    
    // Get sorted decision list with detailed information
    solver_get_decisionlist(solv, id, flags | SOLVER_DECISIONLIST_SORTED | SOLVER_DECISIONLIST_WITHINFO | SOLVER_DECISIONLIST_MERGEDINFO, &q);

    // Process decision list - each entry is 8 elements
    for (i = 0; i < q.count; i += 8) {
        // Extract decision information from queue
        Id v = q.elements[i];           // Solvable ID
        int reason = q.elements[i + 1]; // Why decision was made
        int bits = q.elements[i + 3];   // Additional information bits
        int type = q.elements[i + 4];   // Rule type
        Id from = q.elements[i + 5];    // Source solvable
        Id to = q.elements[i + 6];      // Target solvable
        Id dep = q.elements[i + 7];     // Dependency

        // Skip obvious/trivial decisions (same name upgrades)
        if (reason != SOLVER_REASON_UNSOLVABLE && type == SOLVER_RULE_PKG_SAME_NAME) {
            continue;
        }

        // Get all solvables involved in this decision
        solver_decisionlist_solvables(solv, &q, i, &qp);
        if (qp.count) {
            i += qp.count * 8 - 8;  // Skip merged entries
        }

        // Format and display the decision
        char *solv_str = solvable_to_string(pool, qp.elements[0]);
        
        // Print decision type and affected package
        if (reason == SOLVER_REASON_UNSOLVABLE) {
            printf("unsolvable: ");
        } else {
            printf("%s %s: ", v < 0 ? "conflicted" : "installed", solv_str ? solv_str : pool_solvidset2str(pool, &qp));
        }
        free(solv_str);

        // Simple reason without detailed rule info
        if (type == 0) {
            printf("%s\n", solver_reason2str(solv, reason));
            continue;
        }

        // Check if this is a learnt rule (from conflict analysis)
        if (type == SOLVER_RULE_LEARNT && lq) {
            for (j = 0; j < lq->count; j++) {
                if (lq->elements[j] == q.elements[i + 2]) {
                    break;
                }
            }
            if (j < lq->count) {
                printf("learnt rule #%d\n", j + 1);
                continue;
            }
        }

        // Display detailed decision information
        printf("%s\n", solver_decisioninfo2str(solv, bits, type, from, to, dep));
    }

    queue_free(&qp);
    queue_free(&q);
    printf("\n");
}

