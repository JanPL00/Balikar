#include "includes/builder.h"

// Lokální stav
static int g_staging_enabled = 1;  // default zapnuto
static char *g_staging_dir = NULL;  // NULL = použít STAGING_DIR z common.h
static char *g_build_tmp_dir = NULL;  // NULL = použít /tmp

/**
 * Povolí nebo zakáže staging při sestavování balíku.
 *
 * Při povolení je proměnná prostředí DESTDIR nastavena na staging adresář.
 *
 * @param enabled Nenulová hodnota povolí staging, nula zakáže
 */
void set_staging_enabled(int enabled) {
    g_staging_enabled = enabled ? 1 : 0;
}

/**
 * Nastaví cestu k vlastnímu staging adresáři.
 *
 * Pokud je NULL, použije se výchozí STAGING_DIR z common.h.
 *
 * @param dir Cesta ke staging adresáři nebo NULL pro výchozí hodnotu
 */
void set_staging_dir(const char *dir) {
    if (g_staging_dir) free(g_staging_dir);
    g_staging_dir = dir ? strdup(dir) : NULL;
}

/**
 * Nastaví cestu k dočasnému adresáři pro sestavení.
 *
 * Pokud je NULL, jako základ pro build adresáře se použije /tmp.
 *
 * @param dir Cesta k dočasnému adresáři nebo NULL pro výchozí /tmp
 */
void set_build_tmp_dir(const char *dir) {
    if (g_build_tmp_dir) free(g_build_tmp_dir);
    g_build_tmp_dir = dir ? strdup(dir) : NULL;
}

/**
 * Zjistí typ komprese archivu čtením magic bytes.
 *
 * Podporuje formáty gzip, xz a bzip2.
 *
 * @param fp Ukazatel na soubor archivu (po čtení bude přetočen na začátek)
 * @return Typ komprese: 0=žádná/neznámá, 1=xz, 2=bz2, 3=gz
 */
static int detect_compression(FILE *fp) {
	unsigned char buf[6];
	size_t r = fread(buf, 1, sizeof(buf), fp);
	rewind(fp);  // Resetuj pozici souboru po přečtení magic bytes
	if (r < 2) return 0;
	
	// Zkontroluj gzip magic bytes: 1F 8B
	if (buf[0] == 0x1F && buf[1] == 0x8B) return 3; // gz formát
	
	// Zkontroluj xz magic bytes: FD 37 7A 58 5A 00
	if (r >= 6 && buf[0] == 0xFD && buf[1] == 0x37 && buf[2] == 0x7A && buf[3] == 0x58 && buf[4] == 0x5A && buf[5] == 0x00) return 1; // xz formát
	
	// Zkontroluj bzip2 magic bytes: 'BZ'
	if (buf[0] == 'B' && buf[1] == 'Z') return 2; // bz2 formát
	
	return 0; // neznámý / prostý formát
}

/**
 * Rekurzivně hledá build skript (build.sh) nebo soubory build systému.
 *
 * Používá prohledávání do šířky (BFS) pro preferenci mělčích adresářů.
 * Hledá build.sh, Makefile nebo configure v adresářovém stromě.
 *
 * @param dir Adresář, ve kterém se hledá
 * @param depth Aktuální hloubka rekurze (omezena na 3 úrovně)
 * @return Alokovaná cesta k adresáři s build soubory nebo NULL pokud nenalezeno
 */
