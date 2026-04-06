# Static Build Guide for Balikar

This guide explains how to build a fully static binary of Balikar that works anywhere without dependencies.

## Table of Contents

1. [Why Static Build?](#why-static-build)
2. [Build Methods](#build-methods)
3. [Method 1: Alpine Docker (Recommended)](#method-1-alpine-docker-recommended)
4. [Method 2: Native Static Build](#method-2-native-static-build)
5. [Troubleshooting](#troubleshooting)

## Why Static Build?

### Dynamic Binary (Default)
- **Size:** ~300 KB binary + ~1 MB libraries
- **Portability:** Requires bundled `.so` files or matching system libraries
- **Distribution:** Must ship `lib/` directory with binary

### Static Binary
- **Size:** ~8-15 MB (all libraries embedded)
- **Portability:** Single file, works on any Linux system
- **Distribution:** Just one file, no dependencies
- **Conda features:** Fully preserved (libsolv built with ENABLE_CONDA=ON)

## Build Methods

### Comparison

| Method | Difficulty | Portability | Size | Recommended |
|--------|-----------|-------------|------|-------------|
| **Alpine Docker** | Easy | Works everywhere | ~8 MB | Yes |
| **Native glibc** | Hard | Same glibc version | ~12 MB | Only if no Docker |

## Method 1: Alpine Docker (Recommended)

Uses Alpine Linux with musl libc in Docker for maximum portability.

### Prerequisites
```bash
# Install Docker
sudo apt install docker.io  # Debian/Ubuntu
sudo dnf install docker     # Fedora/RHEL
```

### Build Steps

1. **Run the build script:**
```bash
cd /home/placeja5/Balikar/proj6_balikar
chmod +x build_static_alpine.sh
./build_static_alpine.sh
```

2. **Wait for build** (first time: ~5-10 minutes, subsequent: ~2 minutes)

3. **Binary ready:**
```bash
./balikar-static --help
file ./balikar-static
# Output: balikar-static: ELF 64-bit LSB executable, static-pie linked, stripped

ls -lh ./balikar-static
# Output: -rwxr-xr-x 1 user user 8.2M ... balikar-static
```

### What It Does

The script:
1. Creates `Dockerfile.static` with Alpine Linux
2. Builds libsolv statically with conda support
3. Builds balikar with all static libraries
4. Extracts binary from container
5. Verifies it's fully static

### Advantages

- **100% portable** - works on any Linux (Debian, Ubuntu, Fedora, Arch, etc.)  
- **No glibc issues** - musl libc designed for static linking  
- **DNS works** - no NSS problems  
- **Smaller** - musl produces smaller binaries than glibc  
- **Reproducible** - same environment every time  

## Method 2: Native Static Build

Build directly on your system (requires static libraries installed).

### Prerequisites

#### Debian/Ubuntu
```bash
sudo apt install \
    build-essential \
    cmake \
    git \
    libcurl4-openssl-dev \
    libjansson-dev \
    libarchive-dev \
    libssl-dev \
    zlib1g-dev \
    liblzma-dev \
    libbz2-dev \
    libzstd-dev
```

#### Fedora/RHEL
```bash
sudo dnf install \
    gcc \
    cmake \
    git \
    libcurl-devel \
    jansson-devel \
    libarchive-devel \
    openssl-devel \
    zlib-devel \
    xz-devel \
    bzip2-devel \
    libzstd-devel \
    glibc-static
```

### Step 1: Build Static libsolv

```bash
# Clone libsolv
cd /tmp
git clone https://github.com/openSUSE/libsolv.git
cd libsolv
mkdir build && cd build

# Configure for static build with conda support
cmake .. \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_CONDA=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_BUILD_TYPE=Release

# Build and install
make -j$(nproc)
sudo make install
```

### Step 2: Build Static Balikar

```bash
cd /home/placeja5/Balikar/proj6_balikar

# Set conda prefix to where libsolv was installed
export CONDA_PREFIX=/usr/local

# Run static build script
chmod +x build_static.sh
./build_static.sh
```

### Step 3: Verify

```bash
file ./balikar-static
# Should show: statically linked

ldd ./balikar-static
# Should show: not a dynamic executable
# OR only: linux-vdso.so.1 (kernel virtual library)

./balikar-static --help
```

### Known Issues with glibc Static

- **DNS may not work** - glibc's NSS (Name Service Switch) uses dynamic plugins  
- **Larger binary** - glibc static is bigger than musl  
- **Version specific** - only works with same/compatible glibc version  

**Solution:** Use Alpine Docker method instead.

## CMakeLists.txt Options

The build system now supports a `BUILD_STATIC` option:

```bash
# Static build
cmake -B build -DBUILD_STATIC=ON -DCONDA_PREFIX=/usr/local
cmake --build build

# Dynamic build (default)
cmake -B build -DBUILD_STATIC=OFF -DCONDA_PREFIX=$CONDA_PREFIX
cmake --build build
```

### What BUILD_STATIC Does

1. **Library search:** Changes to look for `.a` files instead of `.so`
2. **Link flags:** Adds `-static -static-libgcc`
3. **Dependencies:** Automatically finds all transitive static libraries
4. **RPATH:** Disabled (not needed for static binary)

## Troubleshooting

### "libsolv.a: No such file"

**Problem:** Static libsolv not installed.

**Solution:**
```bash
# Build libsolv statically (see Step 1 above)
cd /tmp
git clone https://github.com/openSUSE/libsolv.git
cd libsolv && mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=OFF -DENABLE_CONDA=ON
sudo make install
```

### "undefined reference to ..."

**Problem:** Missing static library dependency.

**Solution:** Install development packages with static libraries:
```bash
# Debian/Ubuntu
sudo apt install libssl-dev zlib1g-dev liblzma-dev libbz2-dev

# Fedora
sudo dnf install openssl-devel zlib-devel xz-devel bzip2-devel
```

### "getaddrinfo: No such file" at runtime

**Problem:** glibc NSS issue with static linking.

**Solution:** Use Alpine Docker method - musl doesn't have this problem.

### Docker build fails

**Problem:** Network or permission issues.

**Solutions:**
```bash
# Fix Docker permissions
sudo usermod -aG docker $USER
newgrp docker

# Retry with verbose output
docker build -f Dockerfile.static -t balikar-static-builder . --progress=plain
```

### Binary crashes on different system

**Problem:** glibc version mismatch.

**Solution:** Use Alpine/musl build - it's truly portable across all Linux systems.

## Distribution

### Single Static Binary

```bash
# Just distribute one file
cp balikar-static /usr/local/bin/balikar
chmod +x /usr/local/bin/balikar

# No lib/ directory needed!
# No dependencies!
# Works anywhere!
```

### File Size Comparison

```
Dynamic build:  328 KB binary + 1.2 MB libs = 1.5 MB total
Static build:   8.5 MB binary = 8.5 MB total
```

Static is larger but **much simpler** to distribute.

## Recommended Workflow

For development:
```bash
# Use dynamic build for faster iteration
cmake -B build -DBUILD_STATIC=OFF
cmake --build build
```

For release/distribution:
```bash
# Use Alpine Docker for static release binary
./build_static_alpine.sh

# Distribute balikar-static
# Works on any Linux distro!
```

## Summary

- **Best method:** Alpine Docker (build_static_alpine.sh)  
- **Portability:** Single binary works everywhere  
- **Conda features:** Fully preserved  
- **No dependencies:** No lib/ directory needed  
- **Easy distribution:** Just one file  

The static build is perfect for:
- Release binaries
- Distribution packages
- Embedding in other tools
- Air-gapped systems
- Maximum portability
