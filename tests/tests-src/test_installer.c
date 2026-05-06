//
// Created by placeja5 on 11/20/25.
//

#include <check.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../../src/balikar/includes/installer.h"
#include "../../src/balikar/includes/parser.h"
#include "../../src/common/common.h"

// Test: Basic validation
START_TEST(test_validate_chroot_basic) {
    // Create a temporary directory structure
    system("mkdir -p /tmp/test_chroot_valid/usr/bin");
    system("mkdir -p /tmp/test_chroot_valid/lib");
    system("mkdir -p /tmp/test_chroot_valid/bin");
    
    // Should validate successfully
    int result = validate_chroot("/tmp/test_chroot_valid");
    ck_assert_int_eq(result, 1);
    
    // Cleanup
    system("rm -rf /tmp/test_chroot_valid");
}
END_TEST

// Test: Invalid chroot (doesn't exist)
START_TEST(test_validate_chroot_nonexistent) {
    // Provide fake input for user_confirm
    FILE *fake_in = fopen("/tmp/test_input", "w");
    fprintf(fake_in, "n\n");
    fclose(fake_in);
    FILE *orig_in = stdin;
    stdin = freopen("/tmp/test_input", "r", stdin);

    int result = validate_chroot("/tmp/this_definitely_does_not_exist_12345");
    ck_assert_int_eq(result, 0);

    // Restore
    stdin = orig_in;
    system("rm -rf /tmp/test_input");
}
END_TEST

// Test: Check if package is installed
START_TEST(test_is_package_installed) {
    // Should return 0 or 1, not -1 (error)
    int result = is_package_installed("nonexistent_package_xyz", "/");
    ck_assert_int_ge(result, 0);
}
END_TEST

// Test: Get installed packages
START_TEST(test_get_installed_packages) {
    json_t *packages = get_installed_packages("/");
    ck_assert_ptr_nonnull(packages);
    
    // Should be an array
    ck_assert(json_is_array(packages));
    
    json_decref(packages);
}
END_TEST

// Test: Install package basic flow
START_TEST(test_install_package_basic) {
    // Setup: Create mock staging directory
    system("mkdir -p /tmp/test_staging/usr/bin");
    system("echo '#!/bin/bash' > /tmp/test_staging/usr/bin/test_program");
    system("chmod +x /tmp/test_staging/usr/bin/test_program");
    
    // Setup: Create mock chroot
    system("mkdir -p /tmp/test_chroot_install/usr/bin");
    system("mkdir -p /tmp/test_chroot_install/lib");
    system("mkdir -p /tmp/test_chroot_install/bin");
    
    // Install
    int result = install_package("test_pkg", "1.0", 
                                  "/tmp/test_staging", 
                                  "/tmp/test_chroot_install");
    
    // Should succeed
    ck_assert_int_eq(result, 0);
    
    // Verify file was copied
    struct stat st;
    int file_exists = (stat("/tmp/test_chroot_install/usr/bin/test_program", &st) == 0);
    ck_assert_int_eq(file_exists, 1);
    
    // Cleanup
    system("rm -rf /tmp/test_staging");
    system("rm -rf /tmp/test_chroot_install");
}
END_TEST

// Test: Install with post-install script
START_TEST(test_install_with_script) {
    // Setup: Create mock staging directory with .install script
    system("mkdir -p /tmp/test_staging_script/usr/bin");
    system("mkdir -p /tmp/test_staging_script/.install");
    system("echo '#!/bin/bash' > /tmp/test_staging_script/usr/bin/test_program");
    system("chmod +x /tmp/test_staging_script/usr/bin/test_program");
    
    // Create install script that creates a marker file
    system("echo '#!/bin/bash\ntouch $DESTDIR/usr/bin/post_install_marker' > /tmp/test_staging_script/.install/install.sh");
    system("chmod +x /tmp/test_staging_script/.install/install.sh");
    
    // Setup: Create mock chroot
    system("mkdir -p /tmp/test_chroot_script/usr/bin");
    system("mkdir -p /tmp/test_chroot_script/lib");
    system("mkdir -p /tmp/test_chroot_script/bin");
    
    // Install
    int result = install_package("test_pkg_script", "1.0",
                                  "/tmp/test_staging_script",
                                  "/tmp/test_chroot_script");
    
    // Should succeed
    ck_assert_int_eq(result, 0);
    
    // Verify post-install script ran
    struct stat st;
    int marker_exists = (stat("/tmp/test_chroot_script/usr/bin/post_install_marker", &st) == 0);
    ck_assert_int_eq(marker_exists, 1);
    
    // Cleanup
    system("rm -rf /tmp/test_staging_script");
    system("rm -rf /tmp/test_chroot_script");
}
END_TEST