static char *find_build_or_source(const char *dir, int depth) {
	// Omez hloubku rekurze pro zamezení nadměrného prohledávání
	if (depth > 3) return NULL;
	
	// První priorita: zkontroluj skript build.sh v aktuálním adresáři
	char build_path[MAX_PATH_LEN];
	snprintf(build_path, sizeof(build_path), "%s/build.sh", dir);
	if (access(build_path, F_OK) == 0) {
		return strdup(dir);
	}
	
	// Druhá priorita: zkontroluj Makefile nebo configure skript
	char make_path[MAX_PATH_LEN];
	snprintf(make_path, sizeof(make_path), "%s/Makefile", dir);
	char conf_path[MAX_PATH_LEN];
	snprintf(conf_path, sizeof(conf_path), "%s/configure", dir);
	char conf_upper[MAX_PATH_LEN];
	snprintf(conf_upper, sizeof(conf_upper), "%s/Configure", dir);
	char config_path[MAX_PATH_LEN];
	snprintf(config_path, sizeof(config_path), "%s/config", dir);
	if (access(make_path, F_OK) == 0 || access(conf_path, F_OK) == 0 || 
	    access(conf_upper, F_OK) == 0 || access(config_path, F_OK) == 0) {
		return strdup(dir);
	}
	
	// Zastav rekurzi na limitu hloubky
	if (depth >= 3) return NULL;
	
	// BFS: Nejprve zkontroluj všechny bezprostřední podadresáře pro build soubory
	// před rekurzivním zanořením. Tím se zajistí nalezení configure v
	// openssl-3.4.0/ před sestoupením do openssl-3.4.0/doc/life-cycles/
	DIR *d = opendir(dir);
	if (!d) return NULL;
	
	// Průchod 1: Zkontroluj každý podadresář (bez rekurze)
	struct dirent *ent;
	rewinddir(d);
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		
		char path[MAX_PATH_LEN];
		snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
		struct stat st;
		
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			// Zkontroluj tento podadresář pro build soubory (bez rekurze)
			char sub_build[MAX_PATH_LEN], sub_make[MAX_PATH_LEN], sub_conf[MAX_PATH_LEN];
			char sub_conf_upper[MAX_PATH_LEN], sub_config[MAX_PATH_LEN];
			snprintf(sub_build, sizeof(sub_build), "%s/build.sh", path);
			snprintf(sub_make, sizeof(sub_make), "%s/Makefile", path);
			snprintf(sub_conf, sizeof(sub_conf), "%s/configure", path);
			snprintf(sub_conf_upper, sizeof(sub_conf_upper), "%s/Configure", path);
			snprintf(sub_config, sizeof(sub_config), "%s/config", path);
			
			log_debug(1, "BFS Pass 1: Checking %s for build files", path);
			
			if (access(sub_build, F_OK) == 0 || 
			    access(sub_make, F_OK) == 0 || 
			    access(sub_conf, F_OK) == 0 ||
			    access(sub_conf_upper, F_OK) == 0 ||
			    access(sub_config, F_OK) == 0) {
				log_debug(1, "BFS Pass 1: Found build files in %s", path);
				closedir(d);
				return strdup(path);
			}
		}
	}
	
	// Průchod 2: Pokud nebyly nalezeny build soubory na této úrovni, rekurzivně zanořit
	rewinddir(d);
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

/**
 * Najde zdrojový adresář v rozbaleném archivu.
 *
 * Nejprve se pokouší rekurzivně najít soubory .build, Makefile nebo configure.
 * Pokud nejsou nalezeny, vrátí první podadresář.
 *
 * @param tmp_dir Kořenový adresář rozbaleného archivu
 * @return Alokovaná cesta ke zdrojovému adresáři nebo NULL při chybě
 */
char *find_source_path(const char *tmp_dir) {
	if (!tmp_dir) return NULL;
	
	// Strategie 1: Zkus rekurzivní vyhledávání build souborů
	char *found = find_build_or_source(tmp_dir, 0);
	if (found) return found;
	
	// Strategie 2: Záloha na první podadresář
	// (běžný vzor: archivy obsahují adresář nejvyšší úrovně)
	DIR *d = opendir(tmp_dir);
	if (!d) return strdup(tmp_dir);
	
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		// Přeskoč skryté záznamy
		if (ent->d_name[0] == '.') continue;
		
		char path[MAX_PATH_LEN];
		snprintf(path, sizeof(path), "%s/%s", tmp_dir, ent->d_name);
		struct stat st;
		
		// Vrať první nalezený adresář
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			closedir(d);
			return strdup(path);
		}
	}
	closedir(d);
	
	// Strategie 3: Poslední možnost – použij samotný tmp_dir
	return strdup(tmp_dir);
}

