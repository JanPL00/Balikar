//
// Created by placeja5 on 11/11/25.
//

//
// Testy pro parser.c (Check framework)
//

#include "../../src/balikar/includes/parser.h"

// Použij tvůj existující dir
#define TEST_REPO_DIR "../../../../../../tmp/test_repo"

// Setup: Check jestli dir existuje (nepřepisujeme, jen kontrola)
void setup(void) {
    if (access(TEST_REPO_DIR, F_OK) != 0) {
        fail("Test repo %s does not exist - create it first", TEST_REPO_DIR);
    }
    // Optional: Check specifické soubory
    if (access(TEST_REPO_DIR "/g/glibc/index.json", F_OK) != 0) {
        fail("Missing glibc/index.json");
    }
    if (access(TEST_REPO_DIR "/g/gzip/index.json", F_OK) != 0) {
        fail("Missing gzip/index.json");
    }
}

// Teardown: Optional remove (odeber pokud nechceš mazat)
void teardown(void) {
    // system("rm -rf " TEST_REPO_DIR);  // Odeber pro čistotu, ale zakomentuj pokud chceš zachovat
}

// Test load_json: Validní soubor (z tvého repo)
START_TEST(test_load_json_valid) {
    json_t *json = load_json(TEST_REPO_DIR "/g/glibc/index.json");
    ck_assert_ptr_nonnull(json);
    ck_assert_int_gt(json_object_size(json), 0);  // Alespoň nějaká data
    json_decref(json);
} END_TEST

// Test load_json: Neexistující soubor (error)
START_TEST(test_load_json_invalid) {
    json_t *json = load_json(TEST_REPO_DIR "/nonexistent.json");
    ck_assert_ptr_null(json);
} END_TEST

// Test generate_package_key: Validní (načti z tvého glibc.json)
START_TEST(test_generate_package_key_valid) {
    json_t *index = load_json(TEST_REPO_DIR "/g/glibc/index.json");
    ck_assert_ptr_nonnull(index);
    char *key = generate_package_key(index);
    ck_assert_ptr_nonnull(key);
    // Assert na očekávaný klíč z tvého JSON (uprav podle verze)
    ck_assert_str_eq(key, "glibc-2.38-hd3c86d7_0.tar.bz2");  // Předpoklad, uprav
    free(key);
    json_decref(index);
} END_TEST

// Test generate_package_key: Chybějící pole (mock prázdný)
START_TEST(test_generate_package_key_missing) {
    json_t *index = json_object();  // Prázdný pro test
    char *key = generate_package_key(index);
    ck_assert_ptr_null(key);
    json_decref(index);
} END_TEST

// Test parse_depends_to_solvable: Z tvého glibc depends
START_TEST(test_parse_depends_to_solvable) {
    Pool *pool = pool_create();
    Solvable s = {0};
    json_t *index = load_json(TEST_REPO_DIR "/g/glibc/index.json");
    json_t *depends = json_object_get(index, "depends");

    if (depends && json_is_array(depends)) {
        parse_depends_to_solvable(&s, depends, pool);
        // Správná kontrola - requires by nemělo být 0 pokud jsou závislosti
        ck_assert_int_gt(s.requires, 0);  // Odstranit [0]
    } else {
        fail("No depends in glibc.json");
    }

    json_decref(index);
    pool_free(pool);
} END_TEST

// Test parse_depends_to_solvable: Neplatný formát (mock)
START_TEST(test_parse_depends_to_solvable_invalid) {
    Pool *pool = pool_create();
    Solvable s = {0};
    json_t *depends = json_array();
    json_array_append_new(depends, json_string("pkg invalid 1.0"));

    parse_depends_to_solvable(&s, depends, pool);
    ck_assert_int_eq(s.requires, 0);  // No deps (skip) - odstranit [0]

    json_decref(depends);
    pool_free(pool);

} END_TEST

// Test collect_packages: Rekurze na tvém repo
START_TEST(test_collect_packages) {
    Pool *pool = pool_create();
    Repo *repo = repo_create(pool, "test_repo");
    json_t *packages = json_object();

    collect_packages(TEST_REPO_DIR, packages, pool, repo);

    ck_assert_int_gt(repo->nsolvables, 0);  // Alespoň tvé balíčky (uprav na přesný počet, např. 2)
    ck_assert_int_gt(json_object_size(packages), 0);

    // Check první Solvable (předpoklad glibc)
    Solvable *s = pool->solvables + repo->start;
    ck_assert_str_eq(pool_id2str(pool, s->name), "glibc");  // Uprav podle tvého
    ck_assert_str_eq(pool_id2str(pool, s->arch), "x86_64");

    json_decref(packages);
    repo_free(repo, 1);
    pool_free(pool);
} END_TEST

// Test collect_packages: Custom pole z tvého glibc
START_TEST(test_collect_packages_custom) {
    Pool *pool = pool_create();
    Repo *repo = repo_create(pool, "test_repo");
    json_t *packages = json_object();

    collect_packages(TEST_REPO_DIR, packages, pool, repo);

    Repodata *data = repo_last_repodata(repo);
    Id p = repo->start;  // První Solvable (glibc)

    json_t *mkdeps = get_custom_mkdeps(data, p, pool);
    ck_assert_ptr_nonnull(mkdeps);
    ck_assert_int_gt(json_array_size(mkdeps), 0);  // Alespoň nějaké mkdeps
    json_decref(mkdeps);

    ck_assert_ptr_nonnull(get_custom_source(data, p, pool));  // URL existuje

    ck_assert_int_eq(get_custom_is_group(data, p, pool), 1);  // Předpoklad true pro glibc, uprav

    json_decref(packages);
    repo_free(repo, 1);
    pool_free(pool);
} END_TEST

// Test generate_repodata: Na tvém repo
START_TEST(test_generate_repodata) {
    Pool *pool = pool_create();
    Repo *repo = repo_create(pool, "test_repo");

    int ret = generate_repodata(TEST_REPO_DIR, pool, repo);
    ck_assert_int_eq(ret, 0);

    // Check repodata.json generován
    ck_assert_int_eq(access(TEST_REPO_DIR "/repodata.json", F_OK), 0);

    repo_free(repo, 1);
    pool_free(pool);
} END_TEST

// Suite pro parser
Suite *parser_suite(void) {
    Suite *s = suite_create("Parser");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_load_json_valid);
    tcase_add_test(tc_core, test_load_json_invalid);
    tcase_add_test(tc_core, test_generate_package_key_valid);
    tcase_add_test(tc_core, test_generate_package_key_missing);
    tcase_add_test(tc_core, test_parse_depends_to_solvable);
    tcase_add_test(tc_core, test_parse_depends_to_solvable_invalid);
    tcase_add_test(tc_core, test_collect_packages);
    tcase_add_test(tc_core, test_collect_packages_custom);
    tcase_add_test(tc_core, test_generate_repodata);

    suite_add_tcase(s, tc_core);

    return s;
}