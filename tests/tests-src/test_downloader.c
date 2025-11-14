// Unit tests for downloader.c
// Focus: local file download, sha256 verification, cache reuse, mismatch handling

#include "../../src/balikar/includes/downloader.h"
#include "../../src/utils/includes/debug.h"
#include <check.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Helper: compute sha256 hex string of a file
static char *compute_sha256(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        SHA256_Update(&ctx, buffer, n);
    }
    fclose(fp);
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);
    char *hex = malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    if (!hex) return NULL;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    hex[SHA256_DIGEST_LENGTH * 2] = '\0';
    return hex;
}

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

// Test: successful local file download with matching sha
// Note: In production, SHA is auto-computed by parser for tar files
START_TEST(test_download_package_local_sha_match) {
    DownloaderFixture fx; setup_fixture(&fx);
    char *sha = compute_sha256(fx.file_path);
    ck_assert_ptr_ne(sha, NULL);

    Id sha_key = pool_str2id(fx.pool, "sha256", 1);
    repodata_set_str(fx.data, fx.solv_id, sha_key, sha);
    repodata_internalize(fx.data);

    FILE *res = download_package(fx.solv, fx.file_path, fx.pool, fx.data);
    ck_assert_ptr_ne(res, NULL);
    char buf[32] = {0};
    size_t r = fread(buf, 1, sizeof(buf)-1, res);
    fclose(res);
    ck_assert_int_gt(r, 0);
    ck_assert_str_eq(buf, "hello world");

    free(sha);
    teardown_fixture(&fx);
}
END_TEST

// Test: sha mismatch should fail and return NULL
START_TEST(test_download_package_local_sha_mismatch) {
    DownloaderFixture fx; setup_fixture(&fx);
    // Set wrong sha (all zeros)
    char wrong_sha[SHA256_DIGEST_LENGTH * 2 + 1];
    memset(wrong_sha, '0', sizeof(wrong_sha)-1);
    wrong_sha[sizeof(wrong_sha)-1] = '\0';
    Id sha_key = pool_str2id(fx.pool, "sha256", 1);
    repodata_set_str(fx.data, fx.solv_id, sha_key, wrong_sha);
    repodata_internalize(fx.data);

    FILE *res = download_package(fx.solv, fx.file_path, fx.pool, fx.data);
    ck_assert_ptr_eq(res, NULL);

    teardown_fixture(&fx);
}
END_TEST

// Test: cache reuse (second call should not re-copy)
// Note: Remote downloads also create .sha256 sidecar files for validation
START_TEST(test_download_package_cache_reuse) {
    DownloaderFixture fx; setup_fixture(&fx);
    char *sha = compute_sha256(fx.file_path);
    ck_assert_ptr_ne(sha, NULL);
    Id sha_key = pool_str2id(fx.pool, "sha256", 1);
    repodata_set_str(fx.data, fx.solv_id, sha_key, sha);
    repodata_internalize(fx.data);

    // First download
    FILE *res1 = download_package(fx.solv, fx.file_path, fx.pool, fx.data);
    ck_assert_ptr_ne(res1, NULL);
    fclose(res1);

    // Compose expected cache path
    char *cache_dir = CACHE_DIR; // allocated
    char expected[512];
    snprintf(expected, sizeof(expected), "%s/%s-%s.txt", cache_dir, "testpkg", "1.0");
    ck_assert_int_eq(access(expected, F_OK), 0);

    // Second download should use cached file
    FILE *res2 = download_package(fx.solv, fx.file_path, fx.pool, fx.data);
    ck_assert_ptr_ne(res2, NULL);
    char buf[16] = {0};
    fread(buf, 1, sizeof(buf)-1, res2);
    fclose(res2);
    ck_assert_str_eq(buf, "hello world");

    free(cache_dir);
    free(sha);
    teardown_fixture(&fx);
}
END_TEST

// New: Remote file URL case (file://) without metadata SHA, expect sidecar
START_TEST(test_download_package_remote_file_url_sidecar) {
    DownloaderFixture fx; setup_fixture(&fx);

    // Prepare a source file and build file:// URL
    char url_buf[512];
    snprintf(url_buf, sizeof(url_buf), "file://%s", fx.file_path);

    FILE *res = download_package(fx.solv, url_buf, fx.pool, fx.data);
    ck_assert_ptr_ne(res, NULL);
    char buf[32] = {0};
    size_t r = fread(buf, 1, sizeof(buf)-1, res);
    fclose(res);
    ck_assert_int_gt(r, 0);
    ck_assert_str_eq(buf, "hello world");

    // Verify sidecar exists with 64-char SHA
    char *cache_dir = CACHE_DIR;
    const char *ext = strrchr(url_buf, '.') ? strrchr(url_buf, '.') : ".tar.xz";
    char cache_path[512];
    snprintf(cache_path, sizeof(cache_path), "%s/%s-%s%s", cache_dir, pool_id2str(fx.pool, fx.solv->name), pool_id2str(fx.pool, fx.solv->evr), ext);
    char sidecar[576];
    snprintf(sidecar, sizeof(sidecar), "%s.sha256", cache_path);
    ck_assert_int_eq(access(sidecar, F_OK), 0);
    FILE *sf = fopen(sidecar, "r");
    ck_assert_ptr_ne(sf, NULL);
    char sha_read[128] = {0};
    fgets(sha_read, sizeof(sha_read), sf);
    fclose(sf);
    // trim newline if present
    size_t len = strlen(sha_read);
    if (len && sha_read[len-1] == '\n') sha_read[len-1] = '\0';
    ck_assert_int_eq(strlen(sha_read), 64);

    free(cache_dir);
    teardown_fixture(&fx);
}
END_TEST

Suite *downloader_suite(void) {
    Suite *s = suite_create("Downloader");
    TCase *tc_local = tcase_create("LocalFile");
    tcase_add_test(tc_local, test_download_package_local_sha_match);
    tcase_add_test(tc_local, test_download_package_local_sha_mismatch);
    tcase_add_test(tc_local, test_download_package_cache_reuse);
    tcase_set_timeout(tc_local, 10);
    suite_add_tcase(s, tc_local);

    TCase *tc_remote = tcase_create("RemoteFile");
    tcase_add_test(tc_remote, test_download_package_remote_file_url_sidecar);
    tcase_set_timeout(tc_remote, 10);
    suite_add_tcase(s, tc_remote);
    return s;
}