// ===== Tests for decoupled dependency-tracking functions =====

// Helper: create a mock installed.json with given packages
static void create_mock_installed_db(const char *chroot, json_t *packages_array) {
    char db_dir[512];
    snprintf(db_dir, sizeof(db_dir), "%s/var/lib/balikar", chroot);
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", db_dir);
    system(cmd);

    json_t *root = json_object();
    json_object_set_new(root, "packages", json_deep_copy(packages_array));

    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/installed.json", db_dir);
    json_dump_file(root, db_path, JSON_INDENT(2));
    json_decref(root);
}

// Helper: build a minimal pool+repo with two packages where "app" depends on "lib"
static void setup_dep_pool(Pool **out_pool, Repo **out_repo) {
    Pool *pool = pool_create();
    pool_setarch(pool, "x86_64");
    Repo *repo = repo_create(pool, "test");

    // Create "lib" solvable
    Id lib_id = repo_add_solvable(repo);
    Solvable *lib_s = pool_id2solvable(pool, lib_id);
    lib_s->name = pool_str2id(pool, "lib", 1);
    lib_s->evr  = pool_str2id(pool, "1.0", 1);
    lib_s->arch = pool_str2id(pool, "x86_64", 1);
    lib_s->provides = repo_addid_dep(repo, 0, pool_str2id(pool, "lib", 1), 0);

    // Create "app" solvable that requires "lib"
    Id app_id = repo_add_solvable(repo);
    Solvable *app_s = pool_id2solvable(pool, app_id);
    app_s->name = pool_str2id(pool, "app", 1);
    app_s->evr  = pool_str2id(pool, "2.0", 1);
    app_s->arch = pool_str2id(pool, "x86_64", 1);
    app_s->provides = repo_addid_dep(repo, 0, pool_str2id(pool, "app", 1), 0);
    app_s->requires = repo_addid_dep(repo, 0, pool_str2id(pool, "lib", 1), 0);

    pool_createwhatprovides(pool);
    *out_pool = pool;
    *out_repo = repo;
}

START_TEST(test_find_reverse_deps_basic) {
    Pool *pool; Repo *repo;
    setup_dep_pool(&pool, &repo);

    const char *chroot = "/tmp/test_revdep";
    system("rm -rf /tmp/test_revdep");

    // Create installed DB with "app" and "lib"
    json_t *pkgs = json_array();
    json_t *app = json_object();
    json_object_set_new(app, "name", json_string("app"));
    json_object_set_new(app, "version", json_string("2.0"));
    json_object_set_new(app, "install_reason", json_string("manual"));
    json_array_append_new(pkgs, app);
    json_t *lib = json_object();
    json_object_set_new(lib, "name", json_string("lib"));
    json_object_set_new(lib, "version", json_string("1.0"));
    json_object_set_new(lib, "install_reason", json_string("dependency"));
    json_array_append_new(pkgs, lib);
    create_mock_installed_db(chroot, pkgs);
    json_decref(pkgs);

    // "lib" should have "app" as reverse dependency
    json_t *revdeps = find_reverse_dependencies("lib", chroot, pool, repo, load_installed_db);
    ck_assert_ptr_nonnull(revdeps);
    ck_assert_int_eq(json_array_size(revdeps), 1);
    ck_assert_str_eq(json_string_value(json_array_get(revdeps, 0)), "app");
    json_decref(revdeps);

    // "app" should have no reverse dependencies
    json_t *revdeps2 = find_reverse_dependencies("app", chroot, pool, repo, load_installed_db);
    ck_assert_ptr_nonnull(revdeps2);
    ck_assert_int_eq(json_array_size(revdeps2), 0);
    json_decref(revdeps2);

    repo_free(repo, 1);
    pool_free(pool);
    system("rm -rf /tmp/test_revdep");
}
END_TEST

