//
// Created by placeja5 on 11/14/25.
//

#include "includes/builder.h"
#include <sys/wait.h>  // Pro waitpid

// Lokální stav
static int g_staging_enabled = 1;  // default zapnuto
static char *g_staging_dir = NULL;  // NULL = použít STAGING_DIR z common.h
static char *g_build_tmp_dir = NULL;  // NULL = použít /tmp

void set_staging_enabled(int enabled) {
    g_staging_enabled = enabled ? 1 : 0;
}

void set_staging_dir(const char *dir) {
    if (g_staging_dir) free(g_staging_dir);
    g_staging_dir = dir ? strdup(dir) : NULL;
}

void set_build_tmp_dir(const char *dir) {
    if (g_build_tmp_dir) free(g_build_tmp_dir);
    g_build_tmp_dir = dir ? strdup(dir) : NULL;
}// Detekce komprese podle magic
static int detect_compression(FILE *fp) {
	unsigned char buf[6];
	size_t r = fread(buf, 1, sizeof(buf), fp);
	rewind(fp);
	if (r < 2) return 0;
	// xz magic FD 37 7A 58 5A 00
	if (r >= 6 && buf[0] == 0xFD && buf[1] == 0x37 && buf[2] == 0x7A && buf[3] == 0x58 && buf[4] == 0x5A && buf[5] == 0x00) return 1; // xz
	// bzip2 magic 'BZ'
	if (buf[0] == 'B' && buf[1] == 'Z') return 2; // bz2
	return 0; // unknown / plain
}

// Helper: rekurzivně hledá .build nebo první subdir s obsahem
static char *find_build_or_source(const char *dir, int depth) {
	if (depth > 3) return NULL;  // limit rekurze
	
	// Zkus najít .build v aktuálním dir
	char build_path[MAX_PATH_LEN];
	snprintf(build_path, sizeof(build_path), "%s/.build", dir);
	if (access(build_path, F_OK) == 0) {
		return strdup(dir);
	}
	
	// Zkus najít Makefile nebo configure
	char make_path[MAX_PATH_LEN];
	snprintf(make_path, sizeof(make_path), "%s/Makefile", dir);
	char conf_path[MAX_PATH_LEN];
	snprintf(conf_path, sizeof(conf_path), "%s/configure", dir);
	if (access(make_path, F_OK) == 0 || access(conf_path, F_OK) == 0) {
		return strdup(dir);
	}
	
	// Zkus subdirectories
	DIR *d = opendir(dir);
	if (!d) return NULL;
	struct dirent *ent;
	char *result = NULL;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		char path[MAX_PATH_LEN];
		snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			result = find_build_or_source(path, depth + 1);
			if (result) break;
		}
	}
	closedir(d);
	return result;
}

char *find_source_path(const char *tmp_dir) {
	if (!tmp_dir) return NULL;
	
	// Nejdřív zkus rekurzivní hledání .build/Makefile
	char *found = find_build_or_source(tmp_dir, 0);
	if (found) return found;
	
	// Fallback: první subdirectory (původní logika)
	DIR *d = opendir(tmp_dir);
	if (!d) return strdup(tmp_dir);
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		char path[MAX_PATH_LEN];
		snprintf(path, sizeof(path), "%s/%s", tmp_dir, ent->d_name);
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			closedir(d);
			return strdup(path);
		}
	}
	closedir(d);
	return strdup(tmp_dir); // fallback root
}

void apply_extras(json_t *extras, const char *pkg_name, const char *source_dir) {
	if (!extras || !json_is_array(extras) || !pkg_name || !source_dir) return;
	char *cache_dir = get_cache_dir();
	if (!cache_dir) return;
	char patch_root[2048];
	snprintf(patch_root, sizeof(patch_root), "%s/extras/%s", cache_dir, pkg_name);
	DIR *d = opendir(patch_root);
	if (!d) { free(cache_dir); return; }
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		// Ověř zda je v JSON seznamu
		size_t i; json_t *item; int found = 0;
		json_array_foreach(extras, i, item) {
			if (json_is_string(item) && strstr(json_string_value(item), ent->d_name)) { found = 1; break; }
		}
		if (!found) continue;
		char patch_file[4096];
		snprintf(patch_file, sizeof(patch_file), "%s/%s", patch_root, ent->d_name);
		char cmd[8192];
		snprintf(cmd, sizeof(cmd), "cd '%s' && patch -p1 < '%s' >/dev/null 2>&1", source_dir, patch_file);
		int rc = system(cmd);
		if (rc != 0) {
			log_warn("Patch selhal: %s", patch_file);
		} else {
			log_info("Aplikován patch: %s", patch_file);
		}
	}
	closedir(d);
	free(cache_dir);
}

int run_with_chroot(const char *root, build_callback func, void *data) {
	if (!root || !func) return -1;
	
	pid_t pid = fork();
	if (pid < 0) {
		log_error("Fork selhal pro chroot izolaci");
		return -1;
	}
	
	if (pid == 0) {
		// Child: chroot a spusť callback
		if (chroot(root) != 0) {
			log_error("Chroot selhal: %s", strerror(errno));
			exit(1);
		}
		if (chdir("/") != 0) {
			log_error("Chdir po chroot selhal");
			exit(1);
		}
		int rc = func(data);
		exit(rc);
	}
	
	// Parent: čekej na child
	int status;
	if (waitpid(pid, &status, 0) < 0) {
		log_error("Waitpid selhal");
		return -1;
	}
	
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return -1;
}

