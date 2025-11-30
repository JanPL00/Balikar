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
 * Installs a package with specified install reason (manual or dependency).
 * 
 * @param pkg_name Package name for tracking
 * @param pkg_version Package version string
 * @param staging_dir Path to staging directory
 * @param chroot_path Path to chroot environment
 * @param install_reason "manual" or "dependency"
 * @return 0 on success, -1 on error
 */
int install_package_with_reason(const char *pkg_name, const char *pkg_version,
                                const char *staging_dir, const char *chroot_path,
                                const char *install_reason);

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
 * Removes package with dependency checking and orphan detection.
 * 
 * @param pkg_name Package name to erase
 * @param chroot_path Path to chroot environment
 * @param pool Solver pool (can be NULL to skip dependency checks)
 * @param repo Repository (can be NULL to skip dependency checks)
 * @param show_orphans If true, show orphaned packages after removal
 * @return 0 on success, -1 on error
 */
int erase_package_with_checks(const char *pkg_name, const char *chroot_path,
                               Pool *pool, Repo *repo, int show_orphans);

/**
 * Validates chroot environment structure.
 * Checks for basic directories like /usr, /lib, /bin.
 * 
 * @param chroot_path Path to chroot environment
 * @return 1 if valid, 0 if suspicious/invalid
 */
int validate_chroot(const char *chroot_path);

/**
 * Finds packages that depend on the given package.
 * Returns array of package names that have pkg_name in their dependencies.
 * 
 * @param pkg_name Package name to check
 * @param chroot_path Root directory
 * @param pool Solver pool with dependency information
 * @param repo Repository with installed packages
 * @return JSON array of dependent package names, or NULL if none/error
 */
json_t* find_reverse_dependencies(const char *pkg_name, const char *chroot_path,
                                  Pool *pool, Repo *repo);

/**
 * Finds orphaned packages (dependencies no longer needed).
 * Returns packages with install_reason="dependency" that are not required by any installed package.
 * 
 * @param chroot_path Root directory
 * @param pool Solver pool with dependency information
 * @param repo Repository with installed packages
 * @return JSON array of orphaned package names, or NULL if none/error
 */
json_t* find_orphaned_packages(const char *chroot_path, Pool *pool, Repo *repo);

/**
 * Removes all orphaned packages from the system.
 * Removes packages with install_reason="dependency" that are no longer needed.
 * 
 * @param chroot_path Root directory
 * @param pool Solver pool with dependency information
 * @param repo Repository with installed packages
 * @param dry_run If true, only show what would be removed
 * @return Number of packages removed, or -1 on error
 */
int autoremove_packages(const char *chroot_path, Pool *pool, Repo *repo, int dry_run);

#endif // INSTALLER_H
