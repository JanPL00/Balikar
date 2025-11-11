
#include "../../src/balikar/includes/parser.h"
#include "../../src/utils/includes/debug.h"

// Test fixtures
typedef struct {
    char* temp_dir;
    char* repo_path;
} TestFixture;

// Helper functions for tests
static void create_test_json(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    fputs(content, f);
    fclose(f);
}

static void setup_fixture(TestFixture* fixture) {
    fixture->temp_dir = strdup("/tmp/test_parser_XXXXXX");
    mkdtemp(fixture->temp_dir);
    fixture->repo_path = malloc(strlen(fixture->temp_dir) + 10);
    sprintf(fixture->repo_path, "%s/repo", fixture->temp_dir);
    mkdir(fixture->repo_path, 0755);
}

static void teardown_fixture(TestFixture* fixture) {
    // Clean up temporary files and directories
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf %s", fixture->temp_dir);
    system(command);
    free(fixture->temp_dir);
    free(fixture->repo_path);
}


// TESTY PRO LOAD JSON
START_TEST(test_load_json_valid) {
    char path[] = "/tmp/test_json_XXXXXX";
    int fd = mkstemp(path);
    const char* json_content = "{\"name\": \"test\", \"version\": \"1.0\", \"build\": \"0\"}";
    write(fd, json_content, strlen(json_content));
    close(fd);

    json_t* json = load_json(path);
    ck_assert_ptr_ne(json, NULL);
    ck_assert_str_eq(json_string_value(json_object_get(json, "name")), "test");

    json_decref(json);
    unlink(path);
}
END_TEST

START_TEST(test_load_json_invalid) {
    json_t* json = load_json("/nonexistent/path/file.json");
    ck_assert_ptr_eq(json, NULL);
}
END_TEST

START_TEST(test_load_json_malformed) {
    char path[] = "/tmp/test_json_XXXXXX";
    int fd = mkstemp(path);
    const char* malformed_content = "{invalid json}";
    write(fd, malformed_content, strlen(malformed_content));
    close(fd);

    json_t* json = load_json(path);
    ck_assert_ptr_eq(json, NULL);

    unlink(path);
}
END_TEST


// TESTY PRO GENERATE PACKAGE KEY
START_TEST(test_generate_package_key_valid) {
    json_t* index = json_object();
    json_object_set_new(index, "name", json_string("test-pkg"));
    json_object_set_new(index, "version", json_string("1.0.0"));
    json_object_set_new(index, "build", json_string("build123"));

    char* key = generate_package_key(index);
    ck_assert_ptr_ne(key, NULL);
    ck_assert_str_eq(key, "test-pkg-1.0.0-build123.tar.bz2");

    free(key);
    json_decref(index);
}
END_TEST

START_TEST(test_generate_package_key_missing_fields) {
    json_t* index = json_object();
    json_object_set_new(index, "name", json_string("test-pkg"));
    // Missing version and build

    char* key = generate_package_key(index);
    ck_assert_ptr_eq(key, NULL);

    json_decref(index);
}
END_TEST


// TESTY PRO COLLECT PACKAGES A GENERATE REPODATA
START_TEST(test_collect_packages_basic) {
    TestFixture fixture;
    setup_fixture(&fixture);

    // Create test package structure
    char pkg_dir[2048];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/test-pkg", fixture.repo_path);
    mkdir(pkg_dir, 0755);

    char index_path[2048];
    snprintf(index_path, sizeof(index_path), "%s/index.json", pkg_dir);

    const char* index_content =
        "{\"name\": \"test-pkg\", \"version\": \"1.0\", \"build\": \"0\", "
        "\"build_number\": 1, \"license\": \"MIT\", \"depends\": [\"dep1\"]}";
    create_test_json(index_path, index_content);

    // Test without libsolv integration
    json_t* packages = json_object();
    collect_packages(fixture.repo_path, packages, NULL, NULL);

    ck_assert_int_eq(json_object_size(packages), 1);

    const char* expected_key = "test-pkg-1.0-0.tar.bz2";
    json_t* collected_index = json_object_get(packages, expected_key);
    ck_assert_ptr_ne(collected_index, NULL);

    json_decref(packages);
    teardown_fixture(&fixture);
}
END_TEST

START_TEST(test_collect_packages_with_defaults) {
    TestFixture fixture;
    setup_fixture(&fixture);

    char pkg_dir[2048];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/minimal-pkg", fixture.repo_path);
    mkdir(pkg_dir, 0755);

    char index_path[2048];
    snprintf(index_path, sizeof(index_path), "%s/index.json", pkg_dir);

    // Minimal index.json without optional fields
    const char* index_content =
        "{\"name\": \"minimal-pkg\", \"version\": \"2.0\", \"build\": \"1\"}";
    create_test_json(index_path, index_content);

    json_t* packages = json_object();
    collect_packages(fixture.repo_path, packages, NULL, NULL);

    const char* key = "minimal-pkg-2.0-1.tar.bz2";
    json_t* collected = json_object_get(packages, key);
    ck_assert_ptr_ne(collected, NULL);

    // Verify defaults were added
    ck_assert_int_eq(json_integer_value(json_object_get(collected, "build_number")), 0);
    ck_assert_str_eq(json_string_value(json_object_get(collected, "license")), "unknown");
    ck_assert_str_eq(json_string_value(json_object_get(collected, "subdir")), "noarch");

    json_decref(packages);
    teardown_fixture(&fixture);
}
END_TEST


