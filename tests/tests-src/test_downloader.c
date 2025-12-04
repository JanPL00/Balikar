// Unit tests for downloader.c
// Focus: local file download, cache reuse (no hash verification)

#include "../../src/balikar/includes/downloader.h"
#include <check.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Fixture: creates temp HOME and test file
typedef struct {
    char home_template[64];
    char *home_dir;
    char file_path[256];
    Pool *pool;
    Repo *repo;
    Repodata *data;
    Id solv_id;
    Solvable *solv;
} DownloaderFixture;

static void setup_fixture(DownloaderFixture *f) {
    strcpy(f->home_template, "/tmp/test_downloader_home_XXXXXX");
    f->home_dir = mkdtemp(f->home_template);
    ck_assert_ptr_ne(f->home_dir, NULL);
    // Override HOME so CACHE_DIR macro points inside this temp dir
    setenv("HOME", f->home_dir, 1);

    // Create test source file
    snprintf(f->file_path, sizeof(f->file_path), "%s/source.txt", f->home_dir);
    FILE *fp = fopen(f->file_path, "w");
    ck_assert_ptr_ne(fp, NULL);
    fputs("hello world", fp); // known content
    fclose(fp);

    f->pool = pool_create();
    ck_assert_ptr_ne(f->pool, NULL);
    pool_setarch(f->pool, "x86_64");
    f->repo = repo_create(f->pool, "testrepo");
    ck_assert_ptr_ne(f->repo, NULL);
    f->solv_id = repo_add_solvable(f->repo);
    f->solv = pool_id2solvable(f->pool, f->solv_id);
    f->solv->name = pool_str2id(f->pool, "testpkg", 1);
    f->solv->evr = pool_str2id(f->pool, "1.0", 1);
    f->data = repo_add_repodata(f->repo, 0);
    ck_assert_ptr_ne(f->data, NULL);
}

static void teardown_fixture(DownloaderFixture *f) {
    if (f->repo) repo_free(f->repo, 1);
    if (f->pool) pool_free(f->pool);
    if (f->home_dir) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", f->home_dir);
        system(cmd);
    }
}

// Test: successful local file download (no verification)
START_TEST(test_download_package_local_success) {
    DownloaderFixture fx; setup_fixture(&fx);

    FILE *res = download_package(fx.solv, fx.file_path, fx.pool, fx.data, NULL);
    ck_assert_ptr_ne(res, NULL);
    char buf[32] = {0};
    size_t r = fread(buf, 1, sizeof(buf)-1, res);
    fclose(res);
    ck_assert_int_gt(r, 0);
    ck_assert_str_eq(buf, "hello world");

    teardown_fixture(&fx);
}
END_TEST

// Test: cache reuse (second call should use cached file)
START_TEST(test_download_package_cache_reuse) {
    DownloaderFixture fx; setup_fixture(&fx);

    // First download
    FILE *res1 = download_package(fx.solv, fx.file_path, fx.pool, fx.data, NULL);
    ck_assert_ptr_ne(res1, NULL);
    fclose(res1);

    // Compose expected cache path
    char *cache_dir = CACHE_DIR; // allocated
    char expected[512];
    snprintf(expected, sizeof(expected), "%s/%s-%s.txt", cache_dir, "testpkg", "1.0");
    ck_assert_int_eq(access(expected, F_OK), 0);

    // Second download should use cached file (no re-copy)
    FILE *res2 = download_package(fx.solv, fx.file_path, fx.pool, fx.data, NULL);
    ck_assert_ptr_ne(res2, NULL);
    char buf[16] = {0};
    fread(buf, 1, sizeof(buf)-1, res2);
    fclose(res2);
    ck_assert_str_eq(buf, "hello world");

    free(cache_dir);
    teardown_fixture(&fx);
}
END_TEST

Suite *downloader_suite(void) {
    Suite *s = suite_create("Downloader");
    TCase *tc_local = tcase_create("LocalFile");
    tcase_add_test(tc_local, test_download_package_local_success);
    tcase_add_test(tc_local, test_download_package_cache_reuse);
    tcase_set_timeout(tc_local, 10);
    suite_add_tcase(s, tc_local);
    return s;
}
