
//
// Created by placeja5 on 11/12/25.
//

#include "includes/solver.h"

// Private helper for recursion (internal, not in .h)
static void internal_display_proof(Solver *solv, Id id, int flags, Queue *lq);

/**
 * Create a solver instance for the given pool.
 * 
 * Configures solver flags for the package management workflow:
 * - Allows uninstall operations for conflict resolution
 * - Allows vendor changes for LFS flexibility
 * - Disables downgrade for security
 * - Disables auto-targeting for explicit job control
 * 
 * @param pool The pool to create the solver for
 * @return Configured solver instance, or NULL on failure
 */
Solver *create_solver(Pool *pool) {
    if (!pool) {
        log_error("Cannot create solver: Invalid pool.");
        return NULL;
    }

    Solver *solv = solver_create(pool);  // Z libsolv-bindings.3: solver_create(pool)
    if (!solv) {
        log_error("Failed to create solver.");
        return NULL;
    }

    // Nastav flags pro náš workflow (z constantids.3 a solv.c)
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_UNINSTALL, 1);     // Povol uninstall pro konflikty
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_VENDORCHANGE, 1);  // Povol vendor změny (pro LFS flexibilitu)
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_DOWNGRADE, 0);     // Zakáž downgrade pro bezpečnost (uprav podle potřeby)
    solver_set_flag(solv, SOLVER_FLAG_NO_AUTOTARGET, 1);       // Žádný auto-target pro explicitní jobs

    // Para Conda kompatibilitu (z plánu, i když není v constantids – předpokládám existenci)
    // pool_set_flag(pool, POOL_FLAG_WHATPROVIDESUSESCONDA, 1);  // Pokud existuje, nastav na pool

    // Note: Pool arch should already be set by caller (main.c)
    // Do NOT call set_system_arch here as it reinitializes the pool

    // Note: pool_createwhatprovides should already be called by load_repository
    // Do NOT call it again here as it would invalidate the whatprovides mapping

    log_info("Solver created with flags set and whatprovides indexed.");
    return solv;
}

/**
 * Recursively add makedeps to the job queue.
 * 
 * Separates build-time dependencies from runtime dependencies by extracting
 * custom makedeps metadata and adding them as install jobs. Includes depth
 * limiting to prevent infinite recursion from circular dependencies.
 * 
 * @param job The job queue to add makedeps to
 * @param p The solvable ID to process
 * @param pool The pool containing package information
 * @param repo The repository containing the solvable
 * @param depth Current recursion depth (starts at 0)
 */
void add_mkdeps_to_queue(Queue *job, Id p, Pool *pool, Repo *repo, int depth) {
    // Check recursion depth to prevent infinite loops from circular dependencies
    if (depth > MAX_RECURSION_DEPTH) {
        log_error("Recursion depth exceeded (%d) – possible circular dependency.", depth);
        return;
    }

    // Load repodata for this solvable (assumes repo has been internalized)
    Repodata *data = repo_last_repodata(repo);
    if (!data) {
        log_warn("No repodata for solvable %d – skipping mkdeps.", p);
        return;
    }

    // Get makedeps as JSON array from custom metadata
    json_t *mkdeps = get_custom_mkdeps(data, p, pool);
    if (!mkdeps || !json_is_array(mkdeps)) {
        log_debug(2, "No mkdeps for solvable %d.", p);
        if (mkdeps) json_decref(mkdeps);
        return;
    }

    size_t mkdep_count = json_array_size(mkdeps);
    if (mkdep_count == 0) {
        log_debug(2, "Empty mkdeps array for solvable %d.", p);
        json_decref(mkdeps);
        return;
    }

    // Process each makedep item (package name string)
    size_t i;
    json_t *item;
    json_array_foreach(mkdeps, i, item) {
        if (!json_is_string(item)) continue;

        const char *pkg_name = json_string_value(item);
        // Convert package name to pool ID (create=1 to add if not exists)
        Id pkg_id = pool_str2id(pool, pkg_name, 1);

        // Add to job queue as install operation
        queue_push2(job, SOLVER_INSTALL | SOLVER_SOLVABLE_NAME, pkg_id);
        log_debug(2, "Added mkdep '%s' to job queue at depth %d.", pkg_name, depth);

        // Recursion: Find the solvable for this package and process its makedeps
        Id q;
        Solvable *s;
        FOR_REPO_SOLVABLES(repo, q, s) {
            if (s->name == pkg_id) {
                // Recurse into this dependency's makedeps
                add_mkdeps_to_queue(job, q, pool, repo, depth + 1);
                break;  // Assume unique name in repo
            }
        }
    }

    json_decref(mkdeps);  // Cleanup (z jansson, jak v parser.c)
    log_debug(1, "Added %zu mkdeps for solvable %d at depth %d.", mkdep_count, p, depth);
}

