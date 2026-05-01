# Balikar Package Manager

Source-based package manager inspired by Linux From Scratch (LFS), using libsolv for dependency resolution.

## Features

- **Dependency Resolution** using libsolv (same engine as zypper, dnf)
- **Build-from-Source** workflow (LFS-style)
- **Remote Repositories** support (GitHub, HTTP/HTTPS)
- **Multiple Repository** support with priorities
- **Staging Environment** for safe builds ($DESTDIR)
- **Custom Build Scripts** (.build) and install hooks (.install)

## Quick Start

### Installation Options

#### Option 1: Pre-built Binary (with bundled libraries)

**Prerequisites - Install system libraries:**
```bash
# Debian/Ubuntu
sudo apt install libcurl4 libjansson4 libarchive13 patchelf

# Fedora/RHEL
sudo dnf install libcurl jansson libarchive patchelf
```

**Install Balikar:**
```bash
# Clone repository
git clone https://gitlab.fel.cvut.cz/placeja5/proj6_balikar.git
cd proj6_balikar

# System-wide installation (recommended)
sudo sh install_prebuilt.sh

# OR user installation
sh install_prebuilt.sh
```

#### Option 2: Static Binary (single file, works anywhere)

**Build static binary using Alpine Docker (recommended):**
```bash
# Requires only Docker installed
./build_static_alpine.sh

# Install
sudo cp balikar-static /usr/local/bin/balikar
```

**Or build natively (requires static libraries):**
```bash
# See doc/STATIC_BUILD.md for detailed instructions
./build_static.sh
```

**Advantages of static build:**
- Single file, no dependencies
- Works on any Linux distribution
- No need to distribute lib/ directory
- Perfect for embedded systems or containers

See [doc/STATIC_BUILD.md](doc/STATIC_BUILD.md) for complete static build guide.

**Manual:**
```bash
# Build (requires libsolv, jansson, curl, openssl)
cmake -B build
cmake --build build

# Configure repositories (system-wide)
sudo mkdir -p /etc/balikar
sudo cp repos.conf /etc/balikar/repos.conf

# OR configure per-user
mkdir -p ~/.config/balikar
cp repos.conf ~/.config/balikar/repos.conf

# List available packages
./build/balikar repo list
```

### Basic Usage

```bash
# Install a package (with automatic dependency resolution)
./build/balikar install gzip

# Build from source
./build/balikar build gcc

# Show package information
./build/balikar show bash

# Dry-run (see what would happen)
./build/balikar install gcc --dry-run
```

## Repository Management

```bash
# List all configured repositories
./build/balikar repos

# List configured repositories (alternative)
./build/balikar repo list

# Update repository metadata
./build/balikar repo-update              # Updates primary repository
./build/balikar repo-update --repo=./repo  # Updates specific repository

# Add remote repository
./build/balikar repo add my-repo https://example.com/packages 80

# Enable/disable repositories
./build/balikar repo enable my-repo
./build/balikar repo disable local-dev

# Remove repository
./build/balikar repo remove my-repo
```

### Configuration Files

- **User config**: `~/.config/balikar/repos.conf` (highest priority)
- **System config**: `/etc/balikar/repos.conf` (fallback)

**Example configuration:**
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

sudo sh install_prebuilt.sh   # systémová instalace (doporučeno)
# nebo
sh install_prebuilt.sh        # instalace pro aktuálního uživatele
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
./build_for_distribution.sh
sudo sh install_prebuilt.sh
```

Skript `build_for_distribution.sh` sestaví binárku, pomocí `ldd` detekuje všechny conda-specifické sdílené knihovny a zkopíruje je do `lib/`. Výsledek je identický s Cestou 1 – distribuce pak probíhá stejným skriptem `install_prebuilt.sh`.

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
./build_static.sh
sudo cp balikar-static /usr/local/bin/balikar
```

#### Varianta B: Bez Condy (ruční příprava libsolv)

Pokud Conda není k dispozici, je nutné libsolv zkompilovat ze zdrojů ručně a nainstalovat do systému. Proměnná `CONDA_PREFIX` pak slouží pouze k tomu, aby CMake věděl, kde libsolv hledat – nesouvisí s Condou jako takovou.

```bash
# Kompilace libsolv ze zdrojů
git clone https://github.com/openSUSE/libsolv.git
cd libsolv && mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=OFF -DENABLE_STATIC=ON -DENABLE_CONDA=ON \
         -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) && sudo make install
cd /cesta/k/proj6_balikar

export CONDA_PREFIX=/usr/local
./build_static.sh
sudo cp balikar-static /usr/local/bin/balikar
```

#### Varianta C: Alpine Docker (bez Condy, bez ručního nastavení)
TODO... možná odstranit tuto možnost

Doporučená cesta pro reprodukovatelný build nezávislý na host systému. Alpine Linux používá musl libc, pro které je statické linkování standardní praxí a nevznikají problémy s DNS (na rozdíl od glibc). Uvnitř kontejneru se libsolv automaticky zkompiluje ze zdrojů.

**Požadavky:** pouze Docker.

```bash
./build_static_alpine.sh
sudo cp balikar-static /usr/local/bin/balikar
```

---

### Přehled cest

| Cesta | Conda nutná | Kompilátor nutný | Výsledek | Vhodné pro |
|---|---|---|---|---|
| 1 – Předkompilovaná | Ne | Ne | Binárka + `lib/` | Koncové uživatele |
| 2 – Dynamická kompilace | **Ano** | Ano | Binárka + `lib/` | Vývoj |
| 3A – Statická s Condou | **Ano** | Ano | Jediný soubor | Distribuce |
| 3B – Statická bez Condy | Ne | Ano | Jediný soubor | Systémy bez Condy |
| 3C – Alpine Docker | Ne | Ne (Docker) | Jediný soubor | Přenositelnost |

---

## Package Repository

Official packages: [github.com/JanPL00/balikar-repository](https://github.com/JanPL00/balikar-repository)

Includes 11 packages from Linux From Scratch 12.4:
- bash, binutils, coreutils, gcc, glibc, gmp, gzip, hello, iana-etc, tar, zlib

## Documentation

### User Guides

- **[Repository Management](doc/repo/README.md)** - Complete guide: configuration, remote repositories, CLI commands
- **[Configuration Example](doc/repo/repos.conf.example)** - Annotated repository configuration

### Developer Documentation

- **[Architecture & Workflow](doc/balikar/README.md)** - System architecture, workflow diagrams, module overview
- **[Builder Implementation](doc/balikar/builder.txt)** - Build system details
- **[Parser Details](doc/balikar/parser.txt)** - Metadata parsing and repository loading
- **[Solver Implementation](doc/balikar/solver.txt)** - Dependency resolution
- **[Downloader](doc/balikar/downloader.txt)** - Package downloading and verification
- **[Manual](doc/manual.txt)** - General documentation


# TODO
- split download command for downloading source tar.xz files (fetch-source) a balikar files ve formatu tar.bz2 (download)