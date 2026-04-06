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
- ✅ Single file, no dependencies
- ✅ Works on any Linux distribution
- ✅ No need to distribute lib/ directory
- ✅ Perfect for embedded systems or containers

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