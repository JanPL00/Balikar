//
// Created by placeja5 on 11/5/25.
//

#ifndef IMPL_DEBUG_H
#define IMPL_DEBUG_H

#include "../../common/common.h"  // Pro Pool, Solver, Queue, Id


// ========================================
// DEBUG PROTOTYPES
// ========================================

/**
 * Display a proof for a specific decision or learnt rule.
 * Adapted from doshowproof in solv.c.
 * @param solv The Solver instance.
 * @param id The ID of the decision or problem.
 * @param flags Flags for decision list (e.g., SOLVER_DECISIONLIST_SORTED).
 * @param lq Optional queue of learnt rules (for context).
 */
void display_proof(Solver *solv, Id id, int flags, Queue *lq);

/**
 * Display the full problem proof, including learnt rules.
 * Adapted from showproof in solv.c.
 * @param solv The Solver instance.
 * @param problem The problem ID to display.
 */
void display_problem_proof(Solver *solv, int problem);

#endif //IMPL_DEBUG_H