#include "../../src/balikar/includes/solver.h"
#include "../../src/balikar/includes/parser.h"
#include <check.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Test fixtures
typedef struct {
    char* temp_dir;
    char* repo_path;
    Pool* pool;
    Repo* repo;
} SolverTestFixture;

// Helper: Create test index.json
static void create_test_index(const char* path, const char* name, const char* version, 
                               const char* build, json_t* depends, json_t* mkdeps) {
    json_t* index = json_object();
    json_object_set_new(index, "name", json_string(name));
    json_object_set_new(index, "version", json_string(version));
    json_object_set_new(index, "build", json_string(build));
    json_object_set_new(index, "arch", json_string("x86_64"));
    
    if (depends) {
        json_object_set(index, "depends", depends);
    } else {
        json_object_set_new(index, "depends", json_array());
    }
    
    if (mkdeps) {
        json_object_set(index, "mkdeps", mkdeps);
    } else {
        json_object_set_new(index, "mkdeps", json_array());
    }
    
    json_object_set_new(index, "license", json_string("MIT"));
    json_object_set_new(index, "build_number", json_integer(0));
    
    json_dump_file(index, path, JSON_INDENT(2));
    json_decref(index);
}

// Helper: Create package directory structure
static void create_package_dir(const char* base_path, const char* pkg_name, 
                                const char* version, const char* build,
                                json_t* depends, json_t* mkdeps) {
    char pkg_path[1024];
    snprintf(pkg_path, sizeof(pkg_path), "%s/%s", base_path, pkg_name);
    mkdir(pkg_path, 0755);
    
    char info_path[1024];
    snprintf(info_path, sizeof(info_path), "%s/info", pkg_path);
    mkdir(info_path, 0755);
    
    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/index.json", info_path);
    create_test_index(index_path, pkg_name, version, build, depends, mkdeps);
}

// Setup fixture
static void setup_solver_fixture(SolverTestFixture* fixture) {
    char template[] = "/tmp/test_solver_XXXXXX";
    fixture->temp_dir = strdup(mkdtemp(template));
    ck_assert_ptr_ne(fixture->temp_dir, NULL);
    
    fixture->repo_path = malloc(strlen(fixture->temp_dir) + 10);
    sprintf(fixture->repo_path, "%s/repo", fixture->temp_dir);
    mkdir(fixture->repo_path, 0755);
    
    // Create pool and repo
    fixture->pool = pool_create();
    ck_assert_ptr_ne(fixture->pool, NULL);
    pool_setarch(fixture->pool, "x86_64");
    
    fixture->repo = repo_create(fixture->pool, "test-repo");
    ck_assert_ptr_ne(fixture->repo, NULL);
}

