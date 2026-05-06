#ifndef IMPL_REPO_SOLVER_H
#define IMPL_REPO_SOLVER_H

#include "../../common/common.h"

Solver *create_solver(Pool *pool);

void add_mkdeps_to_queue(Queue *job, Id p, Pool *pool, Repo *repo, int depth, json_t* (*mkdeps_cb)(Repodata*, Id, Pool*));

Queue *solve_dependencies(Solver *solv, const char *pkg_name, Pool *pool, Repo *repo, const char *root_path, json_t* (*mkdeps_cb)(Repodata*, Id, Pool*));

void display_problem_proof(Solver *solv, int problem);

#endif
