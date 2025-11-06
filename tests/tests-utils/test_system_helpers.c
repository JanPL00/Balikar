//
// test_system_helpers.c - Unit testy pro system_helpers.c (pomocí Check framework)
//

#include "../../src/utils/includes/system_helpers.h"
#include "../../src/common/common.h"
#include <check.h>
#include <unistd.h>
#include <sys/utsname.h>

// ========================================
// MOCK FUNCTIONS (using --wrap)
// ========================================

static int mock_uname_fail = 0;

// Wrap function for uname - this will be called instead of real uname when linked with --wrap=uname
int __wrap_uname(struct utsname *buf) {
    if (mock_uname_fail) {
        return -1;  // Simulate error
    }
    // Normal: Fake "x86_64"
    strcpy(buf->sysname, "Linux");
    strcpy(buf->nodename, "test");
    strcpy(buf->release, "5.10.0");
    strcpy(buf->version, "#1 SMP");
    strcpy(buf->machine, "x86_64");
    return 0;
}

// ========================================
// HELPERS FOR MOCKING LOGGING
// ========================================

static char output_buffer[1024];
static int log_info_called = 0;
static char log_info_msg[256];
static int log_warn_called = 0;
static char log_warn_msg[256];
static int log_error_called = 0;
static char log_error_msg[256];

static void reset_mocks(void) {
    log_info_called = 0;
    log_warn_called = 0;
    log_error_called = 0;
    memset(log_info_msg, 0, sizeof(log_info_msg));
    memset(log_warn_msg, 0, sizeof(log_warn_msg));
    memset(log_error_msg, 0, sizeof(log_error_msg));
    memset(output_buffer, 0, sizeof(output_buffer));
}

// Mock logging functions
void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_info_msg, sizeof(log_info_msg), fmt, args);
    va_end(args);
    log_info_called++;
    printf("[INFO] %s\n", log_info_msg);  // Also print for visibility
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_warn_msg, sizeof(log_warn_msg), fmt, args);
    va_end(args);
    log_warn_called++;
    printf("[WARN] %s\n", log_warn_msg);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_error_msg, sizeof(log_error_msg), fmt, args);
    va_end(args);
    log_error_called++;
    printf("[ERROR] %s\n", log_error_msg);
    // Don't exit in tests
}

void log_debug(int level, const char *fmt, ...) {
    // Mock implementation if needed
    printf("[DEBUG %d] ", level);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

// Capture stdout to buffer
static void capture_output(void (*func)(Pool *), Pool *arg) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        ck_assert_msg(0, "Failed to create pipe for output capture");
        return;
    }
    fflush(stdout);
    int old_stdout = dup(STDOUT_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    func(arg);

    fflush(stdout);
    dup2(old_stdout, STDOUT_FILENO);
    ssize_t len = read(pipefd[0], output_buffer, sizeof(output_buffer) - 1);
    if (len >= 0) output_buffer[len] = '\0';
    close(pipefd[0]);
}

// ========================================
// TEST CASES
// ========================================

START_TEST(test_set_system_arch_normal) {
    Pool *pool = pool_create();
    ck_assert_ptr_nonnull(pool);

    // Reset mocks
    mock_uname_fail = 0;
    log_info_called = 0;
    log_warn_called = 0;
    log_error_called = 0;
    memset(output_buffer, 0, sizeof(output_buffer));

    capture_output(set_system_arch, pool);

    const char *arch = get_pool_arch(pool);
    ck_assert_ptr_nonnull(arch);  // First check it's not NULL
    ck_assert_str_eq(arch, "x86_64");

    // Check logging
    ck_assert_int_gt(log_info_called, 0);
    ck_assert_int_eq(log_warn_called, 0);
    ck_assert_int_eq(log_error_called, 0);

    pool_free(pool);
}
END_TEST

START_TEST(test_set_system_arch_fail) {
    Pool *pool = pool_create();
    ck_assert_ptr_nonnull(pool);

    // Setup for failure
    mock_uname_fail = 1;
    reset_mocks();
    memset(output_buffer, 0, sizeof(output_buffer));


    capture_output(set_system_arch, pool);

    const char *arch = get_pool_arch(pool);
    ck_assert_ptr_nonnull(arch);
    ck_assert_str_eq(arch, "x86_64");

    // Check logging - should have warning for fallback
    ck_assert_int_gt(log_warn_called, 0);  // Toto selhává
    ck_assert(strstr(log_warn_msg, "Falling back") != NULL);

    pool_free(pool);
}
END_TEST

START_TEST(test_set_system_arch_invalid_pool) {
    // Reset mocks
    log_error_called = 0;
    memset(output_buffer, 0, sizeof(output_buffer));

    capture_output(set_system_arch, NULL);

    // Should call log_error for invalid pool
    ck_assert_int_eq(log_error_called, 1);
    ck_assert(strstr(log_error_msg, "Invalid pool") != NULL);
}
END_TEST

// ========================================
// SUITE SETUP
// ========================================

Suite *system_helpers_suite(void) {
    Suite *s = suite_create("System Helpers");
    TCase *tc_arch = tcase_create("Set System Arch Tests");

    tcase_add_test(tc_arch, test_set_system_arch_normal);
    tcase_add_test(tc_arch, test_set_system_arch_fail);
    tcase_add_test(tc_arch, test_set_system_arch_invalid_pool);

    suite_add_tcase(s, tc_arch);
    return s;
}