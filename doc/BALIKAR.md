# Balikar Package Manager – Komplexní dokumentace

## Obsah

1. [Přehled projektu](#1-přehled-projektu)
2. [Architektura a workflow](#2-architektura-a-workflow)
3. [Sestavení (build)](#3-sestavení-build)
   - 3.1 [Dynamické sestavení](#31-dynamické-sestavení)
   - 3.2 [Statické sestavení – Přístup 1: Conda](#32-statické-sestavení--přístup-1-conda)
   - 3.3 [Statické sestavení – Přístup 2: Manuální bez Condy](#33-statické-sestavení--přístup-2-manuální-bez-condy)
   - 3.4 [Statické sestavení – Přístup 3: Alpine Docker](#34-statické-sestavení--přístup-3-alpine-docker)
   - 3.5 [Porovnání metod sestavení](#35-porovnání-metod-sestavení)
4. [Distribuce a instalace](#4-distribuce-a-instalace)
5. [Příkazy](#5-příkazy)
6. [Repozitáře a konfigurace](#6-repozitáře-a-konfigurace)
7. [Soubory a metadata](#7-soubory-a-metadata)
8. [Dependency tracking](#8-dependency-tracking)
9. [Testování – výsledky](#9-testování--výsledky)
10. [Architektura kódu – refaktoring](#10-architektura-kódu--refaktoring)

---

## 1. Přehled projektu

Balikar je package manager napsaný v C, postavený na knihovně **libsolv**. Automatizuje celý workflow *build-from-source* pro LFS/LLD styly distribuce. Podporuje dependency resolution, stahování zdrojů, aplikaci patchů, build do stagingu a instalaci.

**Klíčové technologie:**
- `libsolv` (s `ENABLE_CONDA=ON`) – dependency resolution
- `libcurl` – stahování zdrojů
- `libjansson` – parsování JSON metadat
- `libarchive` – práce s tar archivy
- `CMake` – build systém

---

## 2. Architektura a workflow

### Fáze zpracování

```
1. PARSE    → Načtení repository (repodata.json) a solvables
2. SOLVE    → Dependency resolution včetně makedeps rekurze
3. DOWNLOAD → Stažení tar.bz2 archivů a extras (patches) s progress barem
4. BUILD    → Extrakce, aplikace patches, spuštění .build skriptu
5. INSTALL  → Merge staging $P do chroot prostředí
```

### Skupiny příkazů

| Skupina | Příkazy | Účel |
|---|---|---|
| Sestavovací | `install`, `build`, `download` | Stahování, kompilace a instalace balíčků |
| Informační | `resolve`, `repos`, `repo-update` | Zobrazení závislostí, stavu repozitářů (bez změn) |
| Správa systému | `remove`, `autoremove`, `clean-cache` | Odinstalace, úklid osiřelých závislostí a download cache |

### Komponenty

#### Parser (`parser.c`)
Načítá `repodata.json` a plní libsolv Pool solvables. Klíčové funkce:
- `load_repository(Pool *pool, const char *repo_path)` – načtení repozitáře
- `update_repository_metadata(const char *repo_path)` – aktualizace metadat
- `get_custom_source_array(Repodata*, Id, Pool*)` – pole zdrojových URL
- `get_custom_mkdeps(Repodata*, Id, Pool*)` – build-time závislosti

#### Solver (`solver.c`)
High-level rozhraní pro libsolv. Řeší runtime i build-time závislosti (mkdeps) s ochranou proti cyklům (max. hloubka = 10). Klíčové funkce:
- `create_solver(Pool *pool)` – vytvoření solveru s flagi (ALLOW_UNINSTALL=1, ALLOW_VENDORCHANGE=1, ALLOW_DOWNGRADE=0)
- `solve_dependencies(Solver*, const char *pkg_name, Pool*, Repo*)` – hlavní entry point
- `add_mkdeps_to_queue(Queue*, Id, Pool*, Repo*, int depth)` – rekurzivní přidání build-time závislostí

#### Downloader (`downloader.c`)
Stahuje zdrojové archivy a extras (patches) s progress barem. Podporuje fallback přes více `source[]` URL – při selhání jedné URL zkouší další v pořadí. Výsledky cachuje v `/var/cache/balikar/`.

#### Builder (`builder.c`)
Zpracovává stažené archivy. Workflow:
1. Detekce komprese z magic bytes (gzip, xz, bzip2, plain tar)
2. Extrakce do dočasného adresáře (`mkdtemp`)
3. Rekurzivní vyhledání `.build` skriptu (max. hloubka 3)
4. Aplikace patches z extras cache
5. Nastavení prostředí (`DESTDIR=$P`, `MAKEFLAGS=-j$(nproc)`)
6. Spuštění `bash -e .build` nebo fallback `configure && make && make install`
7. Cleanup dočasného adresáře

Konfigurace builderu:
- `set_staging_enabled(int)` – zapnutí/vypnutí staging režimu
- `set_staging_dir(const char*)` – vlastní staging adresář (výchozí `$P`)
- `set_build_tmp_dir(const char*)` – vlastní tmp pro build extrakci

#### Installer (`installer.c`)
Merguje výsledky buildu ze staging directory (`$P`) do cílového root adresáře. Spravuje package tracking databázi (`installed.json`), spouští volitelné post-install hooky (`.install/install.sh`) a zaznamenává file manifest pro odinstalaci.

**Dvě klíčová prostředí:**
- **Staging directory (`$P`)** – dočasná oblast pro výsledky buildu (`DESTDIR=$P make install`); izoluje build od systému
- **Target root** – finální cíl (výchozí `/`, nebo `--root=/mnt/lfs`)

#### Command Pattern (`command_context.c`, `command_registry.c`)
Dispatcher architektura:
- `CommandContext` – centrální stav (parsed CLI args, lazy-loaded pool/repo, subsystem facade)
- `CommandRegistry` – statická tabulka 10 příkazů; O(n) lookup
- `main.c` – 47 řádků, pouze dispatcher (96% redukce z 1157 řádků)

---

## 3. Sestavení (build)

### 3.1 Dynamické sestavení

Standardní sestavení vytvoří binárku (~328 KB) závislou na sdílených knihovnách.

#### Prerekvizity

**Debian/Ubuntu:**
```bash
sudo apt install build-essential cmake git \
    libcurl4-openssl-dev libjansson-dev libarchive-dev \
    libssl-dev zlib1g-dev liblzma-dev libbz2-dev libzstd-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc cmake git libcurl-devel jansson-devel \
    libarchive-devel openssl-devel zlib-devel xz-devel bzip2-devel libzstd-devel
```

#### libsolv (nutno sestavit ručně s `ENABLE_CONDA=ON`)

```bash
cd /tmp
git clone https://github.com/openSUSE/libsolv.git
cd libsolv && mkdir build && cd build
cmake .. -DENABLE_CONDA=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

#### Sestavení balikar

```bash
cd /path/to/proj6_balikar
cmake -B build -DCONDA_PREFIX=$CONDA_PREFIX   # nebo -DCONDA_PREFIX=/usr/local
cmake --build build
```

Výsledná binárka: `build/balikar`

---

### 3.2 Statické sestavení – Přístup 1: Conda

Sestavení statické binárky pomocí Conda prostředí s předinstalovanou libsolv. Výsledek: **~7.0 MB**, glibc staticky, `not a dynamic executable`.

#### Prerekvizity

- Nainstalovaná Conda (Miniconda/Anaconda)
- Conda prostředí s libsolv: `conda install -c conda-forge libsolv`
- Systémová statická libcurl (viz poznámka níže)

> **Poznámka k libcurl na Debianu:** Systémová `libcurl.a` na Debianu obsahuje GSSAPI/LDAP → je nutné sestavit minimální libcurl ze zdrojů:
> ```bash
> wget https://curl.se/download/curl-7.88.1.tar.xz
> tar xJf curl-7.88.1.tar.xz && cd curl-7.88.1
> ./configure --prefix=$CONDA_PREFIX --disable-shared --enable-static \
>     --disable-ldap --without-gssapi --without-librtmp \
>     --disable-ldaps --without-ntlm --without-ntlm-wb
> make -j$(nproc) && make install
> ```

#### Sestavení

```bash
cd /path/to/proj6_balikar
# CONDA_PREFIX musí být nastaven (např. /home/user/miniconda3)
bash build_static.sh
```

Výsledná statická binárka: `./balikar-static`

#### Ověření

```bash
ldd ./balikar-static     # → not a dynamic executable
file ./balikar-static    # → statically linked
./balikar-static --help
```

---

### 3.3 Statické sestavení – Přístup 2: Manuální bez Condy

Sestavení bez Conda prostředí – libsolv a libcurl jsou nainstalovány do vlastního prefixu. Výsledek: **~7.1 MB**, glibc staticky.

#### Prerekvizity

1. Sestavit statickou libsolv:
```bash
cd /tmp
git clone https://github.com/openSUSE/libsolv.git
cd libsolv && mkdir build && cd build
cmake .. -DENABLE_STATIC=ON -DENABLE_CONDA=ON \
    -DCMAKE_INSTALL_PREFIX=/custom/prefix
make -j$(nproc) && make install
```

2. Sestavit minimální statickou libcurl (stejný postup jako v 3.2, s `--prefix=/custom/prefix`)

#### Sestavení

```bash
# Nastavit CONDA_PREFIX na vlastní prefix
CONDA_PREFIX=/custom/prefix bash build_static.sh

# Nebo použít LIBSOLV_PREFIX
LIBSOLV_PREFIX=/custom/prefix bash build_static.sh
```

#### Manuální cmake příkaz

```bash
cmake -B build-static \
    -DBUILD_STATIC=ON \
    -DCONDA_PREFIX=/custom/prefix \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-static --target balikar
```

---

### 3.4 Statické sestavení – Přístup 3: Alpine Docker

Sestavení v Alpine Linux kontejneru s musl libc – maximální přenositelnost. Výsledek: **~24.4 MB**, musl libc staticky, funguje na libovolné Linuxové distribuci.

#### Prerekvizity

```bash
sudo apt install docker.io   # Debian/Ubuntu
sudo dnf install docker      # Fedora/RHEL

# Přidat uživatele do skupiny docker (nutný relogin)
sudo usermod -aG docker $USER
newgrp docker
```

#### Sestavení

```bash
cd /path/to/proj6_balikar

# Rychlé sestavení přes skript
./build_static_alpine.sh

# Nebo ručně přes docker
docker build -f tests/docker-tests/alpine/Dockerfile.alpine \
    -t balikar-static-builder .

# Extrakce binárky z image
docker run --rm balikar-static-builder \
    sh -c "cat /output/balikar" > balikar-static-alpine
chmod +x balikar-static-alpine
```

#### Ověření

```bash
file balikar-static-alpine   # → Not a valid dynamic program (musl static)
ldd balikar-static-alpine    # → Not a valid dynamic program
./balikar-static-alpine --help
```

#### Výhody Alpine/musl metody

- **100% přenositelné** – funguje na Debian, Ubuntu, Fedora, Arch, apod.
- **Žádné problémy s NSS** – musl libc je navržena pro statické linkování
- **Reprodukovatelné** – stejné prostředí pokaždé
- **DNS funguje** – na rozdíl od glibc statické binárky

#### Dockerfile – klíčové cmake parametry

libsolv musí být sestavena s:
```
-DENABLE_STATIC=ON -DENABLE_CONDA=ON
```
(nikoli `-DBUILD_SHARED_LIBS=OFF` samotné – nestačí)

Potřebné Alpine balíčky pro statické linkování:
```
c-ares-dev brotli-dev brotli-static libpsl-dev libpsl-static
libidn2-dev libidn2-static libunistring-dev libunistring-static
```

---

### 3.5 Porovnání metod sestavení

| Metoda | Výsledná velikost | Libc | Přenositelnost | Doporučení |
|---|---|---|---|---|
| **Dynamická** | ~328 KB + ~1 MB libs | glibc | Stejná glibc verze | Vývoj |
| **Statická – Conda** | ~7.0 MB | glibc staticky | Stejná glibc verze | Nativní build s Condou |
| **Statická – Manuální** | ~7.1 MB | glibc staticky | Stejná glibc verze | Bez Condy |
| **Statická – Alpine Docker** | ~24.4 MB | musl staticky | Libovolná Linux distribuce | Release/distribuce |

### CMake parametry

| Parametr | Hodnota | Popis |
|---|---|---|
| `BUILD_STATIC` | `ON`/`OFF` | Statické vs. dynamické linkování |
| `CONDA_PREFIX` | cesta | Prefix pro hledání statických knihoven |
| `LIBSOLV_PREFIX` | cesta | Alternativní prefix pro libsolv (bez Condy) |

---

## 4. Distribuce a instalace

### Struktura distribučního balíčku (dynamická binárka)

```
proj6_balikar/
├── balikar                   # Pre-built binárka (~328 KB)
├── lib/                      # Bundled sdílené knihovny (~1 MB)
│   ├── libsolv.so.1          # Conda libsolv s conda podporou
│   ├── libsolvext.so.1
│   ├── libz.so.1
│   └── libpcre2-posix.so.3
├── install_prebuilt.sh       # Instalační skript (pre-built binárka)
└── install_notbuilt.sh       # Build ze zdrojů, pak instalace
```

> Bundlované knihovny jsou nutné, protože balikar vyžaduje **conda's libsolv** (sestavenou s `ENABLE_CONDA=ON`), která není dostupná v systémových balíčcích.

### Prerekvizity pro instalaci

**Debian/Ubuntu:**
```bash
sudo apt install libcurl4 libjansson4 libarchive13 patchelf bzip2 xz gzip
```

**Fedora/RHEL:**
```bash
sudo dnf install libcurl jansson libarchive patchelf bzip2 xz gzip
```

### Systémová instalace (vyžaduje sudo)

```bash
git clone <repo-url>
cd proj6_balikar
sudo sh install_prebuilt.sh
```

Nainstaluje:
- Binárka: `/usr/local/bin/balikar`
- Knihovny: `/usr/local/lib/balikar/`
- Konfigurace: `/etc/balikar/`

### Uživatelská instalace (bez sudo)

```bash
sh install_prebuilt.sh
```

Nainstaluje:
- Binárka: `~/.local/bin/balikar`
- Knihovny: `~/.local/lib/balikar/`
- Konfigurace: `~/.config/balikar/`

### Build a příprava nové distribuce (pro vývojáře)

```bash
conda activate balikar   # prostředí s libsolv
sh build_for_distribution.sh
```

Vytvoří:
- `./balikar` – binárka pro git commit
- `./lib/` – knihovny pro git commit
- `dist/` – kompletní balíček (není commitován)

### Kompatibilita

**Podporováno:** Debian 11/12, Ubuntu 20.04–24.04, ostatní glibc-based distribuce.  
**Pro statickou binárku (Alpine Docker):** libovolná Linux distribuce.  
**Nepodporováno (dynamická binárka):** Alpine Linux (musl libc).

### Instalace statické binárky

```bash
# Zkopírovat jediný soubor – žádné závislosti, žádný lib/ adresář
sudo cp balikar-static /usr/local/bin/balikar
sudo chmod +x /usr/local/bin/balikar
```

---

## 5. Příkazy

### Syntaxe

```
balikar [OPTIONS] COMMAND [ARGS]
```

### Kompatibilita přepínačů s příkazy

| Přepínač | install | build | download | resolve | remove | autoremove | clean-cache |
|---|---|---|---|---|---|---|---|
| `-d` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| `--dry-run` | ✓ | ✓ | – | – | ✓ | ✓ | – |
| `--no-staging` | ✓ | ✓ | – | – | – | – | – |
| `--staging-dir=PATH` | ✓ | ✓ | – | – | – | – | – |
| `--root=PATH` | ✓ | – | – | – | ✓ | ✓ | – |
| `--build-tmp-dir=PATH` | ✓ | ✓ | – | – | – | – | – |
| `--repo=PATH` | ✓ | ✓ | ✓ | ✓ | – | – | – |
| `--local-package=PATH` | ✓ | ✓ | – | – | – | – | – |
| `--output-dir=PATH` | – | – | ✓ | – | – | – | – |
| `--no-deps` | – | – | ✓ | – | – | – | – |
| `--pkg=NAME` | – | – | – | – | – | – | ✓ |

`repos` a `repo-update` přijímají pouze `-d`. `-h`/`--help` je globální.

---

### install

Kompletní workflow: resolve dependencies → download → build → install.

```bash
# Instalace do systémového rootu
sudo balikar install gzip

# Instalace do LFS prostředí
balikar --staging-dir=/mnt/lfs/stage --root=/mnt/lfs install glibc

# Dry-run (zobrazí plán bez akce)
balikar install bash --dry-run
```

Ukázkový výstup:
```
[INFO] Loading repository: ./repo/repodata.json
[INFO] Resolving dependencies for: gzip
[INFO] Transaction plan:
       Install: gzip-1.14-1 (x86_64)
[INFO] Downloading: gzip-1.14.tar.xz
Downloading gzip-1.14.tar.xz [============================] 100% (1.0MB/1.0MB)
[INFO] Build successful: gzip-1.14
[INFO] Merging staging directory to /
[INFO] Package installed: gzip-1.14
[INFO] Tracking database updated: /var/lib/balikar/installed.json
```

---

### build

Shodný workflow jako `install`, ale merge do systému neproběhne. Výsledek zůstane v staging adresáři pro manuální verifikaci.

```bash
balikar build hello

# Přímá instalace bez DESTDIR mezikroku (pouze v disposable prostředí)
balikar --no-staging build hello
```

---

### resolve

Zobrazí dependency tree a pořadí instalace bez jakýchkoliv změn v systému.

```bash
balikar resolve bash
```

Výstup:
```
[INFO] Transaction plan:
       Install: ncurses-6.5-1 (x86_64)
       Install: readline-8.2-1 (x86_64)
       Install: bash-5.3-1 (x86_64)
Total: 3 package(s) to install
```

---

### download

Stáhne balíček (a volitelně jeho závislosti) bez instalace.

```bash
balikar download bash
balikar download bash --no-deps          # jen samotný balíček
balikar download bash --output-dir=/tmp  # vlastní výstupní adresář
```

---

### remove

Odstraní balíček ze systému. Kontroluje reverse dependencies – pokud na něm jiné balíčky závisí, zobrazí varování a vyžádá si potvrzení (výchozí: `n`).

```bash
balikar remove vim
balikar remove readline --root=/mnt/lfs
```

Po úspěšném odebrání zobrazí osiřelé závislosti:
```
[INFO] Removed 847 files (0 failed)
[INFO] Successfully erased package: vim
[INFO] The following packages are no longer required:
[INFO]   ncurses
[INFO] Use 'balikar autoremove' to remove them.
```

---

### autoremove

Automaticky vyhledá a odstraní osiřelé závislosti (balíčky s `install_reason="dependency"`, na které nic nespoléhá).

```bash
balikar autoremove
balikar autoremove --root=/mnt/lfs
```

---

### repos

Zobrazí nakonfigurované repozitáře.

```bash
balikar repos
```

Výstup:
```
Configured repositories:
NAME                 PRIORITY   ENABLED    URL
------------------------------------------------------------------------
balikar-official     100        yes        https://raw.githubusercontent.com/...
local-dev            50         yes        ./repo

Currently using: https://raw.githubusercontent.com/...
Packages available: 11
```

---

### repo-update

Aktualizuje metadata repozitáře.

```bash
balikar repo-update
balikar repo-update --repo=./repo
balikar repo-update --repo=https://example.com/packages
```

- **Remote repozitáře** (http/https): stáhne `repodata.json` do cache
- **Lokální repozitáře**: vygeneruje `repodata.json` ze všech `index.json` souborů

---

### clean-cache

Vyčistí stažené soubory z download cache.

```bash
balikar clean-cache
balikar clean-cache --pkg=gzip   # jen konkrétní balíček
```

---

### repo (subpříkazy)

```bash
balikar repo list                              # seznam repozitářů
balikar repo add testing https://test.org 80   # přidat repozitář
balikar repo remove testing                    # odebrat repozitář
balikar repo enable testing                    # aktivovat repozitář
balikar repo disable production                # deaktivovat repozitář
```

---

## 6. Repozitáře a konfigurace

### Hierarchie konfiguračních souborů

1. `~/.config/balikar/repos.conf` (nejvyšší priorita)
2. `/etc/balikar/repos.conf` (fallback)
3. Hardcoded výchozí: `./repo` s prioritou 10

### Formát konfiguračního souboru (INI)

```ini
[balikar-official]
url = https://raw.githubusercontent.com/JanPL00/balikar-repository/main
priority = 100
enabled = true

[local-dev]
url = ./repo
priority = 50
enabled = true
```

### Doporučené priority

| Rozsah | Typ repozitáře |
|---|---|
| 150+ | Lokální development |
| 100 | Production remote |
| 50–90 | Testing/staging |
| 10–40 | Fallback |
| 10 | Výchozí `./repo` |

### Formát metadat balíčku (`index.json`)

```json
{
  "name": "bash",
  "version": "5.3",
  "arch": "x86_64",
  "depends": ["glibc", "readline"],
  "mkdeps": [],
  "license": "GPLv3+",
  "source": [
    { "url": "https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz" },
    { "url": "https://mirror.example.com/bash-5.3.tar.gz" }
  ],
  "extras": [],
  "is_group": false,
  "no_package": false
}
```

Pole `source[]` podporuje více mirror URL – downloader zkouší URL postupně.

### Struktura repozitáře na disku

```
myrepo/
├── repodata.json
├── b/
│   └── bash/
│       └── bash-5.3/
│           ├── index.json
│           ├── bash-5.3.tar.bz2   (volitelný)
│           ├── .build
│           └── .install/
│               └── install.sh
└── g/
    └── gzip/
        └── gzip-1.14/
            ├── index.json
            └── .build
```

### Workflow stahování s fallbackem

```
Parser → source[0].url → Selže? → source[1].url → Selže? → source[2].url → ...
                                                              ↓
                                                     Cache: /var/cache/balikar/
```

Ukázkové logy při fallbacku:
```
[WARN] Download failed from source[0]: Connection failed
[INFO] Trying source[1].url: https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz
[INFO] Successfully downloaded from source[1]
```

---

## 7. Soubory a metadata

### Konfigurační soubory

| Soubor | Modul | Popis |
|---|---|---|
| `~/.config/balikar/repos.conf` | `config.c` | Uživatelská konfigurace repozitářů |
| `/etc/balikar/repos.conf` | `config.c` | Systémová konfigurace (fallback) |

### Metadata repozitáře

| Soubor | Modul | Popis |
|---|---|---|
| `<repo>/repodata.json` | `parser.c` | Agregovaná metadata všech balíčků v repozitáři |
| `<repo>/<l>/<pkg>/<ver>/index.json` | `parser.c` | Metadata jednotlivého balíčku |
| `/var/cache/balikar/downloads/repodata.json` | `downloader.c` | Cache remote `repodata.json` |

### Cache a dočasné soubory

| Cesta | Popis |
|---|---|
| `/var/cache/balikar/downloads/<pkg>.tar.*` | Stažené zdrojové archivy |
| `/var/cache/balikar/extras/<pkg>/` | Stažené patches/extras |
| `/tmp/balikar_build_<pkg>_XXXXXX/` | Dočasný build adresář (mazán po buildu) |
| `$P` nebo `--staging-dir` | Staging directory (výsledky buildu před instalací) |

### Databáze nainstalovaných balíčků

| Cesta | Popis |
|---|---|
| `/var/lib/balikar/installed.json` | Databáze (systémová instalace) |
| `<root>/var/lib/balikar/installed.json` | Databáze (LFS prostředí) |

Příklad záznamu v databázi:
```json
{
  "packages": {
    "bash-5.3": {
      "name": "bash",
      "version": "5.3",
      "install_date": "2025-11-30T18:30:00Z",
      "install_reason": "manual",
      "files": ["/usr/bin/bash", "/usr/share/man/man1/bash.1"]
    }
  }
}
```

---

## 8. Dependency tracking

### `install_reason`

Každý nainstalovaný balíček má pole `install_reason`:
- `"manual"` – balíček instaloval přímo uživatel
- `"dependency"` – balíček byl instalován automaticky jako závislost

### Workflow `remove`

1. **Kontrola reverse dependencies** – pokud jiné balíčky závisí na odstraňovaném, zobrazí varování (výchozí potvrzení: `n`)
2. **Odebrání souborů** podle file manifestu
3. **Detekce osiřelých závislostí** – balíčky s `install_reason="dependency"`, na které nic nespoléhá

### Algoritmus `autoremove`

1. Načte `installed.json`
2. Vybere balíčky s `install_reason="dependency"` jako kandidáty
3. Pro každý kandidát ověří, zda ho některý nainstalovaný balíček vyžaduje
4. Zobrazí seznam osiřelých a vyžádá potvrzení
5. Iterativně odstraní (nová kola mohou odhalit další osiřelé)

Příklad:
```
$ balikar remove bash
[INFO] Removed 162 files (0 failed)
[INFO] Successfully erased package: bash
[INFO] The following packages were installed as dependencies and are no longer required:
[INFO]   readline
[INFO] Use 'balikar autoremove' to remove them.

$ balikar autoremove
[INFO] Found 1 orphaned package(s):
[INFO]   - readline
Remove these packages? [y]: y
[INFO] Removed 4082 files (0 failed)
```

---

## 9. Testování – výsledky

**Datum testování:** 2026-04-09  
**Verze:** Po refaktoringu (Command Pattern)

### Funkční testování (11/15 příkazů – 73%)

| Příkaz | Výsledek | Poznámka |
|---|---|---|
| `help` / `--help` / `-h` | PASS | |
| `repos` | PASS | |
| `repo list` | PASS | |
| `repo add/remove/enable/disable` | PARTIAL | Vyžaduje argc/argv v Context |
| `repo-update` | PASS | |
| `resolve <pkg>` | PASS | |
| `install <pkg>` | PASS | |
| `build <pkg>` | PASS | |
| `remove <pkg>` | PASS | |
| `autoremove` | PASS | |
| `download <pkg>` | PASS | |
| `clean-cache` | PASS | |

### Validace a error handling (100%)

```
install bez balíčku → "Package name required for install/build command"
Neznámý příkaz → "[ERROR] Unknown command: <name>"
Neplatná volba → "unrecognized option '--<name>'"
repo bez subcommandu → "Subcommand required for 'repo'"
```

### Správa paměti (Valgrind)

```
total heap usage: 1,293 allocs, 1,292 frees, 117,395 bytes allocated
definitely lost: 0 bytes in 0 blocks
ERROR SUMMARY: 0 errors from 0 contexts
```
**Výsledek:** Žádné memory leaky

### Performance

| Metrika | Hodnota |
|---|---|
| Startup time (průměr 10 běhů) | 6 ms |
| Command dispatch overhead | < 1 ms |
| Binárka (dynamická) | 442 KB |

### Unit testy

```
Running suite(s): Parser Tests  Solver Tests
100%: Checks: 10, Failures: 0, Errors: 0
```

### Metriky kódu

| Metrika | Hodnota |
|---|---|
| `main.c` před refaktoringem | 1157 řádků |
| `main.c` po refaktoringu | 47 řádků (−96 %) |
| Cyklomatická složitost `main()` | ~50 → ~5 |
| Počet modulů příkazů | 15 souborů |
| Valgrind definitivní leaky | 0 bytes |

---

## 10. Architektura kódu – refaktoring

### Přechod na Command Pattern

Projekt prošel refaktoringem z monolitického `main.c` na modulární Command Pattern.

#### Milník 1 – Základní infrastruktura

Vytvořeny:
- `command_context.h/c` – `CommandContext` se stavem CLI, lazy-loaded pool/repo a subsystem facade; lifecycle API (`create`/`destroy`/`ensure_*`)
- `command_registry.h/c` – statická tabulka 10 příkazů, `command_lookup()` (O(n)), `command_dispatch()` (find+validate+execute)

Memory footprint: < 2 KB overhead (Command registry ~1 KB + Context ~500 B).

#### Milník 2 – Jednoduché příkazy (6 modulů)

`cmd_repos.c`, `cmd_autoremove.c`, `cmd_remove.c`, `cmd_resolve.c`, `cmd_clean_cache.c`, `cmd_repo_update.c`

Vzor: lazy-loading pool/repo pouze tehdy, kdy je skutečně potřeba.

#### Milník 3 – Komplexní příkazy (4 moduly)

`cmd_install.c` (sdílí logiku s `build` přes `cmd_install_or_build_execute(ctx, do_install)`), `cmd_download.c`, `cmd_repo.c`

Extrahovány ~700 řádků z monolitického `main.c`.

#### Milník 4 – Integrace dispatcheru

`main.c` redukován z 1157 → 47 řádků:
```c
CommandContext *ctx = command_context_create();
command_context_parse_args(ctx, argc, argv);
int exit_code = command_dispatch(ctx);
command_context_destroy(ctx);
return exit_code;
```

#### Milník 5 – Testování

Všechny metriky splněny – viz [kapitola 9](#9-testování--výsledky).

### Design patterny

| Pattern | Implementace | Benefit |
|---|---|---|
| **Command Pattern** | 10 command modulů s uniform interface | Testovatelnost, rozšiřitelnost |
| **Dispatcher Pattern** | `command_lookup()` + `command_dispatch()` | Eliminace obřího switch/if-else |
| **Context Pattern** | `CommandContext` jako single state container | Žádné globální proměnné |
| **Facade Pattern** | `ctx->subsystems.*` | Dependency injection, mock-friendly |
| **Lazy Loading** | Pool/repo alokováno jen při potřebě | Úspora RAM (5–10 MB) |

### Organizace zdrojových souborů

```
src/
├── balikar/
│   ├── main.c                    # 47 řádků – dispatcher
│   ├── command_context.c         # Context lifecycle
│   ├── command_registry.c        # Registry + dispatch
│   ├── builder.c                 # BUILD fáze
│   ├── downloader.c              # DOWNLOAD fáze
│   ├── installer.c               # INSTALL fáze
│   ├── parser.c                  # PARSE fáze
│   ├── solver.c                  # SOLVE fáze
│   ├── commands/
│   │   ├── cmd_install.c         # install + build
│   │   ├── cmd_download.c        # download
│   │   ├── cmd_resolve.c         # resolve
│   │   ├── cmd_remove.c          # remove
│   │   ├── cmd_autoremove.c      # autoremove
│   │   ├── cmd_repos.c           # repos
│   │   ├── cmd_repo.c            # repo subpříkazy
│   │   ├── cmd_repo_update.c     # repo-update
│   │   └── cmd_clean_cache.c     # clean-cache
│   └── includes/
│       ├── command_context.h
│       ├── command_registry.h
│       └── command.h
├── common/
│   ├── common.c / common.h       # Sdílené typy, log_*, error codes
│   ├── config.c / config.h       # Správa konfigurace repozitářů
└── utils/
    ├── cli_helpers.c             # user_confirm(), CLI utilities
    ├── print.c                   # display_transaction(), progress bar
    ├── string_helpers.c          # String utilities
    └── system_helpers.c          # set_system_arch(), systémové helpers
```
