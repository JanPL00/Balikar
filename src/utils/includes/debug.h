//
// Created by placeja5 on 11/5/25.
//

#ifndef IMPL_DEBUG_H
#define IMPL_DEBUG_H

#include "../../common/common.h"  // Pro Pool, Solver, Queue, Id

// Public debug API

// Display a proof/trace of solver decisions for debugging.
// Shows why packages were installed or conflicts occurred.
// Helps understand dependency resolution process and diagnose issues.
// Based on libsolv's decision list mechanism (adapted from solv.c doshowproof).
// solv: Solver instance containing decisions
// id: Solvable ID to display proof for
// flags: Additional flags for decision list retrieval
// lq: Queue of learnt rules (optional, can be NULL)
void display_proof(Solver *solv, Id id, int flags, Queue *lq);

// Display the full problem proof, including learnt rules.
// Shows complete chain of decisions leading to conflict or unsolvable situation.
// Adapted from showproof in solv.c.
// solv: Solver instance
// problem: Problem ID to display
void display_problem_proof(Solver *solv, int problem);

#endif //IMPL_DEBUG_H