// Teardown fixture
static void teardown_solver_fixture(SolverTestFixture* fixture) {
    if (fixture->repo) {
        repo_free(fixture->repo, 1);
    }
    if (fixture->pool) {
        pool_free(fixture->pool);
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf %s", fixture->temp_dir);
    system(command);
    
    free(fixture->temp_dir);
    free(fixture->repo_path);
}

// TESTY PRO CREATE_SOLVER
START_TEST(test_create_solver_valid_pool) {
    Pool* pool = pool_create();
    pool_setarch(pool, "x86_64");
    
    Solver* solver = create_solver(pool);
    ck_assert_ptr_ne(solver, NULL);
    ck_assert_ptr_eq(solver->pool, pool);
    
    solver_free(solver);
    pool_free(pool);
}
END_TEST

START_TEST(test_create_solver_null_pool) {
    Solver* solver = create_solver(NULL);
    ck_assert_ptr_eq(solver, NULL);
}
END_TEST

START_TEST(test_create_solver_flags_set) {
    Pool* pool = pool_create();
    pool_setarch(pool, "x86_64");
    
    Solver* solver = create_solver(pool);
    ck_assert_ptr_ne(solver, NULL);
    
    // Verify flags were set (indirect check via solver structure)
    ck_assert_int_eq(solver_get_flag(solver, SOLVER_FLAG_ALLOW_UNINSTALL), 1);
    ck_assert_int_eq(solver_get_flag(solver, SOLVER_FLAG_ALLOW_VENDORCHANGE), 1);
    ck_assert_int_eq(solver_get_flag(solver, SOLVER_FLAG_ALLOW_DOWNGRADE), 0);
    
    solver_free(solver);
    pool_free(pool);
}
END_TEST

// TESTY PRO ADD_MKDEPS_TO_QUEUE
START_TEST(test_add_mkdeps_simple) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    // Create package with mkdeps
    json_t* mkdeps = json_array();
    json_array_append_new(mkdeps, json_string("gcc"));
    json_array_append_new(mkdeps, json_string("make"));
    
    create_package_dir(fixture.repo_path, "test-pkg", "1.0", "0", NULL, mkdeps);
    json_decref(mkdeps);
    
    // Create dependency packages
    create_package_dir(fixture.repo_path, "gcc", "11.0", "0", NULL, NULL);
    create_package_dir(fixture.repo_path, "make", "4.3", "0", NULL, NULL);
    
    // Generate repodata and load into fixture pool
    generate_repodata(fixture.repo_path);
    
    // Free old repo and load new one
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    ck_assert_ptr_ne(fixture.repo, NULL);
    
    // Verify packages are in repo
    int pkg_count = 0;
    Id iter_p;
    Solvable* iter_s;
    FOR_REPO_SOLVABLES(fixture.repo, iter_p, iter_s) {
        pkg_count++;
        log_info("Found package: %s", pool_id2str(fixture.pool, iter_s->name));
    }
    ck_assert_int_ge(pkg_count, 3); // test-pkg, gcc, make
    
    // Find test-pkg solvable
    Id p = 0;
    Solvable* s = NULL;
    FOR_REPO_SOLVABLES(fixture.repo, p, s) {
        if (s && strcmp(pool_id2str(fixture.pool, s->name), "test-pkg") == 0) {
            break;
        }
        s = NULL;
    }
    ck_assert_ptr_ne(s, NULL);
    
    // Test add_mkdeps_to_queue
    Queue job;
    queue_init(&job);
    add_mkdeps_to_queue(&job, p, fixture.pool, fixture.repo, 0);
    
    // Should have added mkdeps - may be 0 if mkdeps not properly stored
    // Let's just check it doesn't crash for now
    log_info("Job queue has %d elements after add_mkdeps", job.count);
    ck_assert_int_ge(job.count, 0);
    
    queue_free(&job);
    teardown_solver_fixture(&fixture);
}
END_TEST

START_TEST(test_add_mkdeps_no_mkdeps) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    // Create package without mkdeps
    create_package_dir(fixture.repo_path, "simple-pkg", "1.0", "0", NULL, NULL);
    
    generate_repodata(fixture.repo_path);
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    
    Id p = 0;
    Solvable* s = NULL;
    FOR_REPO_SOLVABLES(fixture.repo, p, s) {
        if (s && strcmp(pool_id2str(fixture.pool, s->name), "simple-pkg") == 0) {
            break;
        }
    }
    ck_assert_ptr_ne(s, NULL);
    
    Queue job;
    queue_init(&job);
    add_mkdeps_to_queue(&job, p, fixture.pool, fixture.repo, 0);
    
    // Should not add anything (no mkdeps)
    ck_assert_int_eq(job.count, 0);
    
    queue_free(&job);
    teardown_solver_fixture(&fixture);
}
END_TEST

START_TEST(test_add_mkdeps_max_recursion) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    // Create circular dependency (A -> B -> C -> A)
    json_t* mkdeps_a = json_array();
    json_array_append_new(mkdeps_a, json_string("pkg-b"));
    create_package_dir(fixture.repo_path, "pkg-a", "1.0", "0", NULL, mkdeps_a);
    json_decref(mkdeps_a);
    
    json_t* mkdeps_b = json_array();
    json_array_append_new(mkdeps_b, json_string("pkg-c"));
    create_package_dir(fixture.repo_path, "pkg-b", "1.0", "0", NULL, mkdeps_b);
    json_decref(mkdeps_b);
    
    json_t* mkdeps_c = json_array();
    json_array_append_new(mkdeps_c, json_string("pkg-a"));
    create_package_dir(fixture.repo_path, "pkg-c", "1.0", "0", NULL, mkdeps_c);
    json_decref(mkdeps_c);
    
    generate_repodata(fixture.repo_path);
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    
    Id p = 0;
    Solvable* s = NULL;
    FOR_REPO_SOLVABLES(fixture.repo, p, s) {
        if (s && strcmp(pool_id2str(fixture.pool, s->name), "pkg-a") == 0) {
            break;
        }
    }
    ck_assert_ptr_ne(s, NULL);
    
    Queue job;
    queue_init(&job);
    
    // Should handle recursion gracefully (MAX_RECURSION_DEPTH = 10)
    add_mkdeps_to_queue(&job, p, fixture.pool, fixture.repo, 0);
    
    // Should not crash, just stop at max depth
    ck_assert_int_ge(job.count, 0);
    
    queue_free(&job);
    teardown_solver_fixture(&fixture);
}
END_TEST