/**
 * Resolve all dependencies for a package.
 * 
 * This is the main dependency resolution function that:
 * 1. Finds the target package solvable
 * 2. Recursively processes all makedeps (build-time dependencies)
 * 3. Adds the main package as a runtime install job
 * 4. Runs the solver to compute the installation transaction
 * 5. Handles any solver problems that arise
 * 
 * @param solv The solver instance to use
 * @param pkg_name Name of the package to resolve dependencies for
 * @param pool The pool containing package information
 * @param repo The repository to search for packages
 * @return Queue with success indicator (1) on success, NULL on failure
 *         Caller must free the returned queue with queue_free() and solv_free()
 */
Queue *solve_dependencies(Solver *solv, const char *pkg_name, Pool *pool, Repo *repo) {
    if (!solv || !pool || !repo || !pkg_name) {
        log_error("Invalid arguments in solve_dependencies.");
        return NULL;
    }

    // Allocate result queue to return success status
    Queue *result = solv_calloc(1, sizeof(Queue));
    queue_init(result);

    // Create job queue for solver operations
    Queue job;
    queue_init(&job);

    // Find the main solvable for the requested package name
    Id name_id = pool_str2id(pool, pkg_name, 0);  // create=0 (don't create if not exists)
    if (!name_id) {
        log_error("Package '%s' not found in pool string table.", pkg_name);
        goto cleanup;
    }

    Id main_p = 0;
    Solvable *s;
    FOR_REPO_SOLVABLES(repo, main_p, s) {
        if (s->name == name_id) break;  // První match
    }
    if (!main_p) {
        log_error("No solvable found for '%s' in repository.", pkg_name);
        goto cleanup;
    }
    
    log_debug(1, "Found solvable %d for package '%s'.", main_p, pkg_name);

    // First, recursively process all makedeps (build-time dependencies)
    // This ensures build dependencies are resolved before runtime dependencies
    add_mkdeps_to_queue(&job, main_p, pool, repo, 0);

    // Then add runtime dependencies: Add main package as SOLVER_INSTALL
    // Use SOLVER_SOLVABLE with solvable ID, not SOLVER_SOLVABLE_NAME
    queue_push2(&job, SOLVER_INSTALL | SOLVER_SOLVABLE, main_p);

    // Run the solver to compute the installation transaction
    int problem_count = solver_solve(solv, &job);
    if (problem_count > 0) {
        log_warn("Solver found %d problems.", problem_count);
        // Display detailed proof for each problem to help diagnose issues
        for (int prob = 1; prob <= problem_count; prob++) {
            display_problem_proof(solv, prob);
        }
        // In unit tests, continue despite problems; in production, ask user
        #ifndef UNIT_TESTS
        if (!user_confirm("Problems found. Continue anyway? [y/n]", 0)) {
            goto cleanup;
        }
        #endif
        log_info("Continuing despite solver problems...");
    } else {
        log_debug(1, "Solver completed successfully with no problems.");
    }

    // Transaction creation is handled by caller (main.c)
    // Set success flag in result to indicate solver completed
    queue_push(result, 1);

cleanup:
    queue_free(&job);
    if (result->count == 0) {
        queue_free(result);
        solv_free(result);
        return NULL;
    }
    return result;
}