/**
 * Aplikuje extra soubory (patche, vlastní build skripty) do zdrojového adresáře.
 *
 * Kopíruje soubory z cache/extras/<pkg_name>/ do build adresáře.
 * Skripty build.sh se kopírují do source_dir, ostatní soubory do nadřazeného adresáře.
 *
 * @param extras JSON pole názvů extra souborů k aplikaci
 * @param pkg_name Název balíku (slouží k nalezení adresáře extras)
 * @param source_dir Zdrojový adresář, kam se mají extras aplikovat
 */
void apply_extras(json_t *extras, const char *pkg_name, const char *source_dir) {
	if (!extras || !json_is_array(extras) || !pkg_name || !source_dir) return;
	
	// Najdi adresář extras v cache
	char *cache_dir = get_cache_dir();
	if (!cache_dir) return;
	char patch_root[2048];
	snprintf(patch_root, sizeof(patch_root), "%s/extras/%s", cache_dir, pkg_name);
	DIR *d = opendir(patch_root);
	if (!d) { free(cache_dir); return; }
	
	// Získej nadřazený adresář source_dir pro kopírování non-build souborů
	char parent_dir[4096];
	snprintf(parent_dir, sizeof(parent_dir), "%s/..", source_dir);
	
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;

		// Zkontroluj zda má být soubor použit:
		// 1. Vždy zkopíruj skript build.sh
		// 2. Zkopíruj soubor pokud je uveden v JSON poli extras
		int found = 0;
		if (strcmp(ent->d_name, "build.sh") == 0) {
			found = 1;  // build.sh se vždy kopíruje
		} else {
			// Zkontroluj zda je název souboru v seznamu extras
			size_t i; json_t *item;
			json_array_foreach(extras, i, item) {
				if (json_is_string(item) && strstr(json_string_value(item), ent->d_name)) { 
					found = 1; 
					break; 
				}
			}
		}
		if (!found) continue;
		
		char extra_file[4096];
		snprintf(extra_file, sizeof(extra_file), "%s/%s", patch_root, ent->d_name);
		
		// Urči cíl:
		// - skript build.sh jde do source_dir (kde bude spuštěn)
		// - Ostatní soubory (záplaty, konfigurace) jdou do nadřazeného adresáře
		char dest_path[4096];
		if (strcmp(ent->d_name, "build.sh") == 0) {
			snprintf(dest_path, sizeof(dest_path), "%s/%s", source_dir, ent->d_name);
		} else {
			snprintf(dest_path, sizeof(dest_path), "%s/%s", parent_dir, ent->d_name);
		}
		
		char cmd[8192];
		snprintf(cmd, sizeof(cmd), "cp -a '%s' '%s'", extra_file, dest_path);
		log_debug(1, "Copying from '%s' to '%s'", extra_file, dest_path);
		log_debug(1, "Running: %s", cmd);
		int rc = system(cmd);
		log_debug(1, "Copy command returned: %d", rc);
		if (rc == 0) {
			log_info("Copied extra file: %s -> %s", ent->d_name, 
			         strcmp(ent->d_name, "build.sh") == 0 ? source_dir : parent_dir);
			// Ověř že soubor byl skutečně zkopírován
			struct stat st;
			if (stat(dest_path, &st) == 0) {
				log_debug(1, "Verified copy: %s (size: %ld)", dest_path, st.st_size);
			} else {
				log_warn("Copy reported success but file doesn't exist: %s", dest_path);
			}
		} else {
			log_warn("Copy failed (%d): %s", rc, ent->d_name);
		}
	}
	closedir(d);
	free(cache_dir);
}

