# Přehled Práce s Metadaty, Konfigurací a Soubory

Komplexní dokumentace o všech souborech, které Balikar package manager čte, vytváří a spravuje během svého provozu.

---

## Obsah

1. [Konfigurační Soubory](#1-konfigurační-soubory)
2. [Metadata Balíčků](#2-metadata-balíčků)
3. [Cache a Dočasné Soubory](#3-cache-dočasné-soubory-a-staging)
4. [Databáze a Tracking](#4-databáze-a-tracking)
5. [Matice Operací](#5-matice-operací)
6. [CLI Příklady](#6-cli-příklady)

---

## 1. Konfigurační Soubory

### 1.1 Repository Configuration

#### `~/.config/balikar/repos.conf`
**Typ:** Uživatelská konfigurace repozitářů  
**Formát:** INI (key=value s [sections])  
**Modul:** `src/common/config.c`  
**Kdy se vytváří:** Při prvním volání `save_repo_config()` nebo CLI příkazu `balikar repo add`  
**Kdy se čte:** Při každém spuštění Balikaru přes `init_repo_config()`

**Struktura:**
```ini
# Balikar Repository Configuration
# Auto-generated file

[repository-name]
url = https://example.com/packages nebo ./repo
priority = 100
enabled = true

[local-dev]
url = ./repo
priority = 50
enabled = true
```

**Příklad vytvoření:**
```bash
# CLI příkaz, který vytvoří/upraví soubor
balikar repo add testing https://testing.example.com/packages 80
```

**Relevantní funkce:**
- `init_repo_config()` - Načítání konfigurace
- `save_repo_config()` - Ukládání změn
- `add_repository()` - Přidání nového repozitáře
- `remove_repository()` - Odebrání repozitáře
- `load_config_file()` - Parsování INI formátu

---

#### `/etc/balikar/repos.conf`
**Typ:** Systémová konfigurace repozitářů  
**Formát:** INI (stejný jako uživatelská)  
**Modul:** `src/common/config.c`  
**Kdy se vytváří:** Manuálně správcem systému  
**Kdy se čte:** Jako fallback pokud `~/.config/balikar/repos.conf` neexistuje

**Priorita načítání:**
1. `~/.config/balikar/repos.conf` (nejvyšší)
2. `/etc/balikar/repos.conf` (fallback)
3. Hardcoded default: `./repo` s prioritou 10

**Poznámka:** Pokud existuje uživatelská konfigurace, systémová se zcela ignoruje.

---

### 1.2 Default Repository

#### `./repo` (hardcoded fallback)
**Typ:** Výchozí lokální repozitář  
**Modul:** `src/common/config.c`  
**Kdy se použije:** Pokud neexistuje žádná konfigurace  
**Priorita:** 10 (nejnižší)

**Vytvoření default repository:**
```c
// V config.c, funkce init_repo_config()
if (!repo_list) {
    repo_config_t *default_repo = calloc(1, sizeof(repo_config_t));
    default_repo->name = strdup("default");
    default_repo->url = strdup("./repo");
    default_repo->priority = 10;
    default_repo->enabled = 1;
    repo_list = default_repo;
}
```

---

## 2. Metadata Balíčků

### 2.1 Repository Metadata

#### `repodata.json`
**Umístění:** 
- Lokální repozitáře: `<repo_path>/repodata.json` (např. `./repo/repodata.json`)
- Remote repozitáře: Cache v `/var/cache/balikar/downloads/repodata.json`

**Typ:** Agregovaná metadata všech balíčků v repozitáři  
**Formát:** JSON  
**Modul:** `src/balikar/parser.c`  
**Kdy se vytváří:** 
- Automaticky při prvním načtení repozitáře pokud chybí
- Automaticky při detekci změn v `index.json` souborech (podle mtime)
- Manuálně příkazem `balikar repo-update`

**Kdy se čte:** Při každém příkazu, který potřebuje informace o balíčcích

**Struktura:**
```json
{
  "info": {
    "subdir": "noarch"
  },
  "packages": {
    "gzip-1.14-0.tar.bz2": {
      "name": "gzip",
      "version": "1.14",
      "build": "0",
      "arch": "x86_64",
      "depends": [],
      "makedeps": ["gcc", "make"],
      "download_url": "https://example.com/g/gzip/gzip-1.14.tar.bz2",
      "source": [
        {
          "url": "https://ftp.gnu.org/gnu/gzip/gzip-1.14.tar.xz",
          "sha256": "977c8c0c5ae6309191e7768e28ebc951..."
        },
        {
          "url": "https://mirror.example.com/gzip/gzip-1.14.tar.xz",
          "sha256": "977c8c0c5ae6309191e7768e28ebc951..."
        }
      ],
      "extras": [
        "https://example.com/patches/gzip-fixes.patch"
      ],
      "license": "GPLv3+",
      "is_group": false,
      "no_package": false
    },
    "bash-5.3-0.tar.bz2": {
      "name": "bash",
      "version": "5.3",
      "build": "0",
      "depends": ["glibc", "readline"],
      "makedeps": ["gcc", "make", "bison"],
      "download_url": "https://example.com/b/bash/bash-5.3.tar.bz2",
      "source": [
        {
          "url": "https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz"
        }
      ],
      "extras": []
    }
  }
}
```

**Relevantní funkce:**
- `generate_repodata()` - Generování repodata.json
- `load_repository()` - Načítání a parsování
- `check_index_newer_than_repodata()` - Detekce změn
- `collect_packages()` - Sběr metadata ze všech balíčků

**Automatická regenerace:**
```c
// V parser.c, funkce load_repository()
if (check_index_newer_than_repodata(repo_path, repodata_st.st_mtime)) {
    log_info("Detected changes in index.json files – regenerating repodata.json...");
    if (generate_repodata(repo_path, pool, repo) != 0) {
        log_error("Failed to regenerate repodata.json");
        return NULL;
    }
}
```

---

### 2.2 Package Metadata

#### `index.json`
**Umístění:** `<repo>/<písmeno>/<název>/<název-verze>/index.json`  
**Příklad:** `./repo/g/gzip/gzip-1.14/index.json`

**Typ:** Metadata jednotlivého balíčku  
**Formát:** JSON  
**Modul:** `src/balikar/parser.c`  
**Kdy se vytváří:** Manuálně při přidávání balíčku do repozitáře  
**Kdy se čte:** 
- Při generování `repodata.json` (scanování celého repozitáře)
- Při kontrole změn (mtime check)

**Struktura:**
```json
{
  "name": "gzip",
  "version": "1.14",
  "build": "0",
  "arch": "x86_64",
  "depends": [],
  "makedeps": ["gcc", "make"],
  "download_url": "https://raw.githubusercontent.com/user/repo/main/g/gzip/gzip-1.14.tar.bz2",
  "source": [
    {
      "url": "https://ftp.gnu.org/gnu/gzip/gzip-1.14.tar.xz",
      "sha256": "977c8c0c5ae6309191e7768e28ebc951c9bc07087fa0db41d3bb991ef0b0c2bb"
    }
  ],
  "extras": [],
  "license": "GPLv3+",
  "summary": "GNU compression utility",
  "is_group": false,
  "no_package": false
}
```

**Popis polí:**
- `name`, `version`, `build` - Identifikace balíčku
- `arch` - Architektura (x86_64, noarch, ...)
- `depends` - Runtime závislosti (pole stringů)
- `makedeps` - Build-time závislosti (pole stringů)
- `download_url` - URL k předkompilovanému balíčku (pro budoucí binární podporu)
- `source` - **Array** zdrojových souborů s multi-mirror podporou
  - `url` - URL ke stažení zdrojového kódu
  - `sha256` - SHA256 checksum (volitelný, pro verifikaci)
- `extras` - Patches a doplňkové soubory ke stažení
- `is_group` - Meta-balíček (pouze metadata, žádný build)
- `no_package` - Virtuální balíček (skip download i build)

**Poznámka k `source` poli:**
- **Nový formát** (array objektů) - podporuje více mirror URL s automatickým fallbackem
- **Starý formát** (string) - stále podporován pro zpětnou kompatibilitu
- Downloader zkouší URL v pořadí až do úspěchu

**Relevantní funkce:**
- `load_json()` - Načítání JSON souboru
- `find_and_load_index()` - Rekurzivní hledání index.json
- `add_default_fields()` - Doplnění výchozích hodnot
- `process_index_json()` - Zpracování metadat

---

#### `info/index.json` (alternativní umístění)
**Umístění:** `<repo>/<písmeno>/<název>/<název-verze>/info/index.json`

**Typ:** Stejný jako `index.json`, alternativní cesta  
**Použití:** Podporováno pro kompatibilitu s různými strukturami balíčků  
**Priorita vyhledávání:** 
1. `<balíček>/index.json`
2. `<balíček>/info/index.json`
3. Rekurzivní hledání v podadresářích (max depth 3)

---

## 3. Cache a Dočasné Soubory

### 3.1 Download Cache

#### `/var/cache/balikar/downloads/`
**Typ:** Hlavní cache adresář pro stažené soubory  
**Modul:** `src/balikar/downloader.c`  
**Kdy se vytváří:** Automaticky při prvním stahování  
**Kdy se čte:** Před každým stahováním (kontrola existence v cache)  
**Kdy se maže:** Příkazem `balikar clean-cache`

**Obsah:**
```
/var/cache/balikar/downloads/
├── gzip-1.14.tar.xz          # Stažený zdrojový tarball
├── bash-5.3.tar.gz           # Další balíčky
├── glibc-2.40.tar.xz
├── repodata.json             # Cache metadat z remote repozitářů
└── extras/                   # Patches a doplňkové soubory
    ├── gzip/
    │   └── gzip-fixes.patch
    └── glibc/
        ├── glibc-fhs-1.patch
        └── tzdata.tar.gz
```

**Workflow stahování s cache:**
```c
// V downloader.c, funkce download_package()
snprintf(cache_path, sizeof(cache_path), "%s/%s-%s%s", 
         cache_dir, solvable_name, solvable_version, ext);

if (access(cache_path, F_OK) == 0) {
    log_debug(1, "Package found in cache: %s", cache_path);
    return fopen(cache_path, "rb");
}

// Cache miss - stáhnout balíček
log_debug(1, "Package not in cache, downloading...");
```

**Relevantní funkce:**
- `download_package()` - Stahování s cache kontrolou
- `get_cache_dir()` - Získání cesty k cache
- Clean funkcionalita v CLI

---

#### `/var/cache/balikar/downloads/extras/<pkg_name>/`
**Typ:** Cache pro patches a doplňkové soubory  
**Struktura:** Podadresář pro každý balíček  
**Modul:** `src/balikar/downloader.c`  
**Kdy se vytváří:** Při stahování extras souborů

**Příklad:**
```
/var/cache/balikar/downloads/extras/
├── gzip/
│   └── gzip-fixes.patch
├── glibc/
│   ├── glibc-fhs-1.patch
│   └── tzdata.tar.gz
└── gcc/
    ├── gcc-patch-001.patch
    └── gcc-patch-002.patch
```

**Relevantní funkce:**
- `download_extras()` - Stahování patches
- `apply_extras()` - Aplikace patches během buildu

---

### 3.2 Build Temporary Files

#### `/tmp/balikar_build_<pkg>_XXXXXX/`
**Typ:** Dočasný build adresář  
**Formát:** `mkdtemp()` s unikátním sufixem  
**Modul:** `src/balikar/builder.c`  
**Kdy se vytváří:** Pro každý build (funkce `commit_element()`)  
**Kdy se maže:** Automaticky po dokončení buildu (úspěch i selhání)

**Konfigurace:**
```c
// Globální proměnná v builder.c
static char *g_build_tmp_dir = NULL;  // NULL = výchozí /tmp

// CLI nastavení
set_build_tmp_dir("/var/tmp/builds");
```

**CLI použití:**
```bash
# Výchozí: /tmp/balikar_build_gzip_A3F8K2
balikar build gzip

# Vlastní tmp adresář
balikar --build-tmp-dir=/var/tmp/isolated build gcc
```

**Obsah během buildu:**
```
/tmp/balikar_build_gzip_A3F8K2/
├── gzip-1.14/              # Extrahovaný tarball
│   ├── .build              # Build skript
│   ├── configure           # Zdrojové soubory
│   ├── Makefile.in
│   ├── src/
│   └── ...
└── build.log               # Build output (pokud redirect)
```

**Relevantní funkce:**
- `commit_element()` - Hlavní build funkce
- `extract_archive()` - Extrakce do tmp
- `find_source_path()` - Hledání .build ve tmp

---

### 3.3 Staging Directory

#### `$P/` nebo vlastní staging
**Typ:** Staging oblast pro build výsledky (DESTDIR)  
**Výchozí:** `$P` (shell proměnná)  
**Modul:** `src/balikar/builder.c`  
**Kdy se vytváří:** Před spuštěním build skriptu  
**Kdy se maže:** Po instalaci nebo manuálně

**Konfigurace:**
```c
// Globální proměnné v builder.c
static int g_staging_enabled = 1;      // Staging zapnuto/vypnuto
static char *g_staging_dir = NULL;     // NULL = $P

// CLI nastavení
set_staging_enabled(1);
set_staging_dir("/mnt/lfs/stage");
```

**CLI použití:**
```bash
# Build do staging (BEZ merge do systému)
balikar build gzip
# → DESTDIR=$P
# → Výsledky zůstávají v $P/usr/bin/gzip
# → Žádná instalace, staging zůstává pro kontrolu

# Install používá staging + merge (full workflow)
balikar install glibc
# → DESTDIR=$P
# → Merge: $P/* → /usr/
# → Cleanup staging po instalaci

# Vlastní staging pro oba příkazy
balikar --staging-dir=/mnt/lfs/stage build gcc
# nebo
balikar --staging-dir=/mnt/lfs/stage install glibc

# Přímá instalace BEZ staging (RIZIKO!)
balikar --no-staging build hello
# DESTDIR není nastaveno
# make install → přímo do /usr/ (NEBEZPEČNÉ!)
# Pouze pro testování v izolované VM
```

**Struktur staging:**
```
$P/  nebo /mnt/lfs/stage/
├── usr/
│   ├── bin/
│   │   ├── gzip           # Build výsledky
│   │   └── gunzip
│   ├── lib/
│   │   └── libz.so.1.3.1
│   ├── share/
│   │   └── man/man1/gzip.1
│   └── include/
│       └── zlib.h
└── etc/                   # Volitelná konfigurace
```

**Workflow:**

**`balikar build` workflow (se stagingem):**
1. `make install DESTDIR=$P` → soubory v `$P/usr/`
2. Žádná instalace - staging zůstává
3. Použití: Verifikace výsledků před instalací

**`balikar install` workflow (se stagingem):**
1. `make install DESTDIR=$P` → soubory v `$P/usr/`
2. `cp -a $P/* /target_root/` → merge do systému
3. `rm -rf $P/*` → cleanup staging

**`balikar --no-staging build` workflow (NEBEZPEČNÉ):**
1. `make install` BEZ DESTDIR → přímo do `/usr/`
2. Žádná izolace, žádný rollback
3. Pouze pro testování v disposable VM!

**Relevantní funkce:**
- `commit_element()` - Nastavení DESTDIR pro build
- `merge_staging_to_root()` - Kopírování do target root

---

### 3.4 Staging Workflow

#### Proč Staging?

Balikar používá **staging adresář** (`$P/` nebo vlastní) jako bezpečnostní vrstvu mezi buildem a systémem:

**Výhody stagingu:**
- **Izolace:** Build nikdy neinstaluje přímo do `/usr/` (bezpečné)
- **Verifikace:** Možnost kontroly výsledků před instalací do systému
- **Rollback:** Jednoduchý cleanup - stačí smazat obsah staging
- **LFS pattern:** Standardní postup při budování Linux From Scratch

#### Staging u Různých Příkazů

**`balikar build <pkg>` - Build se stagingem, BEZ instalace**

```
┌──────────────────────────────────────────────────────────┐
│  Download → Extract → Build                              │
│                         ↓                                 │
│                    DESTDIR=$P                             │
│                         ↓                                 │
│              make install DESTDIR=$P                      │
│                         ↓                                 │
│        $P/usr/bin/gzip, $P/usr/lib/...                    │
│                         ↓                                 │
│                   KONEC WORKFLOW                          │
│              (staging zůstává pro kontrolu)               │
└──────────────────────────────────────────────────────────┘
```

**Použití:**
- Testování buildu bez instalace
- Verifikace výsledků: `ls $P/usr/bin/`
- Debug buildů před nasazením

**`balikar install <pkg>` - Full workflow se stagingem**

```
┌──────────────────────────────────────────────────────────┐
│  Download → Extract → Build                              │
│                         ↓                                 │
│                    DESTDIR=$P                             │
│                         ↓                                 │
│              make install DESTDIR=$P                      │
│                         ↓                                 │
│        $P/usr/bin/gzip, $P/usr/lib/...                    │
│                         ↓                                 │
│                  MERGE DO SYSTÉMU                         │
│                  cp -a $P/* /                             │
│                         ↓                                 │
│            /usr/bin/gzip, /usr/lib/...                    │
│                         ↓                                 │
│              TRACKING + CLEANUP                           │
│    installed.json updated, rm -rf $P/*                    │
└──────────────────────────────────────────────────────────┘
```

**Použití:**
- Plná instalace balíčku do systému
- Automatický tracking do databáze
- Cleanup staging po úspěchu

#### Přímá Instalace (--no-staging)

**VAROVÁNÍ:** Nebezpečné! Pouze pro testování v izolovaném prostředí.

```bash
balikar --no-staging build hello
# DESTDIR není nastaveno
# make install → přímo do /usr/ (NEBEZPEČNÉ!)
```

**Kdy se hodí:**
- Disposable VM pro testování
- Chroot prostředí kde není riziko poškození hostu
- Debug problémů s DESTDIR

**Proč se vyhýbat:**
- Žádná izolace - může poškodit systém
- Nemožný rollback
- Při chybě buildu zůstávají částečné soubory v `/usr/`

#### Vlastní Staging Adresář

```bash
# LFS build - staging na vlastním filesystému
balikar --staging-dir=/mnt/lfs/stage build gcc
# Výsledky v: /mnt/lfs/stage/usr/bin/gcc

# Btrfs snapshot pro rollback
balikar --staging-dir=/btrfs/stage build glibc
btrfs subvolume snapshot /btrfs/stage /btrfs/stage-backup

# Tmpfs pro rychlý build
balikar --staging-dir=/tmp/fast-stage build hello
```

#### Shrnutí Staging

| Příkaz | Staging | Merge | Tracking | Cleanup | Použití |
|--------|---------|-------|----------|---------|----------|
| `build` | ✓ | - | - | - | Test buildu |
| `install` | ✓ | ✓ | ✓ | ✓ | Instalace |
| `--no-staging build` | - | - | - | - | Debug (riziko!) |

---

## 4. Databáze a Tracking

### 4.1 Installed Packages Database

#### `{root}/var/lib/balikar/installed.json`
**Umístění:** 
- Native systém: `/var/lib/balikar/installed.json`
- LFS chroot: `/mnt/lfs/var/lib/balikar/installed.json`
- Test prostředí: `/tmp/test_root/var/lib/balikar/installed.json`

**Typ:** Databáze nainstalovaných balíčků  
**Formát:** JSON  
**Modul:** `src/balikar/installer.c`  
**Kdy se vytváří:** 
- Automaticky při první instalaci do daného root
- Pomocí `ensure_db_dir()` → `mkdir -p {root}/var/lib/balikar`

**Kdy se čte:** 
- Při každé instalaci (kontrola existujících balíčků)
- Příkazy `balikar remove`, `balikar autoremove`
- Query operace

**Kdy se mění:**
- Instalace balíčku: `add_installed_package()`
- Odstranění balíčku: `erase_package()`
- Autoremove: Odstranění osiřelých závislostí

**Struktura:**
```json
{
  "packages": [
    {
      "name": "gzip",
      "version": "1.14",
      "build": "0",
      "install_date": "2025-12-01T14:30:00Z",
      "install_reason": "manual",
      "files": [
        "/usr/bin/gzip",
        "/usr/bin/gunzip",
        "/usr/bin/zcat",
        "/usr/share/man/man1/gzip.1"
      ]
    },
    {
      "name": "glibc",
      "version": "2.40",
      "build": "0",
      "install_date": "2025-12-01T14:29:00Z",
      "install_reason": "dependency",
      "files": [
        "/usr/lib/libc.so.6",
        "/usr/lib/libm.so.6",
        "/usr/include/stdio.h",
        "/usr/include/stdlib.h"
      ]
    },
    {
      "name": "bash",
      "version": "5.3",
      "build": "0",
      "install_date": "2025-12-01T14:31:00Z",
      "install_reason": "manual",
      "files": [
        "/usr/bin/bash",
        "/usr/bin/sh"
      ]
    }
  ]
}
```

**Popis polí:**
- `name`, `version`, `build` - Identifikace balíčku
- `install_date` - ISO 8601 timestamp instalace
- `install_reason` - **Důvod instalace:**
  - `"manual"` - Nainstalováno uživatelem přímo (např. `balikar install bash`)
  - `"dependency"` - Automaticky nainstalováno jako závislost
- `files` - **Kompletní seznam souborů** nainstalovaných tímto balíčkem
  - Používá se pro odstranění (erase)
  - Detekce konfliktů (budoucí feature)

**Tracking install_reason:**
```c
// V installer.c, add_installed_package()
// Je-li balíček explicitně požadován uživatelem
json_object_set_new(package, "install_reason", json_string("manual"));

// Je-li instalován jako závislost
json_object_set_new(package, "install_reason", json_string("dependency"));
```

**Použití install_reason pro autoremove:**
```c
// Najdi balíčky s install_reason="dependency"
// které nejsou potřeba žádným nainstalovaným balíčkem
// → označit jako osiřelé a nabídnout odstranění
```

**Relevantní funkce:**
- `load_installed_db()` - Načtení databáze
- `save_installed_db()` - Uložení změn
- `add_installed_package()` - Přidání záznamu
- `erase_package()` - Odstranění balíčku a souborů
- `is_package_installed()` - Kontrola instalace
- `get_installed_packages()` - Seznam všech balíčků

**Chroot awareness:**
```c
// Helpers pro multi-root support
static void get_db_dir_path(const char *chroot_path, char *buffer, size_t buffer_size);
static void get_db_file_path(const char *chroot_path, char *buffer, size_t buffer_size);
```

---

## 5. Build Artefakty a Dočasné Soubory a Dočasné Soubory

### 5.1 Build Logs

#### `build.log`
**Umístění:** `<build_tmp>/<balíček>/<source_dir>/build.log`  
**Příklad:** `/tmp/balikar_build_gzip_A3F8K2/gzip-1.14/build.log`  
**Typ:** Text log  
**Modul:** `src/balikar/builder.c`  
**Kdy se vytváří:** Během buildu (redirect stdout/stderr) v pracovním adresáři source_dir  
**Kdy se maže:** S build directory po dokončení

**Obsah:**
```
checking for gcc... gcc
checking whether the C compiler works... yes
checking for C compiler default output file name... a.out
...
make[1]: Entering directory '/tmp/balikar_build_gzip_A3F8K2/gzip-1.14'
gcc -O2 -Wall -c gzip.c
...
make install
/usr/bin/install -c gzip /tmp/staging/usr/bin/gzip
```

**Debug použití:**
```bash
# Build s debug logem
balikar -d -d build gzip

# Při selhání - log zůstává v tmp (před cleanup)
# Manuální kontrola:
cat /tmp/balikar_build_gzip_*/build.log
```

---

### 5.2 Build Artifacts

**Umístění:** `<build_tmp>/<balíček>/`  
**Obsah během buildu:**
```
/tmp/balikar_build_gcc_XYZ/gcc-15.2.0/
├── .build                   # Build skript
├── build.log                # Build output
├── configure                # Zdrojové soubory
├── Makefile
├── build/                   # Build directory (out-of-tree)
│   ├── *.o                  # Object soubory
│   ├── gcc                  # Compiled binary
│   └── ...
└── src/
    └── *.c
```

**Cleanup:**
```c
// V builder.c, commit_element() - cleanup na konci
char cleanup_cmd[PATH_MAX + 20];
snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf '%s'", build_dir);
system(cleanup_cmd);
```

---

## 6. Matice Operací

### 6.1 Operace × Soubory - Čtení

| Operace | repos.conf | repodata.json | index.json | installed.json | Cache | .build | .install/install.sh |
|---------|------------|---------------|------------|----------------|-------|--------|---------------------|
| `balikar install` | ✓ | ✓ | - | ✓ | ✓ | ✓ | ✓ |
| `balikar build` | ✓ | ✓ | - | - | ✓ | ✓ | - |
| `balikar resolve` | ✓ | ✓ | - | - | - | - | - |
| `balikar repos` | ✓ | ✓ | ✓ (mtime) | - | - | - | - |
| `balikar repo-update` | ✓ | - | ✓ | - | - | - | - |
| `balikar remove` | - | - | - | ✓ | - | - | - |
| `balikar autoremove` | - | - | - | ✓ | - | - | - |
| `balikar clean-cache` | - | - | - | - | ✓ | - | - |
| `balikar download` | ✓ | ✓ | - | - | ✓ | - | - |

---

### 6.2 Operace × Soubory - Zápis

| Operace | repos.conf | repodata.json | index.json | installed.json | Cache | Staging | Build tmp |
|---------|------------|---------------|------------|----------------|-------|---------|-----------|
| `balikar install` | - | - | - | ✓ | ✓ | ✓ | ✓ (temp) |
| `balikar build` | - | - | - | - | ✓ | - | ✓ (temp) |
| `balikar resolve` | - | - | - | - | - | - | - |
| `balikar repos` | - | ✓ (auto) | - | - | - | - | - |
| `balikar repo-update` | - | ✓ | - | - | ✓ | - | - |
| `balikar repo add` | ✓ | - | - | - | - | - | - |
| `balikar remove` | - | - | - | ✓ | - | - | - |
| `balikar autoremove` | - | - | - | ✓ | - | - | - |
| `balikar clean-cache` | - | - | - | - | ✓ (delete) | - | - |
| `balikar download` | - | - | - | - | ✓ | - | - |

---

### 6.3 Životní Cyklus Souborů

#### Repository Setup
```
1. Manuálně vytvořit: index.json pro každý balíček
2. CLI: balikar repo-update
   → Vytvoří/aktualizuje: repodata.json
3. CLI: balikar repo add testing <url> 80
   → Vytvoří/upraví: ~/.config/balikar/repos.conf
```

#### Build Process (pouze build, bez instalace)
```
1. CLI: balikar build gzip
2. Read: repos.conf → zjistit primary repo
3. Read: repodata.json → načíst metadata
4. Read: Cache → kontrola existence
5. Write: Cache → stažení tar.xz
6. Write: /tmp/balikar_build_gzip_XYZ/ → extrakce
7. Read: .build → spuštění build s DESTDIR=$P
8. Write: $P/ → výsledky buildu (např. $P/usr/bin/gzip)
9. Delete: /tmp/balikar_build_gzip_XYZ/ → cleanup build tmp
10. ŽÁDNÁ INSTALACE - staging zůstává v $P/ pro kontrolu
```

#### Install Process
```
1. CLI: balikar install bash
2. [Build process - viz výše]
3. Read: $P/ → seznam souborů
4. Write: /target_root/ → merge staging
5. Read: .install/install.sh → post-install (volitelné)
6. Write: /target_root/var/lib/balikar/installed.json → tracking
7. Optional: Delete $P/ → cleanup staging
```

#### Remove Process
```
1. CLI: balikar remove old_package
2. Read: /var/lib/balikar/installed.json → načíst seznam souborů
3. Delete: Všechny soubory z "files" pole
4. Write: /var/lib/balikar/installed.json → odstranit záznam
5. Output: Seznam osiřelých závislostí (install_reason="dependency")
```

#### Autoremove Process
```
1. CLI: balikar autoremove
2. Read: /var/lib/balikar/installed.json → najít balíčky s install_reason="dependency"
3. Analyze: Které nejsou potřeba žádným nainstalovaným balíčkem
4. User confirmation: Odstranit osiřelé?
5. Pro každý osiřelý:
   - Delete: Všechny soubory
   - Write: installed.json → odstranit záznam
```

---

### 6.4 Persistence × Dočasnost

#### Persistentní Soubory
- `~/.config/balikar/repos.conf`
- `/etc/balikar/repos.conf`
- `<repo>/repodata.json`
- `<repo>/<letter>/<name>/<name-version>/index.json`
- `/var/cache/balikar/downloads/*`
- `{root}/var/lib/balikar/installed.json`
- Staging `$P/` - persistentní až do:
  - Instalace (`balikar install` → cleanup po merge)
  - Manuálního smazání (`rm -rf $P/*`)
  - Pro `balikar build` - zůstává pro kontrolu

#### Dočasné Soubory (automaticky mazány)
- `/tmp/balikar_build_<pkg>_XXXXXX/` - Po každém buildu
- `build.log` - S build directory
- Build artifacts (*.o, compiled binaries před install) - S build directory

---

## 7. Souhrn Modulů

### Práce se Soubory podle Modulu

#### `src/common/config.c`
**Čte:**
- `~/.config/balikar/repos.conf`
- `/etc/balikar/repos.conf`

**Píše:**
- `~/.config/balikar/repos.conf`

**Operace:**
- Načítání konfigurace (INI parsing)
- Ukládání změn (INI writing)
- Správa repozitářů (add/remove/enable)

---

#### `src/balikar/parser.c`
**Čte:**
- `<repo>/repodata.json`
- `<repo>/<letter>/<name>/<name-version>/index.json`
- `<repo>/<letter>/<name>/<name-version>/info/index.json`
- Tar archivy (pro add_package_tar_bz2)

**Píše:**
- `<repo>/repodata.json` (generování)

**Operace:**
- Generování repodata.json ze všech index.json
- Načítání metadat do libsolv struktur
- Automatická detekce změn (mtime check)
- Parsování závislostí
- Custom pole (mkdeps, source, extras, is_group, no_package)

---

#### `src/balikar/downloader.c`
**Čte:**
- `/var/cache/balikar/downloads/*` (kontrola cache)
- Metadata: source[] pole (multi-source fallback)

**Píše:**
- `/var/cache/balikar/downloads/<name-version>.tar.xz` (stažené balíčky)
- `/var/cache/balikar/downloads/extras/<pkg>/` (patches)
- `/var/cache/balikar/downloads/repodata.json` (remote metadata)

**Operace:**
- Stahování balíčků (HTTP/HTTPS přes libcurl)
- Lokální kopírování (cp)
- Multi-source fallback (postupné zkoušení URL)
- Progress bar při stahování
- Stahování extras (patches)
- Cache management

---

#### `src/balikar/builder.c`
**Čte:**
- Tar archivy z cache
- `.build` skripty
- `index.json` (v extrahovaném archivu)

**Píše:**
- `/tmp/balikar_build_<pkg>_XXXXXX/` (extrakce)
- `$P/` nebo vlastní staging (DESTDIR)
- `build.log` (build output)

**Operace:**
- Extrakce tar archivů (auto-detect komprese)
- Hledání .build (rekurzivní až depth 3)
- Aplikace patches (extras)
- Spouštění build skriptů
- Nastavení environment (DESTDIR, MAKEFLAGS)
- Cleanup build directories

---

#### `src/balikar/installer.c`
**Čte:**
- `$P/` staging directory
- `.install/install.sh` (volitelné)
- `{root}/var/lib/balikar/installed.json`

**Píše:**
- `{root}/var/lib/balikar/installed.json` (tracking)
- `{root}/` target filesystem (merge staging)

**Operace:**
- Sběr file manifestu ze staging
- Merge staging → target root (cp -a)
- Spouštění post-install hooks
- Aktualizace databáze nainstalovaných balíčků
- Tracking install_reason (manual/dependency)
- Odstranění balíčků (erase)
- Autoremove (osiřelé závislosti)

---

## 7. CLI Příklady

### Rozdíly Mezi Klíčovými Příkazy

#### `balikar resolve <pkg>` vs `balikar build <pkg>` vs `balikar install <pkg>`

**`balikar resolve gzip`**
- **DRY-RUN** - žádné změny v systému
- Zobrazí dependency tree
- Použití: Zjistit co by se muselo nainstalovat
```bash
balikar resolve gcc
# Output: gcc → binutils, glibc, zlib, ...
# Žádné stahování, žádný build
```

**`balikar build gzip`**
- Build **bez instalace do systému**
- Stáhne → Build → Staging `$P/` → **Výsledky zůstávají v staging**
- **POUŽÍVÁ staging** ale neprovádí merge
- Použití: Testování buildu, verifikace výsledků před instalací
```bash
balikar build gzip
# Stáhne gzip-1.14.tar.xz
# Build v /tmp/balikar_build_gzip_XYZ/
# Výsledky v $P/usr/bin/gzip
# Pro kontrolu: ls $P/usr/bin/
```

**`balikar install gzip`**
- **Full workflow**: Resolve → Download → Build → Staging → Install
- Používá staging (`$P/`)
- Merge do systému + tracking
```bash
balikar install gzip
# Build → $P/usr/bin/gzip
# Merge → /usr/bin/gzip
# Tracking → /var/lib/balikar/installed.json
```

#### `balikar repos` vs `balikar repo-update`

**`balikar repos`**
- Zobrazí nakonfigurované repozitáře
- **Smart refresh**: Auto-regeneruje repodata.json **pouze pokud** nějaký index.json je novější (mtime check)
- Quick check stavu
```bash
balikar repos
# Output: Enabled repositories:
#         - ./repo (5 packages)
# Pokud změny v index.json:
# "Detected changes in index.json files – regenerating repodata.json..."
```

**`balikar repo-update`**
- **Force refresh**: Vynuceně regeneruje repodata.json **bez ohledu** na timestampy
- Stahuje fresh metadata z remote repozitářů
- Po úpravách nebo pro jistotu
```bash
balikar repo-update
# Output: Repository metadata updated successfully.
#         Repository reloaded: 5 packages found.
# Vždy regeneruje, ignoruje mtime
```

**Kdy použít co:**
- `balikar repos` - Běžná kontrola (rychlé, smart)
- `balikar repo-update` - Po manuálních změnách v repo (force)

---

### Základní Workflow
```bash
# 1. Nastavení repozitáře
balikar repo add official https://raw.githubusercontent.com/user/repo/main 100

# 2. Aktualizace metadat (force refresh)
balikar repo-update

# 3. Zobrazení repozitářů (s auto-refresh pokud změny)
balikar repos

# 4. Analýza závislostí (dry-run)
balikar resolve gzip
# Zobrazí: dependency tree bez změn

# 5. Build balíčku (bez instalace do systému)
balikar build gzip
# Vytvoří: /var/cache/balikar/downloads/gzip-1.14.tar.xz
# Extrahuje do: /tmp/balikar_build_gzip_XYZ/
# Build do staging: $P/usr/bin/gzip
# Výsledky zůstávají v $P/ pro verifikaci

# 6. Instalace (full workflow)
balikar install gzip
# Build → Staging $P/ → Merge do /usr/
# Tracking: /var/lib/balikar/installed.json

# 7. Kontrola instalace
cat /var/lib/balikar/installed.json | jq '.packages[] | select(.name=="gzip")'

# 8. Odstranění
balikar remove gzip

# 9. Autoremove osiřelých závislostí
balikar autoremove
```

### Pokročilé Použití
```bash
# Build do vlastního staging
balikar --staging-dir=/mnt/lfs/stage build gcc

# Instalace do chroot
balikar install glibc --root=/mnt/lfs

# Build do izolovaného tmp
balikar --build-tmp-dir=/var/tmp/isolated build glibc

# Přímá instalace bez staging
balikar --no-staging build hello

# Vyčištění cache
balikar clean-cache
balikar clean-cache --pkg=gcc  # Pouze gcc

# Download bez buildu
balikar download bash
balikar download --output-dir=/sources --no-deps gcc
```

---

## 8. Závěr

Tento dokument poskytuje kompletní přehled všech souborů, které Balikar vytváří, čte a upravuje během svého provozu:

- **Konfigurace:** `repos.conf` (uživatelská a systémová)
- **Metadata:** `repodata.json`, `index.json`
- **Cache:** `/var/cache/balikar/downloads/`
- **Databáze:** `installed.json` (chroot-aware)
- **Balíčky:** `.tar.{bz2,gz,xz}`, `.build`, `.install/install.sh`
- **Staging:** `$P/` - bezpečnostní vrstva mezi buildem a systémem
- **Dočasné:** Build directories

Dokumentace je synchronizována s aktuálním kódem a pokrývá všechny nuance včetně:
- Multi-source fallback pro vysokou dostupnost
- Install reason tracking pro autoremove
- Chroot-aware databáze pro multi-root support
- Automatická detekce změn v metadatech

Pro podrobnosti o jednotlivých modulech viz:
- [parser.txt](parser.txt)
- [downloader.txt](downloader.txt)
- [builder.txt](builder.txt)
- [installer.txt](installer.txt)
- [dependency_tracking.txt](dependency_tracking.txt)
- [README.md](README.md)
