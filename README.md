# Balikar – správce balíčků

Správce balíčků sestavující ze zdrojových kódů, inspirovaný projektem Linux From Scratch (LFS), využívající libsolv pro řešení závislostí.

## Funkce

- **Řešení závislostí** pomocí libsolv (stejný engine jako zypper, dnf)
- **Sestavení ze zdrojů** (LFS styl)
- **Vzdálené repozitáře** (GitHub, HTTP/HTTPS)
- **Více repozitářů** s prioritami
- **Staging prostředí** pro bezpečné sestavení ($DESTDIR)
- **Vlastní build skripty** (.build) a install hooky (.install)

## Rychlý start

### Možnosti instalace

#### Možnost 1: Předkompilovaná binárka (s přibalenou knihovnou)

**Požadavky – systémové balíky:**
```bash
# Debian/Ubuntu
sudo apt install libcurl4 libjansson4 libarchive13 patchelf

# Fedora/RHEL
sudo dnf install libcurl jansson libarchive patchelf
```

**Instalace Balikaru:**
```bash
# Klonuj repozitář
git clone https://gitlab.fel.cvut.cz/placeja5/proj6_balikar.git
cd proj6_balikar

# Systémová instalace (doporučeno)
sudo sh run_scripts/install/install_prebuilt.sh

# nebo instalace pro aktuálního uživatele
sh run_scripts/install/install_prebuilt.sh
```

#### Možnost 2: Statická binárka (jediný soubor, funguje kdekoli)

**Sestavení statické binárky pomocí Alpine Docker (doporučeno):**
```bash
# Vyžaduje pouze nainstalovaný Docker
./run_scripts/build/build_static_alpine.sh

# Instalace
sudo cp balikar-static /usr/local/bin/balikar
```

**Nebo nativní sestavení (vyžaduje statické knihovny):**
```bash
# Viz doc/STATIC_BUILD.md pro podrobné instrukce
./run_scripts/build/build_static.sh
```

**Výhody statické binárky:**
- Jediný soubor, žádné závislosti
- Funguje na jakékoli linuxové distribuci
- Není potřeba distribuovat adresář lib/
- Vhodné pro embedded systémy nebo kontejnery

**Ruční sestavení:**
```bash
# Sestavení (vyžaduje libsolv, jansson, curl, openssl)
cmake -B build
cmake --build build

# Konfigurace repozitářů (systémová)
sudo mkdir -p /etc/balikar
sudo cp repos.conf /etc/balikar/repos.conf

# nebo konfigurace pro aktuálního uživatele
mkdir -p ~/.config/balikar
cp repos.conf ~/.config/balikar/repos.conf

# Výpis dostupných balíčků
./build/balikar repo list
```

### Základní použití

```bash
# Instalace balíčku (s automatickým řešením závislostí)
./build/balikar install gzip

# Sestavení ze zdrojů
./build/balikar build gcc

# Zobrazení informací o balíčku
./build/balikar show bash

# Dry-run (zobrazení co by se stalo)
./build/balikar install gcc --dry-run
```

## Správa repozitářů

```bash
# Výpis všech konfigurovaných repozitářů
./build/balikar repos

# Výpis konfigurovaných repozitářů (alternativa)
./build/balikar repo list

# Aktualizace metadat repozitáře
./build/balikar repo-update              # Aktualizuje primární repozitář
./build/balikar repo-update --repo=./repo  # Aktualizuje konkrétní repozitář

```

### Konfigurační soubory

- **Uživatelská konfigurace**: `~/.config/balikar/repos.conf` (nejvyšší priorita)
- **Systémová konfigurace**: `/etc/balikar/repos.conf` (záloha)

**Příklad konfigurace:**
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

## Sestavení a zprovoznění

Balikar lze získat a spustit třemi způsoby. Jejich podmínky a postup jsou popsány níže.

---

### Cesta 1: Předkompilovaná binárka (nejjednodušší)

Repozitář obsahuje předkompilovanou binárku spolu s adresářem `lib/`, který obsahuje potřebné knihovny (zejména libsolv s `ENABLE_CONDA=ON`), jež nejsou součástí standardních distribucí. Není potřeba Conda ani kompilátor.

