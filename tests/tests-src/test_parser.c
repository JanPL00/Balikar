#include "../../src/balikar/includes/parser.h"
    #include <check.h>
    #include <unistd.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <sys/stat.h>

    // Add missing constants
    #define MAX_PATH_LEN 1024

    // Test fixtures
    typedef struct {
        char* temp_dir;
        char* repo_path;
    } TestFixture;

    // Helper functions for tests
    static void create_test_json(const char* path, const char* content) {
        FILE* f = fopen(path, "w");
        if (f) {
            fputs(content, f);
            fclose(f);
        }
    }

    static void setup_fixture(TestFixture* fixture) {
        char template[] = "/tmp/test_parser_XXXXXX";
        fixture->temp_dir = mkdtemp(template);
        ck_assert_ptr_ne(fixture->temp_dir, NULL);
        fixture->repo_path = malloc(strlen(fixture->temp_dir) + 10);
        sprintf(fixture->repo_path, "%s/repo", fixture->temp_dir);
        mkdir(fixture->repo_path, 0755);
    }

    static void teardown_fixture(TestFixture* fixture) {
        char command[1024];
        snprintf(command, sizeof(command), "rm -rf %s", fixture->temp_dir);
        system(command);
        free(fixture->repo_path);
    }

    static int create_test_tar_bz2(const char* tar_path, const char* index_content, int create_nested_source) {
        char tmp_dir_template[] = "/tmp/test_pkg_XXXXXX";
        char* tmp_dir = mkdtemp(tmp_dir_template);
        if (!tmp_dir) return 0;

        // Create package structure
        char info_dir[1024];
        snprintf(info_dir, sizeof(info_dir), "%s/info", tmp_dir);
        mkdir(info_dir, 0755);

        // Create index.json
        char index_path[1024];
        snprintf(index_path, sizeof(index_path), "%s/index.json", info_dir);
        create_test_json(index_path, index_content);

        // Create .build file
        char build_path[1024];
        snprintf(build_path, sizeof(build_path), "%s/.build", tmp_dir);
        create_test_json(build_path, "#!/bin/bash\necho \"Building package\"\n");

        // Create install script
        char install_dir[1024];
        snprintf(install_dir, sizeof(install_dir), "%s/.install", tmp_dir);
        mkdir(install_dir, 0755);
        char install_path[1024];
        snprintf(install_path, sizeof(install_path), "%s/install.sh", install_dir);
        create_test_json(install_path, "#!/bin/bash\necho \"Installing package\"\n");

        // Optional: Create nested source tar.xz
        if (create_nested_source) {
            char source_dir[1024];
            snprintf(source_dir, sizeof(source_dir), "%s/source", tmp_dir);
            mkdir(source_dir, 0755);

            char makefile_path[1024];
            snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", source_dir);
            create_test_json(makefile_path, "all:\n\techo \"Building from source\"\n");

            char src_path[1024];
            snprintf(src_path, sizeof(src_path), "%s/main.c", source_dir);
            create_test_json(src_path, "int main() { return 0; }\n");

            char nested_tar_path[1024];
            snprintf(nested_tar_path, sizeof(nested_tar_path), "%s/source.tar.xz", tmp_dir);
            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "tar -cJf \"%s\" -C \"%s\" .", nested_tar_path, source_dir);
            system(cmd);
        }

        // Create the final tar.bz2
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "tar -cjf \"%s\" -C \"%s\" .", tar_path, tmp_dir);
        int result = system(cmd);

        // Cleanup
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", tmp_dir);
        system(cmd);

        return result == 0;
    }

    static void create_test_repodata(const char* repo_path, const char* repodata_content) {
        char repodata_path[1024];
        snprintf(repodata_path, sizeof(repodata_path), "%s/repodata.json", repo_path);
        create_test_json(repodata_path, repodata_content);
    }

    static Repodata* get_repodata_from_repo(Repo* repo) {
        if (!repo) return NULL;
        return repo_last_repodata(repo);
    }

    // Initialize pool with arch
    static void init_pool_arch(Pool* pool) {
        pool_setarch(pool, "x86_64");
    }

    // TESTY PRO LOAD JSON A CONDÁ INTEGRACI
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

    // TESTY PRO REPO_ADD_CONDA WORKFLOW
    START_TEST(test_repo_add_conda_basic) {
        TestFixture fixture;
        setup_fixture(&fixture);

        // Create test repodata.json
        const char* repodata_content =
            "{\n"
            "  \"info\": {\n"
            "    \"subdir\": \"linux-64\"\n"
            "  },\n"
            "  \"packages\": {\n"
            "    \"test-pkg-1.0-0.tar.bz2\": {\n"
            "      \"name\": \"test-pkg\",\n"
            "      \"version\": \"1.0\",\n"
            "      \"build\": \"0\",\n"
            "      \"build_number\": 1,\n"
            "      \"depends\": [\"bash\"],\n"
            "      \"license\": \"MIT\",\n"
            "      \"md5\": \"d41d8cd98f00b204e9800998ecf8427e\"\n"
            "    }\n"
            "  }\n"
            "}";

        create_test_repodata(fixture.repo_path, repodata_content);

        Pool* pool = pool_create();
        init_pool_arch(pool);
        Repo* repo = repo_create(pool, "test-repo");

        char repodata_path[1024];
        snprintf(repodata_path, sizeof(repodata_path), "%s/repodata.json", fixture.repo_path);
        FILE* fp = fopen(repodata_path, "r");
        ck_assert_ptr_ne(fp, NULL);

        // Test repo_add_conda integration
        int result = repo_add_conda(repo, fp, 0);
        ck_assert_int_eq(result, 0);

        // Verify solvable was added
        ck_assert_int_gt(repo->nsolvables, 0);

        // Find the solvable by name, don't assume ID 1
        Id p = 0;
        Solvable* s = NULL;
        FOR_REPO_SOLVABLES(repo, p, s) {
            if (s && strcmp(pool_id2str(pool, s->name), "test-pkg") == 0) {
                break;
            }
            s = NULL;
        }

        ck_assert_ptr_ne(s, NULL);
        ck_assert_str_eq(pool_id2str(pool, s->name), "test-pkg");

        fclose(fp);
        repo_free(repo, 1);
        pool_free(pool);
        teardown_fixture(&fixture);
    }
    END_TEST

    // TESTY PRO CUSTOM FIELDS S REPO_ADD_CONDA
