//
// Created by placeja5 on 11/12/25.
//

#ifndef IMPL_REPO_SOLVER_H
#define IMPL_REPO_SOLVER_H

#include "../../common/common.h"

// Create a solver instance for the pool.
Solver *create_solver(Pool *pool);

// Add makedeps to the job queue recursively.
void add_mkdeps_to_queue(Queue *job, Id p, Pool *pool, Repo *repo, int depth, json_t* (*mkdeps_cb)(Repodata*, Id, Pool*));

// Solve dependencies for a package and return the job queue.
// root_path: target installation root (default "/") to check for already installed packages
Queue *solve_dependencies(Solver *solv, const char *pkg_name, Pool *pool, Repo *repo, const char *root_path, json_t* (*mkdeps_cb)(Repodata*, Id, Pool*));

// Display solver problem proof information (includes learnt rules and decision trace).
void display_problem_proof(Solver *solv, int problem);

// Find reverse dependencies - packages that depend on pkg_name
json_t* find_reverse_dependencies(const char *pkg_name, const char *chroot_path, Pool *pool, Repo *repo, json_t* (*load_db_cb)(const char*));

// Find orphaned packages - dependencies no longer needed
json_t* find_orphaned_packages(const char *chroot_path, Pool *pool, Repo *repo, json_t* (*load_db_cb)(const char*));

// Remove all orphaned packages
int autoremove_packages(const char *chroot_path, Pool *pool, Repo *repo, int dry_run, json_t* (*load_db_cb)(const char*), int (*erase_cb)(const char*, const char*));

// Erase package with dependency checks and orphan detection
int erase_package_with_checks(const char *pkg_name, const char *chroot_path, Pool *pool, Repo *repo, int show_orphans, json_t* (*load_db_cb)(const char*), int (*erase_cb)(const char*, const char*));

#endif