START_TEST(test_find_orphaned_packages_basic) {
    Pool *pool; Repo *repo;
    setup_dep_pool(&pool, &repo);

    const char *chroot = "/tmp/test_orphan";
    system("rm -rf /tmp/test_orphan");

    // Only "lib" installed as dependency (app removed) → lib is orphaned
    json_t *pkgs = json_array();
    json_t *lib = json_object();
    json_object_set_new(lib, "name", json_string("lib"));
    json_object_set_new(lib, "version", json_string("1.0"));
    json_object_set_new(lib, "install_reason", json_string("dependency"));
    json_array_append_new(pkgs, lib);
    create_mock_installed_db(chroot, pkgs);
    json_decref(pkgs);

    json_t *orphans = find_orphaned_packages(chroot, pool, repo, load_installed_db);
    ck_assert_ptr_nonnull(orphans);
    ck_assert_int_ge(json_array_size(orphans), 1);
    ck_assert_str_eq(json_string_value(json_array_get(orphans, 0)), "lib");
    json_decref(orphans);

    repo_free(repo, 1);
    pool_free(pool);
    system("rm -rf /tmp/test_orphan");
}
END_TEST

START_TEST(test_find_orphans_none_when_dep_needed) {
    Pool *pool; Repo *repo;
    setup_dep_pool(&pool, &repo);

    const char *chroot = "/tmp/test_no_orphan";
    system("rm -rf /tmp/test_no_orphan");

    // Both "app" (manual) and "lib" (dependency) installed → lib is NOT orphaned
    json_t *pkgs = json_array();
    json_t *app = json_object();
    json_object_set_new(app, "name", json_string("app"));
    json_object_set_new(app, "version", json_string("2.0"));
    json_object_set_new(app, "install_reason", json_string("manual"));
    json_array_append_new(pkgs, app);
    json_t *lib = json_object();
    json_object_set_new(lib, "name", json_string("lib"));
    json_object_set_new(lib, "version", json_string("1.0"));
    json_object_set_new(lib, "install_reason", json_string("dependency"));
    json_array_append_new(pkgs, lib);
    create_mock_installed_db(chroot, pkgs);
    json_decref(pkgs);

    json_t *orphans = find_orphaned_packages(chroot, pool, repo, load_installed_db);
    ck_assert_ptr_nonnull(orphans);
    ck_assert_int_eq(json_array_size(orphans), 0);
    json_decref(orphans);

    repo_free(repo, 1);
    pool_free(pool);
    system("rm -rf /tmp/test_no_orphan");
}
END_TEST

START_TEST(test_find_reverse_deps_null_args) {
    json_t *r = find_reverse_dependencies(NULL, "/tmp", NULL, NULL, load_installed_db);
    ck_assert_ptr_null(r);
}
END_TEST

Suite *installer_suite(void) {
    Suite *s = suite_create("Installer");
    
    TCase *tc_validation = tcase_create("Validation");
    tcase_add_test(tc_validation, test_validate_chroot_basic);
    tcase_add_test(tc_validation, test_validate_chroot_nonexistent);
    suite_add_tcase(s, tc_validation);
    
    TCase *tc_tracking = tcase_create("Tracking");
    tcase_add_test(tc_tracking, test_is_package_installed);
    tcase_add_test(tc_tracking, test_get_installed_packages);
    suite_add_tcase(s, tc_tracking);
    
    TCase *tc_install = tcase_create("Installation");
    tcase_add_test(tc_install, test_install_package_basic);
    tcase_add_test(tc_install, test_install_with_script);
    suite_add_tcase(s, tc_install);
    
    TCase *tc_deptrack = tcase_create("DependencyTracking");
    tcase_add_test(tc_deptrack, test_find_reverse_deps_basic);
    tcase_add_test(tc_deptrack, test_find_orphaned_packages_basic);
    tcase_add_test(tc_deptrack, test_find_orphans_none_when_dep_needed);
    tcase_add_test(tc_deptrack, test_find_reverse_deps_null_args);
    suite_add_tcase(s, tc_deptrack);
    
    return s;
}
