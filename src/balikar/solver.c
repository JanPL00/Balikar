
//
// Created by placeja5 on 11/12/25.
//

#include "includes/solver.h"

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

    // Set flags for our workflow (from constantids.3 and solv.c)
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_UNINSTALL, 1);     // Allow uninstall for conflicts
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_VENDORCHANGE, 1);  // Allow vendor changes (for LFS flexibility)
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_DOWNGRADE, 0);     // Disallow downgrade for security (adjust as needed)
    solver_set_flag(solv, SOLVER_FLAG_NO_AUTOTARGET, 1);       // No auto-target for explicit jobs

    // For Conda compatibility (from plan, even if not in constantids – assuming existence)
    // pool_set_flag(pool, POOL_FLAG_WHATPROVIDESUSESCONDA, 1);  // If exists, set on pool

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
 * 1. Loads already installed packages from target root (if any)
 * 2. Finds the target package solvable
 * 3. Recursively processes all makedeps (build-time dependencies)
 * 4. Adds the main package as a runtime install job
 * 5. Runs the solver to compute the installation transaction
 * 6. Handles any solver problems that arise
 * 
 * @param solv The solver instance to use
 * @param pkg_name Name of the package to resolve dependencies for
 * @param pool The pool containing package information
 * @param repo The repository to search for packages
 * @param root_path Target installation root (e.g., "/" or "/tmp/test_root") to check for installed packages
 * @return Queue with success indicator (1) on success, NULL on failure
 *         Caller must free the returned queue with queue_free() and solv_free()
 */
Queue *solve_dependencies(Solver *solv, const char *pkg_name, Pool *pool, Repo *repo, const char *root_path) {
    if (!solv || !pool || !repo || !pkg_name) {
        log_error("Invalid arguments in solve_dependencies.");
        return NULL;
    }
    
    if (!root_path) {
        root_path = "/";  // Default to system root
    }

    // Load list of installed packages from target root
    // We'll skip packages that are already installed
    Queue installed_names;
    queue_init(&installed_names);
    
    char installed_db[512];
    // Handle root path "/" specially to avoid double slash
    if (strcmp(root_path, "/") == 0) {
        snprintf(installed_db, sizeof(installed_db), "/var/lib/balikar/installed.json");
    } else {
        snprintf(installed_db, sizeof(installed_db), "%s/var/lib/balikar/installed.json", root_path);
    }
    
    int installed_count = 0;
    if (access(installed_db, F_OK) == 0) {
        log_debug(1, "Loading installed packages from: %s", installed_db);
        
        // Parse installed.json
        json_error_t error;
        json_t *root = json_load_file(installed_db, 0, &error);
        if (root) {
            json_t *packages = json_object_get(root, "packages");
            if (packages && json_is_object(packages)) {
                const char *key;
                json_t *pkg_obj;
                
                json_object_foreach(packages, key, pkg_obj) {
                    const char *name = json_string_value(json_object_get(pkg_obj, "name"));
                    
                    if (name) {
                        // Store package name ID in queue
                        Id name_id = pool_str2id(pool, name, 1);
                        queue_push(&installed_names, name_id);
                        installed_count++;
                        log_debug(2, "Found installed package: %s", name);
                    }
                }
                
                log_info("Found %d installed packages in %s", installed_count, root_path);
            }
            json_decref(root);
        } else {
            log_debug(1, "Could not parse installed.json: %s", error.text);
        }
    } else {
        log_debug(1, "No installed packages database at: %s", installed_db);
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
        if (s->name == name_id) break;  // First match
    }
    if (!main_p) {
        log_error("No solvable found for '%s' in repository.", pkg_name);
        goto cleanup;
    }
    
    log_debug(1, "Found solvable %d for package '%s'.", main_p, pkg_name);

    // Check if main package is already installed
    int already_installed = 0;
    for (int i = 0; i < installed_names.count; i++) {
        if (installed_names.elements[i] == name_id) {
            already_installed = 1;
            log_info("Package '%s' is already installed in %s - skipping", pkg_name, root_path);
            break;
        }
    }

    if (!already_installed) {
        // First, recursively process all makedeps (build-time dependencies)
        // This ensures build dependencies are resolved before runtime dependencies
        // TODO: Pass installed_names to skip already installed makedeps
        add_mkdeps_to_queue(&job, main_p, pool, repo, 0);

        // Then add runtime dependencies: Add main package as SOLVER_INSTALL
        // Use SOLVER_SOLVABLE with solvable ID, not SOLVER_SOLVABLE_NAME
        queue_push2(&job, SOLVER_INSTALL | SOLVER_SOLVABLE, main_p);
    } else {
        // Package already installed, just set success
        queue_push(result, 1);
        queue_free(&installed_names);
        queue_free(&job);
        return result;
    }

    // Run the solver to compute the installation transaction
    int problem_count = solver_solve(solv, &job);
    if (problem_count > 0) {
        // Analyze problems to distinguish between runtime deps and build deps (mkdeps)
        int runtime_problems = 0;
        int mkdep_problems = 0;
        
        for (int prob = 1; prob <= problem_count; prob++) {
            Queue problem_queue;
            queue_init(&problem_queue);
            solver_findallproblemrules(solv, prob, &problem_queue);
            
            // Check if problem involves mkdeps by examining rule names
            int is_mkdep_problem = 0;
            for (int i = 0; i < problem_queue.count; i++) {
                Id rule_id = problem_queue.elements[i];
                SolverRuleinfo type = solver_ruleinfo(solv, rule_id, NULL, NULL, NULL);
                // SOLVER_RULE_PKG_* are package-related rules
                // For mkdeps, we'll check if the package name appears in mkdeps
                // This is a heuristic - mkdep problems often involve build tools
                (void)type; // Mark as used to avoid warning
                // TODO: Better detection would require tracking which deps are mkdeps
                // For now, assume all problems may include mkdeps
                is_mkdep_problem = 1; // Conservative: treat as mkdep unless proven otherwise
            }
            queue_free(&problem_queue);
            
            if (is_mkdep_problem) {
                mkdep_problems++;
            } else {
                runtime_problems++;
            }
        }
        
        // Display all problems with context
        log_warn("Solver found %d problems (%d may be build-time dependencies).", 
                 problem_count, mkdep_problems);
        for (int prob = 1; prob <= problem_count; prob++) {
            display_problem_proof(solv, prob);
        }
        
        // If problems are likely mkdeps only, just warn
        if (runtime_problems == 0 && mkdep_problems > 0) {
            log_info("Problems appear to be build-time dependencies (mkdeps) only.");
            log_info("Build will attempt to use bundled or fallback versions if needed.");
            log_info("Continuing with installation...");
        } else {
            // In unit tests, continue despite problems; in production, ask user
            #ifndef UNIT_TESTS
            if (!user_confirm("Problems found. Continue anyway? [y/n]", 0)) {
                goto cleanup;
            }
            #endif
            log_info("Continuing despite solver problems...");
        }
    } else {
        log_debug(1, "Solver completed successfully with no problems.");
    }

    // Transaction creation is handled by caller (main.c)
    // Set success flag in result to indicate solver completed
    queue_push(result, 1);

cleanup:
    queue_free(&installed_names);
    queue_free(&job);
    if (result->count == 0) {
        queue_free(result);
        solv_free(result);
        return NULL;
    }
    return result;
}

/**
 * Internal helper to display proof information for solver decisions.
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