/**
 * Spustí build callback funkci v chroot prostředí.
 *
 * Forkuje child proces, provede chroot do zadaného adresáře a spustí callback.
 * Slouží pro izolované sestavení s kontrolovaným přístupem k filesystému.
 *
 * @param root Kořenový adresář pro chroot
 * @param func Callback funkce pro spuštění v chroot
 * @param data Uživatelská data předaná callbacku
 * @return Návratový kód callbacku nebo -1 při chybě
 */
int run_with_chroot(const char *root, build_callback func, void *data) {
	if (!root || !func) return -1;
	
	// Fork pro vytvoření izolovaného procesu pro chroot
	pid_t pid = fork();
	if (pid < 0) {
		log_error("Fork failed for chroot isolation");
		return -1;
	}
	
	if (pid == 0) {
		// Dceřiný proces: proveď chroot a spusť callback
		if (chroot(root) != 0) {
			log_error("Chroot failed: %s", strerror(errno));
			exit(1);
		}
		
		// Přejdi do kořenového adresáře uvnitř chroot
		if (chdir("/") != 0) {
			log_error("Chdir after chroot failed");
			exit(1);
		}
		
		// Spusť callback a ukonči s jeho návratovým kódem
		int rc = func(data);
		exit(rc);
	}
	
	// Rodičovský proces: čekej na dokončení dítěte
	int status;
	if (waitpid(pid, &status, 0) < 0) {
		log_error("Waitpid failed");
		return -1;
	}
	
	// Vrať exit status dítěte
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return -1;
}

/**
 * Rozbalí archivní soubor do adresáře.
 *
 * Automaticky zjistí typ komprese a použije odpovídající příznaky taru.
 * Podporuje plain tar, gzip, xz a bzip2 komprimované archivy.
 *
 * @param fp Ukazatel na soubor archivu (čte se od začátku)
 * @param tmp_dir Cílový adresář pro rozbalení
 * @return 0 při úspěchu, -1 při chybě
 */
