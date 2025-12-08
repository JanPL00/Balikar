//
// Created by placeja5 on 11/20/25.
//

#include <check.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../../src/balikar/includes/installer.h"
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
    int result = validate_chroot("/tmp/this_definitely_does_not_exist_12345");
    ck_assert_int_eq(result, 0);
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
    
    // Should be an object
    ck_assert(json_is_object(packages));
    
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
    
    return s;
}