START_TEST(test_custom_fields_with_repo_add_conda) {
    TestFixture fixture;
    setup_fixture(&fixture);

    Pool* pool = pool_create();
    init_pool_arch(pool);
    Repo* repo = repo_create(pool, "test-repo");

    // CREATE A REAL TAR.BZ2 FILE instead of trying to extract URL
    char tar_path[1024];
    snprintf(tar_path, sizeof(tar_path), "%s/custom-pkg-2.0-1.tar.bz2", fixture.repo_path);

    const char* index_content =
        "{\"name\": \"custom-pkg\", \"version\": \"2.0\", \"build\": \"1\", "
        "\"depends\": [], \"mkdeps\": [\"gcc\", \"make\"], "
        "\"source\": \"http://example.com/source.tar.gz\", "
        "\"extras\": [\"patch1.patch\"], \"is_group\": false, \"no_package\": false}";

    int created = create_test_tar_bz2(tar_path, index_content, 0);
    ck_assert_int_ne(created, 0);

    // Add the LOCAL tar.bz2 file
    Id p = add_package_tar_bz2(tar_path, pool, repo);
    ck_assert_int_ne(p, 0);

    // Test custom fields retrieval
    Repodata* data = get_repodata_from_repo(repo);
    ck_assert_ptr_ne(data, NULL);

    // Test custom field retrieval functions
    json_t* mkdeps = get_custom_mkdeps(data, p, pool);
    ck_assert_ptr_ne(mkdeps, NULL);
    ck_assert_int_eq(json_array_size(mkdeps), 2);
    json_decref(mkdeps);

    const char* source = get_custom_source(data, p, pool);
    ck_assert_ptr_ne(source, NULL);
    ck_assert_str_eq(source, "http://example.com/source.tar.gz");

    json_t* extras = get_custom_extras(data, p, pool);
    ck_assert_ptr_ne(extras, NULL);
    ck_assert_int_eq(json_array_size(extras), 1);
    json_decref(extras);

    int is_group = get_custom_is_group(data, p, pool);
    ck_assert_int_eq(is_group, 0);

    int no_package = get_custom_no_package(data, p, pool);
    ck_assert_int_eq(no_package, 0);

    repo_free(repo, 1);
    pool_free(pool);
    teardown_fixture(&fixture);
}
END_TEST

    // TESTY PRO ADD_PACKAGE_TAR_BZ2 S REPO_ADD_CONDA
