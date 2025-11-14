//
// Created by placeja5 on 11/5/25.
//

#include "includes/debug.h"
#include "includes/string_helpers.h"  // Pro solvable_to_string (pro lepší výpis)


void display_proof(Solver *solv, Id id, int flags, Queue *lq) {
    if (!solv || !solv->pool) {
        log_error("Invalid solver or pool in display_proof.");
        return;
    }

    Pool *pool = solv->pool;
    Queue q, qp;
    int i, j;

    queue_init(&q);
    queue_init(&qp);
    solver_get_decisionlist(solv, id, flags | SOLVER_DECISIONLIST_SORTED | SOLVER_DECISIONLIST_WITHINFO | SOLVER_DECISIONLIST_MERGEDINFO, &q);

    for (i = 0; i < q.count; i += 8) {
        Id v = q.elements[i];
        int reason = q.elements[i + 1], bits = q.elements[i + 3], type = q.elements[i + 4];
        Id from = q.elements[i + 5], to = q.elements[i + 6], dep = q.elements[i + 7];

        if (reason != SOLVER_REASON_UNSOLVABLE && type == SOLVER_RULE_PKG_SAME_NAME) {
            continue;  // Skip "obvious" decisions, as in solv.c
        }

        solver_decisionlist_solvables(solv, &q, i, &qp);
        if (qp.count) {
            i += qp.count * 8 - 8;
        }

        // Use solvable_to_string for better output
        char *solv_str = solvable_to_string(pool, qp.elements[0]);  // Assume first solvable for simplicity
        if (reason == SOLVER_REASON_UNSOLVABLE) {
            printf("unsolvable: ");
        } else {
            printf("%s %s: ", v < 0 ? "conflicted" : "installed", solv_str ? solv_str : pool_solvidset2str(pool, &qp));
        }
        free(solv_str);

        if (type == 0) {
            printf("%s\n", solver_reason2str(solv, reason));
            continue;
        }

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

        printf("%s\n", solver_decisioninfo2str(solv, bits, type, from, to, dep));
    }

    queue_free(&qp);
    queue_free(&q);
    printf("\n");
}

