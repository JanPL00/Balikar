//
// Created by placeja5 on 11/12/25.
//

#ifndef IMPL_REPO_SOLVER_H
#define IMPL_REPO_SOLVER_H

#include "../../common/common.h"
#include "parser.h"

// Create a solver instance for the pool.
Solver *create_solver(Pool *pool);

// Add makedeps to the job queue recursively.
void add_mkdeps_to_queue(Queue *job, Id p, Pool *pool, Repo *repo, int depth);

// Solve dependencies for a package and return the job queue.
// root_path: target installation root (default "/") to check for already installed packages
Queue *solve_dependencies(Solver *solv, const char *pkg_name, Pool *pool, Repo *repo, const char *root_path);

// Display solver problem proof information.
void display_problem_proof(Solver *solv, int problem);

#endif