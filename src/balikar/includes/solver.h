//
// Created by placeja5 on 11/12/25.
//

#ifndef IMPL_REPO_SOLVER_H
#define IMPL_REPO_SOLVER_H

#include "../../common/common.h"
#include "../../utils/includes/debug.h"   // Pro display_problem_proof atd.
#include "../../utils/includes/system_helpers.h"  // Pro set_system_arch
#include "../../utils/includes/cli_helpers.h"
#include "../../utils/includes/print.h"  // Pro display_transaction
#include "../../utils/includes/string_helpers.h"  // Pro solvable_to_string
#include "parser.h"  // Pro get_custom_*

// Konstanty pro rekurzi a limity (proti cirkulárním deps)

// Prototypy funkcí
Solver *create_solver(Pool *pool);
void add_mkdeps_to_queue(Queue *job, Id p, Pool *pool, Repo *repo, int depth);
Queue *solve_dependencies(Solver *solv, const char *pkg_name, Pool *pool, Repo *repo);
void display_problem_proof(Solver *solv, int problem);

#endif //IMPL_REPO_SOLVER_H