static int extract_archive(FILE *fp, const char *tmp_dir) {
	if (!fp || !tmp_dir) return -1;
	
	rewind(fp);
	// Detekuj formát komprese podle magic bytes
	int comp = detect_compression(fp);
	
	// Vyber příslušné příznaky tar podle komprese
	const char *tarflag = "xf"; // prostý tar
	if (comp == 1) tarflag = "xJf"; // xz komprese
	else if (comp == 2) tarflag = "xjf"; // bz2 komprese
	else if (comp == 3) tarflag = "xzf"; // gz komprese
	
	// Otevři tar proces s rourou pro stdin
	char cmd[4096];
	snprintf(cmd, sizeof(cmd), "tar %s - -C '%s'", tarflag, tmp_dir);
	FILE *tar = popen(cmd, "w");
	if (!tar) return -1;
	
	// Streamuj data archivu do tar procesu
	unsigned char buf[8192];
	size_t r;
	while ((r = fread(buf, 1, sizeof(buf), fp)) > 0) {
		if (fwrite(buf, 1, r, tar) != r) {
			pclose(tar);
			return -1;
		}
	}
	
	// Počkej na dokončení tar
	int status = pclose(tar);
	if (status != 0) return -1;
	
	// Zkontroluj zda je to balík balikar s vnořeným zdrojovým archivem
	// Hledej index.json v prvním podadresáři
	DIR *d = opendir(tmp_dir);
	if (d) {
		struct dirent *ent;
		char first_subdir[2048] = {0};
		
		// Najdi první podadresář
		while ((ent = readdir(d)) != NULL) {
			if (ent->d_name[0] == '.') continue;
			
			char test_path[2048];
			snprintf(test_path, sizeof(test_path), "%s/%s", tmp_dir, ent->d_name);
			
			struct stat st;
			if (stat(test_path, &st) == 0 && S_ISDIR(st.st_mode)) {
				strncpy(first_subdir, test_path, sizeof(first_subdir) - 1);
				break;
			}
		}
		closedir(d);
		
		// Zkontroluj zda existuje index.json
		if (first_subdir[0]) {
			char index_json_path[2560];
			snprintf(index_json_path, sizeof(index_json_path), "%s/index.json", first_subdir);
			
			if (access(index_json_path, F_OK) == 0) {
				log_debug(1, "Found index.json, checking for nested source archive");
				
				// Načti index.json
				json_error_t error;
				json_t *index_root = json_load_file(index_json_path, 0, &error);
				if (index_root) {
					json_t *source_array = json_object_get(index_root, "source");
					if (source_array && json_is_array(source_array) && json_array_size(source_array) > 0) {
						json_t *first_source = json_array_get(source_array, 0);
						json_t *url_obj = json_object_get(first_source, "url");
						
						if (url_obj && json_is_string(url_obj)) {
							const char *source_file = json_string_value(url_obj);
							char nested_tar_path[2560];
							
							// Zkontroluj zda je zdrojový soubor vložen lokálně (ne vzdálená URL)
							if (!strstr(source_file, "://")) {
								// Jde o lokální název souboru – zkontroluj zda soubor existuje v rozbaleném archivu repo
								snprintf(nested_tar_path, sizeof(nested_tar_path), "%s/%s", first_subdir, source_file);
								
								if (access(nested_tar_path, F_OK) == 0) {
									log_info("Extracting embedded source: %s", source_file);
									
									// Rozbal vnořený tar do NADŘAZENÉHO adresáře (tmp_dir), ne do first_subdir
									// Tím se zabrání dvojitému vnoření: openssl-3.4.0/openssl-3.4.0/
									char extract_cmd[4096];
									snprintf(extract_cmd, sizeof(extract_cmd), "tar -xf '%s' -C '%s' 2>/dev/null", nested_tar_path, tmp_dir);
									
									int extract_status = system(extract_cmd);
									if (extract_status == 0) {
										log_debug(1, "Embedded source extracted successfully to build directory");
									} else {
										log_warn("Failed to extract embedded source (status=%d)", extract_status);
									}
								} else {
									log_debug(1, "Embedded source file not found: %s (already extracted by downloader?)", source_file);
								}
							}
							// Poznámka: Vzdálené URL jsou nyní zpracovávány downloaderem před sestavením
						}
					}
					json_decref(index_root);
				}
			}
		}
	}
	
	return 0;
}

/**
 * Sestaví a nainstaluje balík ze zdrojového archivu.
 *
 * Hlavní orchestrační funkce sestavení:
 * 1. Rozbalí archiv do dočasného adresáře
 * 2. Najde zdrojový adresář s build soubory
 * 3. Aplikuje extra soubory (patche, vlastní build skripty)
 * 4. Nastaví build prostředí (staging, MAKEFLAGS)
 * 5. Spustí build skript nebo záložní configure+make
 * 6. Zpracuje chyby a uvolní prostředky
 *
 * @param trans Kontext transakce (zatím nevyužito)
 * @param p ID balíku (solvable ID)
 * @param fp Ukazatel na soubor zdrojového archivu
 * @param pool Pool pro rozlišení závislostí
 * @param is_group Příznak skupinového balíku (pouze extrakce, žádné sestavení)
 * @param no_package Příznak meta-balíku bez archivu
 * @param extras JSON pole extra souborů k aplikaci
 * @return 0 při úspěchu, -1 při chybě
 */
