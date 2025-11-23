//
// Created by placeja5 on 11/20/25.
//

#ifndef INSTALLER_H
#define INSTALLER_H

#include "../../common/common.h"


/**
 * Installs a package from staging directory to chroot environment.
 * 
 * Workflow:
 * 1. Validates staging directory exists and contains files
 * 2. Validates chroot directory exists
 * 3. Collects file manifest from staging
 * 4. Merges staging to chroot (cp -a)
 * 5. Executes .install/install.sh if exists (optional post-install hooks)
 * 6. Updates installed packages database
 * 
 * @param pkg_name Package name for tracking
 * @param pkg_version Package version string
 * @param staging_dir Path to staging directory (e.g., $P or /mnt/lfs/stage)
 * @param chroot_path Path to chroot environment (e.g., /mnt/lfs or /tmp/pm-chroot)
 * @return 0 on success, -1 on error
 */
int install_package(const char *pkg_name, const char *pkg_version,
                    const char *staging_dir, const char *chroot_path);

/**
 * Checks if a package is already installed.
 * Queries the installed packages database ({chroot}/var/lib/balikar/installed.json).
 * 
 * @param pkg_name Package name to check
 * @param chroot_path Root directory (defaults to "/" if NULL)
 * @return 1 if installed, 0 if not installed, -1 on error
 */
int is_package_installed(const char *pkg_name, const char *chroot_path);

/**
 * Retrieves list of all installed packages from database.
 * 
 * @param chroot_path Root directory (defaults to "/" if NULL)
 * @return JSON array of installed package objects, or NULL on error
 *         Caller must json_decref() the returned object
 */
json_t* get_installed_packages(const char *chroot_path);

/**
 * Removes (erases) an installed package from the system.
 * 
 * Workflow:
 * 1. Checks if package is installed
 * 2. Loads file manifest from database
 * 3. Removes files from chroot
 * 4. Updates database
 * 
 * @param pkg_name Package name to erase
 * @param chroot_path Path to chroot environment
 * @return 0 on success, -1 on error
 */
int erase_package(const char *pkg_name, const char *chroot_path);

/**
 * Validates chroot environment structure.
 * Checks for basic directories like /usr, /lib, /bin.
 * 
 * @param chroot_path Path to chroot environment
 * @return 1 if valid, 0 if suspicious/invalid
 */
int validate_chroot(const char *chroot_path);

#endif // INSTALLER_H
