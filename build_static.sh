#!/bin/sh
# Build fully static binary for Balikar Package Manager
# Works on any Linux system with static libraries available

set -e

# Always run from the project root (where CMakeLists.txt lives)
cd "$(dirname "$0")"

echo "========= Building STATIC Balikar Binary ========="
echo ""

# Check if running in conda environment
if [ -z "$CONDA_PREFIX" ]; then
    echo "WARNING: CONDA_PREFIX not set!"
    echo "Setting to /usr for system libsolv..."
    export CONDA_PREFIX=/usr
fi

echo "Using CONDA_PREFIX: $CONDA_PREFIX"
echo ""

# Check for required static libraries
echo "Checking for required static libraries..."
MISSING_LIBS=""

# Core libraries
for lib in libcurl.a libjansson.a libarchive.a libssl.a libcrypto.a libz.a; do
    if ! find /usr/lib /usr/local/lib -name "$lib" 2>/dev/null | grep -q .; then
        MISSING_LIBS="$MISSING_LIBS $lib"
    fi
done

# Check for static libsolv
if ! find "$CONDA_PREFIX/lib" /usr/lib /usr/local/lib -name "libsolv.a" 2>/dev/null | grep -q .; then
    echo ""
    echo "ERROR: libsolv.a not found!"
    echo ""
    echo "You need to build libsolv statically first:"
    echo "  git clone https://github.com/openSUSE/libsolv.git"
    echo "  cd libsolv && mkdir build && cd build"
    echo "  cmake .. -DBUILD_SHARED_LIBS=OFF -DENABLE_CONDA=ON -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX"
    echo "  make -j\$(nproc) && sudo make install"
    echo ""
    exit 1
fi

if [ -n "$MISSING_LIBS" ]; then
    echo ""
    echo "ERROR: Missing static libraries:$MISSING_LIBS"
    echo ""
    echo "Install development packages with static libraries:"
    echo ""
    echo "Debian/Ubuntu:"
    echo "  sudo apt install libcurl4-openssl-dev libjansson-dev libarchive-dev \\"
    echo "                   libssl-dev zlib1g-dev liblzma-dev libbz2-dev libzstd-dev"
    echo ""
    echo "Fedora/RHEL:"
    echo "  sudo dnf install libcurl-devel jansson-devel libarchive-devel \\"
    echo "                   openssl-devel zlib-devel xz-devel bzip2-devel libzstd-devel"
    echo ""
    echo "Alpine Linux:"
    echo "  sudo apk add curl-dev curl-static jansson-dev jansson-static \\"
    echo "               libarchive-dev libarchive-static openssl-libs-static zlib-static"
    echo ""
    exit 1
fi

echo "All required static libraries found."
echo ""

# Clean previous build
if [ -d "build-static" ]; then
    echo "Cleaning previous static build..."
    rm -rf build-static
fi

# Configure with static option
echo "Configuring static build..."
cmake -B build-static \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_STATIC=ON \
    -DCONDA_PREFIX="$CONDA_PREFIX"

echo ""
echo "Building static binary..."
cmake --build build-static -j$(nproc)

# Copy to root
echo ""
echo "Copying binary to project root..."
cp -v build-static/balikar ./balikar-static
chmod +x ./balikar-static

# Verify it's static
echo ""
echo "========= Build Complete ========="
echo ""
echo "Static binary created: ./balikar-static"
echo ""
echo "Verification:"
file ./balikar-static
echo ""
echo "Size:"
ls -lh ./balikar-static
echo ""
echo "Dependencies (should show only linux-vdso and possibly ld-linux):"
ldd ./balikar-static 2>&1 || echo "Fully static - no dynamic dependencies!"
echo ""
echo "Test run:"
./balikar-static --help || true
echo ""
echo "To install:"
echo "  sudo cp ./balikar-static /usr/local/bin/balikar"
echo "  sudo chmod +x /usr/local/bin/balikar"