int commit_element(Transaction *trans, Id p, FILE *fp, Pool *pool, int is_group, int no_package, json_t *extras) {
	(void)trans; // Vyhrazeno pro budoucí použití (např. pořadí transakcí)
	if (!fp || !pool) return -1;

	// Extrahuj metadata balíku

	char *pkg_name = pool_id2str(pool, pool->solvables[p].name);
	
	// Přeskoč virtuální balíky (žádné skutečné soubory k sestavení)
	if (no_package) {
		log_info("Skipping virtual/no_package: %s", pkg_name);
		if (fp) fclose(fp);
		if (extras) json_decref(extras);
		return 0;
	}

	// Vytvoř dočasný adresář sestavení pomocí nakonfigurovaného umístění tmp
	const char *tmp_base = g_build_tmp_dir ? g_build_tmp_dir : "/tmp";
	char tmpl[512];
	snprintf(tmpl, sizeof(tmpl), "%s/balikar_build_%s_XXXXXX", tmp_base, pkg_name ? pkg_name : "pkg");
	char *build_dir = strdup(tmpl);
	if (!build_dir) return -1;
	
	// Vytvoř jedinečný adresář pomocí mkdtemp (nahradí XXXXXX náhodnými znaky)
	if (!mkdtemp(build_dir)) {
		log_error("mkdtemp failed for %s", pkg_name);
		free(build_dir);
		return -1;
	}
	log_info("Build dir: %s", build_dir);

	// Rozbal zdrojový archiv do adresáře sestavení
	if (extract_archive(fp, build_dir) != 0) {
		log_error("Archive extraction failed for %s", pkg_name);
		goto fail;
	}

	// Najdi skutečný zdrojový adresář v rozbaleném archivu
	char *source_dir = find_source_path(build_dir);
	if (!source_dir) {
		log_error("Source dir not found for %s", pkg_name);
		goto fail;
	}
	log_info("Source dir: %s", source_dir);

	// Aplikuj doplňkové soubory (záplaty, vlastní build skripty)
	if (extras) apply_extras(extras, pkg_name, source_dir);

	// Konfiguruj prostředí sestavení
	// Nastav DESTDIR pro staging instalaci pokud je povolena
	// Vyčisti a znovu vytvoř staging adresář pro zamezení konfliktů mezi sestaveními
	if (g_staging_enabled) {
		const char *destdir = g_staging_dir ? g_staging_dir : STAGING_DIR;
		
		// Vyčisti a znovu vytvoř staging adresář
		char cleanup_cmd[512];
		snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf '%s' 2>/dev/null && mkdir -p '%s'", destdir, destdir);
		system(cleanup_cmd);
		log_debug(1, "Cleaned and recreated staging directory: %s", destdir);
		
		setenv("DESTDIR", destdir, 1);
		log_debug(1, "DESTDIR set to: %s", destdir);
	} else {
		unsetenv("DESTDIR");
	}
	
	// Nastav MAKEFLAGS pro paralelní kompilaci
	long nproc = sysconf(_SC_NPROCESSORS_ONLN);
	if (nproc < 1) nproc = 1;
	char mf[64]; snprintf(mf, sizeof(mf), "-j%ld", nproc);
	setenv("MAKEFLAGS", mf, 1);

	// Zpracuj skupinové balíky (pouze data, není potřeba kompilace)
	if (is_group) {
		log_info("is_group: extraction only for %s", pkg_name);
		goto done;
	}

	// Spusť proces sestavení
	// Přejdi do zdrojového adresáře pro sestavení
	if (chdir(source_dir) != 0) {
		log_error("Cannot chdir to %s", source_dir);
		goto fail_chdir;
	}
	
	// Urči strategii sestavení:
	// 1. Pokud existuje skript build.sh, použij ho (preferovaná varianta)
	// 2. Jinak použij standardní configure+make (záloha)
	log_debug(1, "Checking for build.sh in directory: %s", source_dir);
	char build_fullpath[4096];
	snprintf(build_fullpath, sizeof(build_fullpath), "%s/build.sh", source_dir);
	log_debug(1, "Full build.sh path would be: %s", build_fullpath);
	
	struct stat build_st;
	int have_build = access("build.sh", X_OK) == 0;
	if (stat("build.sh", &build_st) == 0) {
		log_debug(1, "build.sh exists: size=%ld, executable=%d", build_st.st_size, have_build);
	} else {
		log_debug(1, "build.sh does NOT exist in %s (errno=%d)", source_dir, errno);
	}
	
	// Připrav příkaz sestavení s explicitně nastaveným DESTDIR v příkazu
	// Tím se zajistí dostupnost DESTDIR i pokud setenv() nepropaguje správně
	char build_cmd[2048];
	const char *destdir = g_staging_enabled ? (g_staging_dir ? g_staging_dir : STAGING_DIR) : "";
	
	if (have_build) {
		if (g_staging_enabled) {
			snprintf(build_cmd, sizeof(build_cmd), "DESTDIR='%s' bash -e build.sh > build.log 2>&1", destdir);
		} else {
			snprintf(build_cmd, sizeof(build_cmd), "bash -e build.sh > build.log 2>&1");
		}
	} else {
		// Záloha: standardní configure+make+install s DESTDIR
		if (g_staging_enabled) {
			snprintf(build_cmd, sizeof(build_cmd), "./configure --prefix=/usr >/dev/null 2>&1 && make >/dev/null 2>&1 && make DESTDIR='%s' install >/dev/null 2>&1", destdir);
		} else {
			snprintf(build_cmd, sizeof(build_cmd), "./configure --prefix=/usr >/dev/null 2>&1 && make >/dev/null 2>&1 && make install >/dev/null 2>&1");
		}
	}
	
	log_info("Running build: %s", have_build ? "build.sh" : "fallback configure+make");
	log_debug(1, "Build command: %s", build_cmd);
	
	// Spusť příkaz sestavení
	int rc = system(build_cmd);
	if (rc != 0) {
		log_warn("Build failed (%d) for %s", rc, pkg_name);
		
		// Zobraz poslední řádky build logu pro pomoc při diagnostice selhání
		if (access("build.log", F_OK) == 0) {
			log_warn("--- Last lines of build.log ---");
			system("tail -20 build.log >&2");
			log_warn("--- End of build.log ---");
		}
		
		// Zopakuj pokus se sekvenčním make (MAKEFLAGS=-j1) pro zamezení race conditions při paralelním sestavení
		log_info("Retrying build with sequential make (MAKEFLAGS=-j1)...");
		char retry_cmd[2048];
		if (have_build) {
			if (g_staging_enabled) {
				snprintf(retry_cmd, sizeof(retry_cmd),
					"DESTDIR='%s' MAKEFLAGS='-j1' bash -e build.sh > build.log 2>&1", destdir);
			} else {
				snprintf(retry_cmd, sizeof(retry_cmd),
					"MAKEFLAGS='-j1' bash -e build.sh > build.log 2>&1");
			}
		} else {
			snprintf(retry_cmd, sizeof(retry_cmd), "%s", build_cmd);
		}
		rc = system(retry_cmd);
		
		if (rc != 0) {
			// Zobraz také log z neúspěšného opakování
			if (access("build.log", F_OK) == 0) {
				log_warn("--- Build.log after retry ---");
				system("tail -30 build.log >&2");
				log_warn("--- End ---");
			}
			log_error("Build permanently failed for %s", pkg_name);
			goto fail_chdir;
		}
		log_info("Build succeeded on retry with sequential make");
	}

done:
	// Úspěšné dokončení – uklid prostředky
	free(source_dir);
	fclose(fp);
	
	// Přejdi z build_dir před jeho odstraněním
	// (některé systémy neumožňují smazat aktuální pracovní adresář)
	chdir("/tmp");
	
	// Odstraň dočasný adresář sestavení
	char rmcmd[512];
	snprintf(rmcmd, sizeof(rmcmd), "rm -rf '%s'", build_dir);
	system(rmcmd);
	free(build_dir);
	if (extras) json_decref(extras);
	return 0;

fail_chdir:
	free(source_dir);
fail:
	// Zpracování chyb – uklid prostředky
	fclose(fp);
	
	// Přejdi z build_dir před jeho odstraněním
	chdir("/tmp");
	
	// Odstraň dočasný adresář sestavení
	char rmcmd2[512];
	snprintf(rmcmd2, sizeof(rmcmd2), "rm -rf '%s'", build_dir);
	system(rmcmd2);
	free(build_dir);
	if (extras) json_decref(extras);
	return -1;
}

