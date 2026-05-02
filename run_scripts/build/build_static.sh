#!/bin/sh
# Build fully static binary for Balikar Package Manager
# Works on any Linux system with static libraries available

set -e

# Always run from the project root (where CMakeLists.txt lives)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "========= Building STATIC Balikar Binary ========="
echo "Project root: $PROJECT_ROOT"
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

# Core libraries — hledej nejdřív v CONDA_PREFIX, pak v systémových cestách
for lib in libcurl.a libjansson.a libarchive.a libssl.a libcrypto.a libz.a; do
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

# Detekce tranzitivních závislostí systémové libcurl.a
# Balikar volá libcurl jen na jednoduché HTTPS GET — nepotřebuje gssapi,
# ldap, sasl, ssh2, rtmp, brotli, idn2, psl, nghttp2 ani zstd. Conda
# libcurl je sestavena minimalisticky a linkuje se proti ssl/crypto/z.
# Distribuční libcurl.a (Debian/RHEL) je ale obvykle sestavena se všemi
# těmito rozšířeními → linker by hledal jejich .a, které typicky nejsou
# v základní instalaci a jejichž symboly balikar stejně nikdy nezavolá.
# Místo nafouknutí binárky desítkami nevyužitých knihoven vyžadujeme,
# aby uživatel sestavil minimalistickou libcurl ze zdrojů.
if [ -z "$CONDA_PREFIX" ] || [ "$CONDA_PREFIX" = "/usr/local" ] || \
   ! [ -f "$CONDA_PREFIX/lib/libcurl.a" ]; then
    echo "Variant B detected (system libcurl.a) — auditing transitive deps..."
    SYS_PC_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/lib64/pkgconfig"
    CURL_DEPS=$(PKG_CONFIG_LIBDIR="$SYS_PC_PATH" pkg-config --static --libs libcurl 2>/dev/null \
                | tr ' ' '\n' | sed -n 's/^-l//p' | sort -u)

    UNUSED_DEPS=""
    for dep in $CURL_DEPS; do
        case "$dep" in
            curl|ssl|crypto|z|pthread|dl|m|c|gcc_s) continue ;;
        esac
        UNUSED_DEPS="$UNUSED_DEPS $dep"
    done

    if [ -n "$UNUSED_DEPS" ]; then
        echo ""
        echo "ERROR: System libcurl.a is built with features that balikar does not use:"
        echo "  $UNUSED_DEPS"
        echo ""
        echo "Linking against this libcurl.a would either fail (missing .a) or"
        echo "needlessly bloat the static binary by megabytes."
        echo ""
        echo "Recommended fix — build minimal static libcurl into \$CONDA_PREFIX:"
        echo ""
        echo "  PREFIX=\${CONDA_PREFIX:-/usr/local}"
        echo "  wget https://curl.se/download/curl-8.5.0.tar.xz"
        echo "  tar xf curl-8.5.0.tar.xz && cd curl-8.5.0"
        echo "  ./configure --prefix=\$PREFIX --disable-shared --enable-static \\"
        echo "      --with-openssl \\"
        echo "      --without-libpsl --without-nghttp2 --without-libidn2 --without-brotli \\"
        echo "      --without-zstd --without-libssh2 --without-librtmp --without-gssapi \\"
        echo "      --disable-ldap --disable-ldaps"
        echo "  make -j\$(nproc) && make install"
        echo "  cd .. && CONDA_PREFIX=\$PREFIX bash run_scripts/build/build_static.sh"
        echo ""
        exit 1
    fi
    echo "System libcurl.a has no extra features — proceeding."
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
