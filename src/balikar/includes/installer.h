#ifndef INSTALLER_H
#define INSTALLER_H

#include "../../common/common.h"

int install_package(const char *pkg_name, const char *pkg_version,
                    const char *staging_dir, const char *chroot_path);

int install_package_with_reason(const char *pkg_name, const char *pkg_version,
                                const char *staging_dir, const char *chroot_path,
                                const char *install_reason);

int is_package_installed(const char *pkg_name, const char *chroot_path);

json_t* get_installed_packages(const char *chroot_path);

json_t* load_installed_db(const char *chroot_path);

int erase_package(const char *pkg_name, const char *chroot_path);

int validate_chroot(const char *chroot_path);

json_t* find_reverse_dependencies(const char *pkg_name, const char *chroot_path, Pool *pool, Repo *repo, json_t* (*load_db_cb)(const char*));

json_t* find_orphaned_packages(const char *chroot_path, Pool *pool, Repo *repo, json_t* (*load_db_cb)(const char*));

int autoremove_packages(const char *chroot_path, Pool *pool, Repo *repo, int dry_run, json_t* (*load_db_cb)(const char*), int (*erase_cb)(const char*, const char*));

int erase_package_with_checks(const char *pkg_name, const char *chroot_path, Pool *pool, Repo *repo, int show_orphans, json_t* (*load_db_cb)(const char*), int (*erase_cb)(const char*, const char*));

#endif