static int extract_archive(FILE *fp, const char *tmp_dir) {
	if (!fp || !tmp_dir) return -1;
	rewind(fp);
	int comp = detect_compression(fp);
	const char *tarflag = "xf"; // plain
	if (comp == 1) tarflag = "xJf"; // xz
	else if (comp == 2) tarflag = "xjf"; // bz2
	char cmd[4096];
	snprintf(cmd, sizeof(cmd), "tar %s - -C '%s'", tarflag, tmp_dir);
	FILE *tar = popen(cmd, "w");
	if (!tar) return -1;
	unsigned char buf[8192];
	size_t r;
	while ((r = fread(buf, 1, sizeof(buf), fp)) > 0) {
		if (fwrite(buf, 1, r, tar) != r) {
			pclose(tar);
			return -1;
		}
	}
	int status = pclose(tar);
	if (status != 0) return -1;
	return 0;
}

int commit_element(Transaction *trans, Repodata *data, Id p, FILE *fp, Pool *pool) {
	(void)trans; // zatím nevyužito pro pořadí
	if (!data || !fp || !pool) return -1;

	// Metadata
	int is_group = get_custom_is_group(data, p, pool);
	int no_package = get_custom_no_package(data, p, pool);
	json_t *extras = get_custom_extras(data, p, pool);

	char *pkg_name = pool_id2str(pool, pool->solvables[p].name);
	if (no_package) {
		log_info("Skipping virtual/no_package: %s", pkg_name);
		if (fp) fclose(fp);  // Zavři fp i pro skip
		if (extras) json_decref(extras);
		return 0;
	}

	// mkdtemp dir v konfigurovatelném tmp
	const char *tmp_base = g_build_tmp_dir ? g_build_tmp_dir : "/tmp";
	char tmpl[512];
	snprintf(tmpl, sizeof(tmpl), "%s/balikar_build_%s_XXXXXX", tmp_base, pkg_name ? pkg_name : "pkg");
	char *build_dir = strdup(tmpl);
	if (!build_dir) return -1;
	if (!mkdtemp(build_dir)) {
		log_error("mkdtemp selhalo pro %s", pkg_name);
		free(build_dir);
		return -1;
	}
	log_info("Build dir: %s", build_dir);

	if (extract_archive(fp, build_dir) != 0) {
		log_error("Extrahování archivu selhalo pro %s", pkg_name);
		goto fail;
	}

	char *source_dir = find_source_path(build_dir);
	if (!source_dir) {
		log_error("Nenalezen source dir pro %s", pkg_name);
		goto fail;
	}
	log_info("Source dir: %s", source_dir);

	// Extras (patches)
	if (extras) apply_extras(extras, pkg_name, source_dir);

	// Staging env
	if (g_staging_enabled) {
		const char *destdir = g_staging_dir ? g_staging_dir : STAGING_DIR;
		setenv("DESTDIR", destdir, 1);
		log_debug(1, "DESTDIR nastaveno na: %s", destdir);
	} else {
		unsetenv("DESTDIR");
	}
	long nproc = sysconf(_SC_NPROCESSORS_ONLN);
	if (nproc < 1) nproc = 1;
	char mf[64]; snprintf(mf, sizeof(mf), "-j%ld", nproc);
	setenv("MAKEFLAGS", mf, 1);

	// is_group => pouze data (žádný build)
	if (is_group) {
		log_info("is_group: pouze extrakce pro %s", pkg_name);
		goto done;
	}

	// Build skript nebo fallback
	if (chdir(source_dir) != 0) {
		log_error("Nelze chdir do %s", source_dir);
		goto fail_chdir;
	}
	int have_build = access(".build", X_OK) == 0;
	const char *build_cmd = have_build ? "bash -e .build > build.log 2>&1" : "./configure --prefix=/usr >/dev/null 2>&1 && make >/dev/null 2>&1 && make install >/dev/null 2>&1";
	log_info("Spouštím build: %s", have_build ? ".build" : "fallback configure+make");
	int rc = system(build_cmd);
	if (rc != 0) {
		log_warn("Build selhal (%d) pro %s", rc, pkg_name);
		if (user_confirm("Retry build? [y/N]", 0)) {
			rc = system(build_cmd);
		}
		if (rc != 0) {
			log_error("Build definitivně selhal pro %s", pkg_name);
			goto fail_chdir;
		}
	}

done:
	// Cleanup
	free(source_dir);
	fclose(fp);
	// Odstranění build dir (jednoduché rm -rf)
	char rmcmd[512];
	snprintf(rmcmd, sizeof(rmcmd), "rm -rf '%s'", build_dir);
	system(rmcmd);
	free(build_dir);
	if (extras) json_decref(extras);
	return 0;

fail_chdir:
	free(source_dir);
fail:
	fclose(fp);
	char rmcmd2[512];
	snprintf(rmcmd2, sizeof(rmcmd2), "rm -rf '%s'", build_dir);
	system(rmcmd2);
	free(build_dir);
	if (extras) json_decref(extras);
	return -1;
}