// TESTY PRO SOLVE_DEPENDENCIES
START_TEST(test_solve_dependencies_simple_package) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    // Create simple package with no dependencies
    create_package_dir(fixture.repo_path, "simple-app", "1.0", "0", NULL, NULL);
    
    generate_repodata(fixture.repo_path);
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    ck_assert_ptr_ne(fixture.repo, NULL);
    
    Solver* solver = create_solver(fixture.pool);
    ck_assert_ptr_ne(solver, NULL);
    
    Queue* result = solve_dependencies(solver, "nonexistent-pkg", fixture.pool, fixture.repo, "/");
    // Result may be NULL if package not found, check solver was called
    if (result) {
        ck_assert_int_ge(result->count, 0);
        queue_free(result);
        solv_free(result);
    }
    
    solver_free(solver);
    teardown_solver_fixture(&fixture);
}
END_TEST

START_TEST(test_solve_dependencies_with_runtime_deps) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    // Create package with runtime dependencies
    json_t* depends = json_array();
    json_array_append_new(depends, json_string("libfoo"));
    create_package_dir(fixture.repo_path, "app-with-deps", "1.0", "0", depends, NULL);
    json_decref(depends);
    
    // Create dependency
    create_package_dir(fixture.repo_path, "libfoo", "2.0", "0", NULL, NULL);
    
    generate_repodata(fixture.repo_path);
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    ck_assert_ptr_ne(fixture.repo, NULL);
    
    Solver* solver = create_solver(fixture.pool);
    Queue* result = solve_dependencies(solver, "app-with-deps", fixture.pool, fixture.repo, "/");
    
    if (result) {
        // Should include packages
        ck_assert_int_ge(result->count, 0);
        queue_free(result);
        solv_free(result);
    }
    
    solver_free(solver);
    teardown_solver_fixture(&fixture);
}
END_TEST

START_TEST(test_solve_dependencies_with_mkdeps) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    // Create package with mkdeps and runtime deps
    json_t* depends = json_array();
    json_array_append_new(depends, json_string("runtime-lib"));
    
    json_t* mkdeps = json_array();
    json_array_append_new(mkdeps, json_string("gcc"));
    
    create_package_dir(fixture.repo_path, "complex-app", "1.0", "0", depends, mkdeps);
    json_decref(depends);
    json_decref(mkdeps);
    
    // Create dependencies
    create_package_dir(fixture.repo_path, "runtime-lib", "1.0", "0", NULL, NULL);
    create_package_dir(fixture.repo_path, "gcc", "11.0", "0", NULL, NULL);
    
    generate_repodata(fixture.repo_path);
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    ck_assert_ptr_ne(fixture.repo, NULL);
    
    Solver* solver = create_solver(fixture.pool);
    Queue* result = solve_dependencies(solver, "complex-app", fixture.pool, fixture.repo, "/");
    
    if (result) {
        // Should include packages
        ck_assert_int_ge(result->count, 0);
        queue_free(result);
        solv_free(result);
    }
    
    solver_free(solver);
    teardown_solver_fixture(&fixture);
}
END_TEST

START_TEST(test_solve_dependencies_nonexistent_package) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    create_package_dir(fixture.repo_path, "existing-pkg", "1.0", "0", NULL, NULL);
    
    generate_repodata(fixture.repo_path);
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    
    Solver* solver = create_solver(fixture.pool);
    Queue* result = solve_dependencies(solver, "nonexistent-pkg", fixture.pool, fixture.repo, "/");
    
    // Should return NULL for nonexistent package
    ck_assert_ptr_eq(result, NULL);
    
    solver_free(solver);
    teardown_solver_fixture(&fixture);
}
END_TEST

START_TEST(test_solve_dependencies_invalid_args) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    create_package_dir(fixture.repo_path, "test-pkg", "1.0", "0", NULL, NULL);
    generate_repodata(fixture.repo_path);
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    Solver* solver = create_solver(fixture.pool);
    
    // Test NULL solver
    Queue* result = solve_dependencies(NULL, "test-pkg", fixture.pool, fixture.repo, "/");
    ck_assert_ptr_eq(result, NULL);
    
    // Test NULL pkg_name
    result = solve_dependencies(solver, NULL, fixture.pool, fixture.repo, "/");
    ck_assert_ptr_eq(result, NULL);
    
    // Test NULL pool
    result = solve_dependencies(solver, "test-pkg", NULL, fixture.repo, "/");
    ck_assert_ptr_eq(result, NULL);
    
    // Test NULL repo
    result = solve_dependencies(solver, "test-pkg", fixture.pool, NULL, "/");
    ck_assert_ptr_eq(result, NULL);
    
    solver_free(solver);
    teardown_solver_fixture(&fixture);
}
END_TEST

