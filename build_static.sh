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
    # Podpora pro manuální sestavení bez Condy:
    # Pokud uživatel nainstaloval libsolv.a ručně, může nastavit LIBSOLV_PREFIX
    # na adresář obsahující lib/libsolv.a a include/solv/
    # Příklad:
    #   git clone https://github.com/openSUSE/libsolv.git
    #   cmake -B libsolv/build libsolv -DENABLE_STATIC=ON -DENABLE_CONDA=ON \
    #         -DCMAKE_INSTALL_PREFIX=/usr/local
    #   make -C libsolv/build install
    #   CONDA_PREFIX=/usr/local bash build_static.sh
    if [ -n "$LIBSOLV_PREFIX" ]; then
        export CONDA_PREFIX="$LIBSOLV_PREFIX"
        echo "Using LIBSOLV_PREFIX as CONDA_PREFIX: $CONDA_PREFIX"
    else
        echo "WARNING: CONDA_PREFIX not set!"
        echo "Setting to /usr/local for system libsolv..."
        echo "(Pro manuální instalaci nastavte: CONDA_PREFIX=/cesta/k/libsolv bash build_static.sh)"
        export CONDA_PREFIX=/usr/local
    fi
fi

echo "Using CONDA_PREFIX: $CONDA_PREFIX"
echo ""

# Check for required static libraries
echo "Checking for required static libraries..."
MISSING_LIBS=""

# Core libraries — hledej v CONDA_PREFIX a systémových cestách (bez libcurl — tu stavíme ze zdrojů)
for lib in libjansson.a libarchive.a libssl.a libcrypto.a libz.a; do
    if ! find "$CONDA_PREFIX/lib" /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu -name "$lib" 2>/dev/null | grep -q .; then
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
    echo "  sudo apt install libjansson-dev libarchive-dev \\"
    echo "                   libssl-dev zlib1g-dev liblzma-dev libbz2-dev libzstd-dev"
    echo ""
    echo "Fedora/RHEL:"
    echo "  sudo dnf install jansson-devel libarchive-devel \\"
    echo "                   openssl-devel zlib-devel xz-devel bzip2-devel libzstd-devel"
    echo ""
    echo "Alpine Linux:"
    echo "  sudo apk add jansson-dev jansson-static \\"
    echo "               libarchive-dev libarchive-static openssl-libs-static zlib-static"
    echo ""
    exit 1
fi

echo "All required static libraries found."
echo ""

# Build libcurl from source with minimal features (HTTPS only)
# Systémový libcurl.a na Debian/Ubuntu je sestaven se SSH2, RTMP, GSSAPI, LDAP —
# to vyžaduje desítky extra statických knihoven. Proto stavíme vlastní minimální verzi.
if ! [ -f "$CONDA_PREFIX/lib/libcurl.a" ]; then
    echo "Building libcurl from source (minimal: HTTPS only, no SSH/RTMP/LDAP/GSSAPI)..."
    TMP_CURL=$(mktemp -d)
    git clone --depth=1 https://github.com/curl/curl.git "$TMP_CURL/curl"
    cmake -B "$TMP_CURL/build" "$TMP_CURL/curl" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DCURL_USE_OPENSSL=ON \
        -DCURL_USE_LIBSSH2=OFF \
        -DCURL_BROTLI=OFF \
        -DCURL_USE_GSSAPI=OFF \
        -DCURL_DISABLE_LDAP=ON \
        -DCURL_DISABLE_LDAPS=ON \
        -DCURL_USE_LIBRTMP=OFF \
        -DUSE_NGHTTP2=OFF \
        -DCURL_ZSTD=OFF \
        -DCURL_USE_LIBPSL=OFF \
        -DCURL_USE_LIBIDN2=OFF \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_INSTALL_PREFIX="$CONDA_PREFIX"
    cmake --build "$TMP_CURL/build" -j$(nproc)
    cmake --install "$TMP_CURL/build" 2>/dev/null || sudo cmake --install "$TMP_CURL/build"
    rm -rf "$TMP_CURL"
    echo "libcurl installed to $CONDA_PREFIX"
    echo ""
else
    echo "libcurl already built at $CONDA_PREFIX/lib/libcurl.a — skipping."
    echo ""
fi

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
    -DCONDA_PREFIX="$CONDA_PREFIX" \
    -DCMAKE_PREFIX_PATH="$CONDA_PREFIX"

echo ""
echo "Building static binary..."
cmake --build build-static -j$(nproc) --target balikar

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