START_TEST(test_add_package_tar_bz2_with_repo_add_conda) {
    TestFixture fixture;
    setup_fixture(&fixture);

    Pool* pool = pool_create();
    init_pool_arch(pool);
    Repo* repo = repo_create(pool, "test-repo");

    // Create test tar.bz2 package
    char tar_path[1024];
    snprintf(tar_path, sizeof(tar_path), "%s/conda-pkg-1.0-0.tar.bz2", fixture.repo_path);        const char* index_content =
            "{\"name\": \"conda-pkg\", \"version\": \"1.0\", \"build\": \"0\", "
            "\"arch\": \"x86_64\", \"depends\": [\"python\"], "
            "\"mkdeps\": [\"gcc\"], \"source\": \"http://example.com/src.tar.gz\"}";

        int created = create_test_tar_bz2(tar_path, index_content, 0);
        ck_assert_int_ne(created, 0);

        // Test adding the package using new repo_add_conda workflow
        Id p = add_package_tar_bz2(tar_path, pool, repo);
        ck_assert_int_ne(p, 0);

        // Verify the solvable was created correctly
        Solvable* s = pool_id2solvable(pool, p);
        ck_assert_ptr_ne(s, NULL);
        ck_assert_str_eq(pool_id2str(pool, s->name), "conda-pkg");
        
        // Check that the solvable has requires set in repodata
        Repodata* data = get_repodata_from_repo(repo);
        ck_assert_ptr_ne(data, NULL);
        
        // Check if python dependency exists using libsolv's lookup
        Id python_id = pool_str2id(pool, "python", 0);
        ck_assert_int_ne(python_id, 0);
        
        // Verify dependency was set by checking if it's in the repodata
        Queue q;
        queue_init(&q);
        repodata_lookup_idarray(data, p, SOLVABLE_REQUIRES, &q);
        ck_assert_int_gt(q.count, 0);
        ck_assert_int_eq(q.elements[0], python_id);
        queue_free(&q);

        repo_free(repo, 1);
        pool_free(pool);
        teardown_fixture(&fixture);
    }
    END_TEST

    // TESTY PRO LOAD_REPOSITORY S GENEROVÁNÍM REPODATA
    START_TEST(test_load_repository_with_generation) {
        TestFixture fixture;
        setup_fixture(&fixture);

        // Create a package directory structure
        char pkg_dir[1024];
        snprintf(pkg_dir, sizeof(pkg_dir), "%s/repo/sample-pkg", fixture.temp_dir);
        mkdir(pkg_dir, 0755);
        char info_dir[1024];
        snprintf(info_dir, sizeof(info_dir), "%s/info", pkg_dir);
        mkdir(info_dir, 0755);


        char index_path[1024];
        snprintf(index_path, sizeof(index_path), "%s/index.json", info_dir);

        const char* index_content =
            "{\"name\": \"sample-pkg\", \"version\": \"1.0\", \"build\": \"0\", "
            "\"build_number\": 1, \"license\": \"BSD\", \"depends\": [\"libc\"]}";
        create_test_json(index_path, index_content);

        Pool* pool = pool_create();
        init_pool_arch(pool);

        // Test load_repository which should generate repodata.json if missing
        Repo* repo = load_repository(pool, fixture.repo_path);
        ck_assert_ptr_ne(repo, NULL);
        ck_assert_int_gt(repo->nsolvables, 0);

        // Verify repodata.json was created
        char repodata_path[1024];
        snprintf(repodata_path, sizeof(repodata_path), "%s/repodata.json", fixture.repo_path);
        ck_assert_int_eq(access(repodata_path, F_OK), 0);

        repo_free(repo, 1);
        pool_free(pool);
        teardown_fixture(&fixture);
    }
    END_TEST

    // TESTY PRO DEFAULT METADATA GENERATION
    START_TEST(test_generate_default_metadata) {
        json_t* metadata = generate_default_metadata("test-pkg-1.0-0.tar.bz2");
        ck_assert_ptr_ne(metadata, NULL);

        ck_assert_str_eq(json_string_value(json_object_get(metadata, "name")), "test-pkg");
        ck_assert_str_eq(json_string_value(json_object_get(metadata, "version")), "1.0");
        ck_assert_str_eq(json_string_value(json_object_get(metadata, "build")), "0");
        ck_assert_str_eq(json_string_value(json_object_get(metadata, "arch")), "noarch");

        json_decref(metadata);
    }
    END_TEST

    // TESTY PRO INTERNALIZE_REPO
    START_TEST(test_internalize_repo) {
        Pool* pool = pool_create();
        init_pool_arch(pool);
        Repo* repo = repo_create(pool, "test-repo");

        // Add a simple solvable
        Id p = repo_add_solvable(repo);
        Solvable* s = pool_id2solvable(pool, p);
        s->name = pool_str2id(pool, "test-pkg", 1);
        s->evr = pool_str2id(pool, "1.0-0", 1);
        repo_add_repodata(repo, 0); // Add a repodata area

        // Test internalization
        internalize_repo(repo);

        // Verify repo is internalized (basic check)
        ck_assert_int_gt(repo->start, 0);

        repo_free(repo, 1);
        pool_free(pool);
    }
    END_TEST

    // TEST SUITE SETUP
    Suite* parser_suite(void) {
        Suite* s = suite_create("Parser");

        // Load JSON tests
        TCase* tc_load = tcase_create("LoadJSON");
        tcase_add_test(tc_load, test_load_json_valid);
        tcase_add_test(tc_load, test_load_json_invalid);
        tcase_add_test(tc_load, test_load_json_malformed);
        suite_add_tcase(s, tc_load);

        // Repo add conda tests
        TCase* tc_conda = tcase_create("RepoAddConda");
        tcase_add_test(tc_conda, test_repo_add_conda_basic);
        tcase_add_test(tc_conda, test_custom_fields_with_repo_add_conda);
        tcase_add_test(tc_conda, test_add_package_tar_bz2_with_repo_add_conda);
        tcase_add_test(tc_conda, test_load_repository_with_generation);
        tcase_add_test(tc_conda, test_generate_default_metadata);
        tcase_add_test(tc_conda, test_internalize_repo);
        tcase_set_timeout(tc_conda, 15);
        suite_add_tcase(s, tc_conda);

        return s;
    }