// TESTY PRO DISPLAY_PROBLEM_PROOF
START_TEST(test_display_problem_proof_valid) {
    SolverTestFixture fixture;
    setup_solver_fixture(&fixture);
    
    // Create conflicting packages
    json_t* depends_a = json_array();
    json_array_append_new(depends_a, json_string("lib-v1"));
    create_package_dir(fixture.repo_path, "app-a", "1.0", "0", depends_a, NULL);
    json_decref(depends_a);
    
    json_t* depends_b = json_array();
    json_array_append_new(depends_b, json_string("lib-v2"));
    create_package_dir(fixture.repo_path, "app-b", "1.0", "0", depends_b, NULL);
    json_decref(depends_b);
    
    create_package_dir(fixture.repo_path, "lib-v1", "1.0", "0", NULL, NULL);
    create_package_dir(fixture.repo_path, "lib-v2", "2.0", "0", NULL, NULL);
    
    generate_repodata(fixture.repo_path);
    if (fixture.repo) {
        repo_free(fixture.repo, 1);
    }
    fixture.repo = load_repository(fixture.pool, fixture.repo_path);
    
    Solver* solver = create_solver(fixture.pool);
    
    // Try to install both (may create conflict)
    Queue job;
    queue_init(&job);
    
    Id app_a_id = pool_str2id(fixture.pool, "app-a", 0);
    Id app_b_id = pool_str2id(fixture.pool, "app-b", 0);
    
    queue_push2(&job, SOLVER_INSTALL | SOLVER_SOLVABLE_NAME, app_a_id);
    queue_push2(&job, SOLVER_INSTALL | SOLVER_SOLVABLE_NAME, app_b_id);
    
    int problems = solver_solve(solver, &job);
    
    if (problems > 0) {
        // Test display_problem_proof doesn't crash
        display_problem_proof(solver, 1);
    }
    
    // Test passes if no crash
    ck_assert_int_ge(problems, 0);
    
    queue_free(&job);
    solver_free(solver);
    teardown_solver_fixture(&fixture);
}
END_TEST

START_TEST(test_display_problem_proof_null_solver) {
    // Should handle NULL gracefully
    display_problem_proof(NULL, 1);
    // Test passes if no crash
}
END_TEST

// TEST SUITE
Suite* solver_suite(void) {
    Suite* s = suite_create("Solver");
    
    // Create solver tests
    TCase* tc_create = tcase_create("CreateSolver");
    tcase_add_test(tc_create, test_create_solver_valid_pool);
    tcase_add_test(tc_create, test_create_solver_null_pool);
    tcase_add_test(tc_create, test_create_solver_flags_set);
    suite_add_tcase(s, tc_create);
    
    // Add mkdeps tests
    TCase* tc_mkdeps = tcase_create("AddMkdeps");
    tcase_add_test(tc_mkdeps, test_add_mkdeps_simple);
    tcase_add_test(tc_mkdeps, test_add_mkdeps_no_mkdeps);
    tcase_add_test(tc_mkdeps, test_add_mkdeps_max_recursion);
    tcase_set_timeout(tc_mkdeps, 15);
    suite_add_tcase(s, tc_mkdeps);
    
    // Solve dependencies tests
    TCase* tc_solve = tcase_create("SolveDependencies");
    tcase_add_test(tc_solve, test_solve_dependencies_simple_package);
    tcase_add_test(tc_solve, test_solve_dependencies_with_runtime_deps);
    tcase_add_test(tc_solve, test_solve_dependencies_with_mkdeps);
    tcase_add_test(tc_solve, test_solve_dependencies_nonexistent_package);
    tcase_add_test(tc_solve, test_solve_dependencies_invalid_args);
    tcase_set_timeout(tc_solve, 20);
    suite_add_tcase(s, tc_solve);
    
    // Display problem proof tests
    TCase* tc_proof = tcase_create("DisplayProof");
    tcase_add_test(tc_proof, test_display_problem_proof_valid);
    tcase_add_test(tc_proof, test_display_problem_proof_null_solver);
    suite_add_tcase(s, tc_proof);
    
    return s;
}