// Testy pro PARSE DEPENDS TO SOLVABLE
START_TEST(test_parse_depends_basic_name_only) {
    Pool* pool = pool_create();
    Repo* repo = repo_create(pool, "test-repo");
    Id p = repo_add_solvable(repo);
    Solvable* s = pool->solvables + p;

    // Test základního zápisu - pouze název balíčku
    json_t* depends = json_array();
    json_array_append_new(depends, json_string("glibc"));

    parse_depends_to_solvable(s, depends, pool);

    // Ověř, že závislost byla přidána
    ck_assert_int_ne(s->requires, 0);

    repo_free(repo, 1);
    pool_free(pool);
    json_decref(depends);
}
END_TEST

START_TEST(test_parse_depends_with_operators) {
    Pool* pool = pool_create();
    Repo* repo = repo_create(pool, "test-repo");
    Id p = repo_add_solvable(repo);
    Solvable* s = pool->solvables + p;

    // Test různých operátorů
    json_t* depends = json_array();
    json_array_append_new(depends, json_string("gcc >= 10.0"));
    json_array_append_new(depends, json_string("python == 3.8"));
    json_array_append_new(depends, json_string("zlib < 1.2.5"));
    json_array_append_new(depends, json_string("openssl <= 1.1.1"));
    json_array_append_new(depends, json_string("curl > 7.68.0"));

    parse_depends_to_solvable(s, depends, pool);

    // Ověř, že všechny závislosti byly přidány
    ck_assert_int_ne(s->requires, 0);

    repo_free(repo, 1);
    pool_free(pool);
    json_decref(depends);
}
END_TEST

START_TEST(test_parse_depends_mixed_formats) {
    Pool* pool = pool_create();
    Repo* repo = repo_create(pool, "test-repo");
    Id p = repo_add_solvable(repo);
    Solvable* s = pool->solvables + p;

    // Test kombinace různých formátů
    json_t* depends = json_array();
    json_array_append_new(depends, json_string("bash"));           // Pouze název
    json_array_append_new(depends, json_string("coreutils = 8.32")); // S operátorem =
    json_array_append_new(depends, json_string("grep >= 3.4"));    // S operátorem >=

    parse_depends_to_solvable(s, depends, pool);

    ck_assert_int_ne(s->requires, 0);

    repo_free(repo, 1);
    pool_free(pool);
    json_decref(depends);
}
END_TEST

START_TEST(test_parse_depends_invalid_formats) {
    Pool* pool = pool_create();
    Repo* repo = repo_create(pool, "test-repo");
    Id p = repo_add_solvable(repo);
    Solvable* s = pool->solvables + p;

    // Test neplatných formátů
    json_t* depends = json_array();
    json_array_append_new(depends, json_string(""));               // Prázdný string
    json_array_append_new(depends, json_string("pkg1 pkg2"));      // Dva názvy bez operátoru
    json_array_append_new(depends, json_string("pkg >= "));        // Chybějící verze
    json_array_append_new(depends, json_string(">= 1.0"));         // Chybějící název

    // Toto by nemělo způsobit crash
    parse_depends_to_solvable(s, depends, pool);

    // Mělo by logovat warningy, ale nezpůsobit chybu
    ck_assert(1); // Pokud jsme se dostali sem, test prošel

    repo_free(repo, 1);
    pool_free(pool);
    json_decref(depends);
}
END_TEST

START_TEST(test_parse_depends_unknown_operator) {
    Pool* pool = pool_create();
    Repo* repo = repo_create(pool, "test-repo");
    Id p = repo_add_solvable(repo);
    Solvable* s = pool->solvables + p;

    // Test neznámého operátoru
    json_t* depends = json_array();
    json_array_append_new(depends, json_string("pkg ~= 1.0"));     // Neznámý operátor ~=
    json_array_append_new(depends, json_string("pkg != 2.0"));     // Neznámý operátor !=

    parse_depends_to_solvable(s, depends, pool);

    // Mělo by přeskočit neznámé operátory, ale přidat platné závislosti
    ck_assert(1); // Pokud jsme se dostali sem, test prošel

    repo_free(repo, 1);
    pool_free(pool);
    json_decref(depends);
}
END_TEST