**Požadavky – systémové balíky:**
```bash
# Debian/Ubuntu
sudo apt install libcurl4 libjansson4 libarchive13 patchelf

# Fedora/RHEL
sudo dnf install libcurl jansson libarchive patchelf
```

**Postup:**
```bash
git clone https://gitlab.fel.cvut.cz/placeja5/proj6_balikar.git
cd proj6_balikar

sudo sh run_scripts/install/install_prebuilt.sh   # systémová instalace (doporučeno)
# nebo
sh run_scripts/install/install_prebuilt.sh        # instalace pro aktuálního uživatele
```

Instalační skript zkopíruje binárku do `/usr/local/bin/`, knihovny do `/usr/local/lib/balikar/` a pomocí `patchelf` nastaví RPATH tak, aby je binárka za běhu nalezla.

---

### Cesta 2: Dynamická kompilace ze zdrojů

Binárka se sestaví linkováním vůči sdíleným knihovnám (`.so`). Libsolv musí být dostupná jako sdílená knihovna zkompilovaná s příznakem `ENABLE_CONDA=ON` – tuto podmínku splňuje **Conda** (Miniforge/Anaconda) s instalovaným prostředím obsahujícím libsolv.

**Požadavky:**
- Nainstalovaná Conda (Miniforge) s aktivním prostředím obsahujícím libsolv
- Systémové balíky: `libcurl`, `libjansson`, `libarchive` (vývojové verze s hlavičkovými soubory)

```bash
# Debian/Ubuntu
sudo apt install libcurl4-openssl-dev libjansson-dev libarchive-dev cmake build-essential pkg-config

# Fedora/RHEL
sudo dnf install libcurl-devel jansson-devel libarchive-devel cmake gcc pkg-config
```

**Postup:**
```bash
git clone https://gitlab.fel.cvut.cz/placeja5/proj6_balikar.git
cd proj6_balikar

conda activate <tvoje-prostředí>   # CONDA_PREFIX musí být nastaven
./run_scripts/build/build_for_distribution.sh
sudo sh run_scripts/install/install_prebuilt.sh
```

Skript `run_scripts/build/build_for_distribution.sh` sestaví binárku, pomocí `ldd` detekuje všechny conda-specifické sdílené knihovny a zkopíruje je do `lib/`. Výsledek je identický s Cestou 1 – distribuce pak probíhá stejným skriptem `run_scripts/install/install_prebuilt.sh`.
> **Poznámka:** CMake tvrdě vyžaduje nastavenou proměnnou `CONDA_PREFIX`. Bez aktivního conda prostředí konfigurace selže.

---

### Cesta 3: Statická kompilace ze zdrojů

Veškeré závislosti jsou zalinkovány přímo do binárky. Výsledkem je jediný přenositelný soubor bez jakékoli závislosti na systémových nebo conda knihovnách.

#### Varianta A: S Condou

Nejjednodušší cesta ke statické binárce, pokud máte Conda k dispozici.

**Požadavky:**
- Aktivní Conda prostředí s libsolv
- Statické verze systémových knihoven: `libcurl`, `libjansson`, `libarchive`, `libssl`, `zlib` a jejich statické archívy (`.a`)

```bash
# Debian/Ubuntu
sudo apt install libcurl4-openssl-dev libjansson-dev libarchive-dev \
                 libssl-dev zlib1g-dev liblzma-dev libbz2-dev libzstd-dev \
                 cmake build-essential

# Fedora/RHEL
sudo dnf install libcurl-devel jansson-devel libarchive-devel \
                 openssl-devel zlib-devel xz-devel bzip2-devel libzstd-devel \
                 cmake gcc glibc-static
```

**Postup:**
```bash
conda activate <tvoje-prostředí>
./run_scripts/build/build_static.sh
sudo cp balikar-static /usr/local/bin/balikar
```

#### Varianta B: Bez Condy (ruční příprava libsolv + libcurl)