/**
 * Display detailed proof information for a solver problem.
 * 
 * Shows learnt rules and decision traces that led to the problem.
 * Adapted from solv.c (doshowproof + showproof) to help diagnose
 * why the solver could not find a valid solution.
 * 
 * @param solv The solver instance containing the problem
 * @param problem The problem number to display (1-indexed)
 */
void display_problem_proof(Solver *solv, int problem) {
    if (!solv) {
        log_error("Invalid solver in display_problem_proof.");
        return;
    }

    Queue lq;
    int i;

    printf("\n");
    queue_init(&lq);
    
    // Get learnt rules (rules the solver derived during conflict resolution)
    solver_get_learnt(solv, problem, SOLVER_DECISIONLIST_PROBLEM, &lq);

    // Display each learnt rule
    for (i = 0; i < lq.count; i++) {
        printf("Learnt rule #%d:\n", i + 1);
        internal_display_proof(solv, lq.elements[i], SOLVER_DECISIONLIST_LEARNTRULE, &lq);
    }

    // Display the full decision proof that led to this problem
    printf("Proof:\n");
    internal_display_proof(solv, problem, SOLVER_DECISIONLIST_PROBLEM, &lq);

    queue_free(&lq);
}

/**
 * Internal helper to display proof information recursively.
 * 
 * Adapted from doshowproof in solv.c. Traverses the decision list
 * and displays the reasoning chain that led to each decision or conflict.
 * Filters out "obvious" decisions to reduce noise in the output.
 * 
 * @param solv The solver instance
 * @param id The decision/problem ID to display
 * @param flags Control flags for decision list retrieval
 * @param lq Learnt rules queue for reference
 */
static void internal_display_proof(Solver *solv, Id id, int flags, Queue *lq) {
    Pool *pool = solv->pool;
    Queue q, qp;
    int i, j;

    queue_init(&q);
    queue_init(&qp);
    
    // Get decision list with sorting and detailed information
    solver_get_decisionlist(solv, id, flags | SOLVER_DECISIONLIST_SORTED | SOLVER_DECISIONLIST_WITHINFO | SOLVER_DECISIONLIST_MERGEDINFO, &q);

    // Process each decision in the list (8 elements per decision)
    for (i = 0; i < q.count; i += 8) {
        Id v = q.elements[i];
        int reason = q.elements[i + 1], bits = q.elements[i + 3], type = q.elements[i + 4];
        Id from = q.elements[i + 5], to = q.elements[i + 6], dep = q.elements[i + 7];

        // Skip "obvious" decisions (same package name) unless unsolvable
        if (reason != SOLVER_REASON_UNSOLVABLE && type == SOLVER_RULE_PKG_SAME_NAME) {
            continue;
        }

        // Get affected solvables for this decision
        solver_decisionlist_solvables(solv, &q, i, &qp);
        if (qp.count) {
            i += qp.count * 8 - 8;  // Skip merged decisions
        }

        // Print decision type and affected packages
        if (reason == SOLVER_REASON_UNSOLVABLE) {
            printf("unsolvable: ");
        } else {
            printf("%s %s: ", v < 0 ? "conflicted" : "installed", pool_solvidset2str(pool, &qp));
        }

        // If no specific rule type, just print the reason
        if (type == 0) {
            printf("%s\n", solver_reason2str(solv, reason));
            continue;
        }

        // Check if this is a learnt rule and reference it
        if (type == SOLVER_RULE_LEARNT && lq) {
            for (j = 0; j < lq->count; j++) {
                if (lq->elements[j] == q.elements[i + 2]) break;
            }
            if (j < lq->count) {
                printf("learnt rule #%d\n", j + 1);
                continue;
            }
        }

        // Print detailed decision information
        printf("%s\n", solver_decisioninfo2str(solv, bits, type, from, to, dep));
    }

    queue_free(&qp);
    queue_free(&q);
    printf("\n");
}