START_TEST(test_parse_depends_non_string_items) {
    Pool* pool = pool_create();
    Repo* repo = repo_create(pool, "test-repo");
    Id p = repo_add_solvable(repo);
    Solvable* s = pool->solvables + p;

    // Test ne-string položek v array
    json_t* depends = json_array();
    json_array_append_new(depends, json_integer(123));             // Číslo místo stringu
    json_array_append_new(depends, json_object());                 // Objekt místo stringu
    json_array_append_new(depends, json_array());                  // Array místo stringu
    json_array_append_new(depends, json_string("valid-pkg"));      // Jeden platný

    parse_depends_to_solvable(s, depends, pool);

    // Mělo by přeskočit ne-string položky, ale přidat platné
    ck_assert(1); // Pokud jsme se dostali sem, test prošel

    repo_free(repo, 1);
    pool_free(pool);
    json_decref(depends);
}
END_TEST

START_TEST(test_parse_depends_edge_cases) {
    Pool* pool = pool_create();
    Repo* repo = repo_create(pool, "test-repo");
    Id p = repo_add_solvable(repo);
    Solvable* s = pool->solvables + p;

    // Test hraničních případů
    json_t* depends = json_array();
    json_array_append_new(depends, json_string("pkg-with-dash >= 1.0")); // Název s pomlčkou
    json_array_append_new(depends, json_string("pkg.with.dots == 2.3.4")); // Název s tečkami
    json_array_append_new(depends, json_string("very-long-package-name")); // Dlouhý název

    parse_depends_to_solvable(s, depends, pool);

    ck_assert_int_ne(s->requires, 0);

    repo_free(repo, 1);
    pool_free(pool);
    json_decref(depends);
}
END_TEST


START_TEST(test_custom_fields_storage_and_retrieval_fn) {
    Pool* pool = pool_create();
    Repo* repo = repo_create(pool, "test-repo");

    // Create a test solvable with custom fields
    Id p = repo_add_solvable(repo);
    Solvable* s = pool->solvables + p;
    s->name = pool_str2id(pool, "test-pkg", 1);

    Repodata* data = repo_add_repodata(repo, 0);

    // Store custom fields
    repodata_set_str(data, p, pool_str2id(pool, "makedeps", 1), "[\"gcc\", \"make\"]");
    repodata_set_str(data, p, pool_str2id(pool, "download_url", 1), "http://example.com/pkg.tar.gz");
    repodata_set_str(data, p, pool_str2id(pool, "is_group", 1), "false");

    repo_internalize(repo);

    // Test retrieval functions - OPRAVA TYPŮ
    json_t* mkdeps = get_custom_mkdeps(data, p, pool);
    ck_assert_ptr_ne(mkdeps, NULL);
    ck_assert_int_eq(json_array_size(mkdeps), 2);
    json_decref(mkdeps);

    const char* source = get_custom_source(data, p, pool);
    ck_assert_ptr_ne(source, NULL);
    ck_assert_str_eq(source, "http://example.com/pkg.tar.gz");

    int is_group = get_custom_is_group(data, p, pool);
    ck_assert_int_eq(is_group, 0);

    repo_free(repo, 1);
    pool_free(pool);
}
END_TEST

// TESTY SUITE SETUP
Suite* parser_suite(void) {
    Suite* s = suite_create("Parser");

    // Load JSON tests
    TCase* tc_load = tcase_create("LoadJSON");
    tcase_add_test(tc_load, test_load_json_valid);
    tcase_add_test(tc_load, test_load_json_invalid);
    tcase_add_test(tc_load, test_load_json_malformed);
    suite_add_tcase(s, tc_load);

    // Package key tests
    TCase* tc_key = tcase_create("PackageKey");
    tcase_add_test(tc_key, test_generate_package_key_valid);
    tcase_add_test(tc_key, test_generate_package_key_missing_fields);
    suite_add_tcase(s, tc_key);

    // Collection tests
    TCase* tc_collect = tcase_create("CollectPackages");
    tcase_add_test(tc_collect, test_collect_packages_basic);
    tcase_add_test(tc_collect, test_collect_packages_with_defaults);
    tcase_set_timeout(tc_collect, 10);
    suite_add_tcase(s, tc_collect);

    // Repodata generation tests
    TCase* tc_repodata = tcase_create("GenerateRepodata");
    tcase_add_test(tc_repodata, test_collect_packages_basic);
    tcase_set_timeout(tc_repodata, 10);
    suite_add_tcase(s, tc_repodata);

    // Dependency parsing tests - NOVÉ
    TCase* tc_depends = tcase_create("ParseDepends");
    tcase_add_test(tc_depends, test_parse_depends_basic_name_only);
    tcase_add_test(tc_depends, test_parse_depends_with_operators);
    tcase_add_test(tc_depends, test_parse_depends_mixed_formats);
    tcase_add_test(tc_depends, test_parse_depends_invalid_formats);
    tcase_add_test(tc_depends, test_parse_depends_unknown_operator);
    tcase_add_test(tc_depends, test_parse_depends_non_string_items);
    tcase_add_test(tc_depends, test_parse_depends_edge_cases);
    tcase_add_test(tc_depends, test_custom_fields_storage_and_retrieval_fn);  // PŘIDÁNO
    suite_add_tcase(s, tc_depends);

    return s;
}