Pokud Conda není k dispozici, je nutné **libsolv i libcurl** zkompilovat staticky ze zdrojů. Distribuční `libcurl.a` (Debian/RHEL) je sestavena s mnoha rozšířeními (gssapi, ldap, brotli, idn2, nghttp2, ssh2, rtmp, zstd, …), která balikar **vůbec nepoužívá** — balikar volá libcurl jen pro jednoduché HTTPS GET. Linkování proti distribuční verzi by buď selhalo na chybějící `.a` extras, nebo by binárku nafouklo o megabajty zbytečného kódu. Conda libcurl je z téhož důvodu sestavena minimalisticky — Variant B postup to napodobuje.

Proměnná `CONDA_PREFIX` slouží jen jako prefix, kam CMake hledá `.a` knihovny — nesouvisí s Condou jako takovou.

```bash
PREFIX=/usr/local   # nebo libovolný jiný prefix s právy zápisu

# 1. Statická libsolv (s ENABLE_CONDA=ON kvůli conda metadatovému formátu)
git clone https://github.com/openSUSE/libsolv.git
cd libsolv && mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=OFF -DENABLE_STATIC=ON -DENABLE_CONDA=ON \
         -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) && sudo make install
cd ../..

# 2. Minimalistická statická libcurl (jen ssl/crypto/z, žádné extras)
wget https://curl.se/download/curl-8.5.0.tar.xz
tar xf curl-8.5.0.tar.xz && cd curl-8.5.0
./configure --prefix=$PREFIX --disable-shared --enable-static --with-openssl \
            --without-libpsl --without-nghttp2 --without-libidn2 --without-brotli \
            --without-zstd --without-libssh2 --without-librtmp --without-gssapi \
            --disable-ldap --disable-ldaps
make -j$(nproc) && sudo make install
cd ..

# 3. Statický build balikaru
cd /cesta/k/proj6_balikar
export CONDA_PREFIX=$PREFIX
./run_scripts/build/build_static.sh
sudo cp balikar-static /usr/local/bin/balikar
```

Pokud by uživatel zapomněl na krok 2 a nasměroval `CONDA_PREFIX` na prefix s distribuční `libcurl.a`, `build_static.sh` selže s čitelnou chybou a vypíše přesně tento postup.

#### Varianta C: Alpine Docker (bez Condy, bez ručního nastavení), DOPORUČENÉ

Doporučená cesta pro reprodukovatelný build nezávislý na host systému. Alpine Linux používá musl libc, pro které je statické linkování standardní praxí a nevznikají problémy s DNS (na rozdíl od glibc). Uvnitř kontejneru se libsolv automaticky zkompiluje ze zdrojů.
Jde o sestavení na základě dockerfile proj6_balikar/tests/docker-tests/alpine.


**Požadavky:** pouze Docker.


```bash
cd /cesta/k/proj6_balikar
docker build -f tests/docker-tests/alpine/Dockerfile.alpine -t balikar-static-builder .

docker create --name tmp balikar-static-builder
docker cp tmp:/usr/local/bin/balikar ./balikar-static
docker rm tmp
```

## Repozitář balíčků

Oficiální balíčky: [github.com/JanPL00/balikar-repository](https://github.com/JanPL00/balikar-repository)

## Dokumentace

### Uživatelské příručky

- **[Uživatelský manuál](doc/manual.txt)** – Přehled příkazů, options, use cases, troubleshooting
- **[Správa repozitářů](doc/repo/README.md)** – Konfigurace, vzdálené repozitáře, multi-source fallback, CLI příkazy
- **[Příklad konfigurace repos.conf](doc/repo/repos.conf.example)** – Okomentovaný příklad konfiguračního souboru

### Dokumentace pro vývojáře

- **[Implementace builderu](doc/balikar/builder.txt)** – Podrobnosti build systému
- **[Podrobnosti parseru](doc/balikar/parser.txt)** – Parsování metadat a načítání repozitářů
- **[Implementace solveru](doc/balikar/solver.txt)** – Řešení závislostí
- **[Downloader](doc/balikar/downloader.txt)** – Stahování a ověřování balíčků
- **[Testování](tests/README.md)** – Možnosti testování programu

### Organizace zdrojových souborů

```
src/
├── balikar/
│   ├── main.c                    # dispatcher
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

