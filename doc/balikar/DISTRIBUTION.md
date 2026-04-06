# Balikar Distribution Guide

## Pre-built Binary Distribution

This repository contains pre-built binaries with bundled conda libraries for easy installation.

### Structure

```
proj6_balikar/
├── balikar              # Pre-built binary (328KB)
├── lib/                 # Bundled libraries (~1MB)
│   ├── libsolv.so.1     # Conda libsolv with conda support
│   ├── libsolvext.so.1
│   ├── libz.so.1
│   └── libpcre2-posix.so.3
└── install_prebuilt.sh  # Installation script
```

### For Users: Installation

#### Prerequisites

Install required system libraries:

**Debian/Ubuntu:**
```bash
sudo apt install libcurl4 libjansson4 libarchive13 patchelf bzip2 xz gzip
```

**Fedora/RHEL:**
```bash
sudo dnf install libcurl jansson libarchive patchelf bzip2 xz gzip
```

#### System-wide installation (requires sudo):
```bash
git clone <your-repo-url>
cd proj6_balikar
sudo sh install_prebuilt.sh
```

This installs:
- Binary: `/usr/local/bin/balikar`
- Libraries: `/usr/local/lib/balikar/`
- Config: `/etc/balikar/`

#### User installation (no sudo):
```bash
git clone <your-repo-url>
cd proj6_balikar
sh install_prebuilt.sh
```

This installs:
- Binary: `~/.local/bin/balikar`
- Libraries: `~/.local/lib/balikar/`
- Config: `~/.config/balikar/`

### For Developers: Building Distribution

#### Requirements:
- Active conda environment with libsolv
- cmake, gcc, patchelf

#### Build new distribution:
```bash
conda activate balikar  # Or your conda env with libsolv
sh build_for_distribution.sh
```

This creates:
- `./balikar` - Binary for git commit
- `./lib/` - Libraries for git commit
- `dist/` - Complete package (not committed)

#### Commit to git:
```bash
git add balikar lib/
git commit -m "Update pre-built distribution v1.x.x"
git push
```

### Why Bundled Libraries?

Balikar requires **conda's libsolv** which includes conda-specific features not available in system libsolv packages. The bundled libraries ensure:

1. Works on any Debian/Ubuntu system without conda
2. Consistent behavior across distributions
3. No dependency on user's conda installation

### Compatibility

**Supported:**
- Debian 11, 12
- Ubuntu 20.04, 22.04, 24.04
- Other glibc-based Linux distributions

**Not Supported:**
- Alpine Linux (musl libc) - requires separate build
- macOS, Windows - not tested

For Alpine, build directly on Alpine:
```bash
apk add cmake gcc g++ make libsolv-dev curl-dev jansson-dev libarchive-dev
cmake -B build && cmake --build build
```

### Troubleshooting

**"libsolv.so.1: cannot open shared object file"**
- Solution: Make sure `lib/` directory exists alongside `balikar` binary
- Run `sh install_prebuilt.sh` to properly install with libraries

**"patchelf: command not found"**
- Not critical - RPATH is set during build
- Install patchelf for RPATH updates: `sudo apt install patchelf`

### Scripts Overview

- `install_prebuilt.sh` - Install pre-built binary + libraries
- `install_notbuilt.sh` - Build from source, then install
- `build_for_distribution.sh` - Create distribution package (dev only)
- `build_and_install.sh` - Quick build+install (legacy)
