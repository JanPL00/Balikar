#!/bin/sh
# Build script for distribution binary with bundled conda libraries

set -e

# Always run from the project root (where CMakeLists.txt lives)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "========= Building Balikar for Distribution (with bundled libs) ========="
echo "Project root: $PROJECT_ROOT"
echo ""

# Check CONDA_PREFIX is set
if [ -z "$CONDA_PREFIX" ]; then
    echo "ERROR: CONDA_PREFIX not set!"
    echo "Please activate conda environment first:"
    echo "  conda activate balikar"
    exit 1
fi

echo "Using conda environment: $CONDA_PREFIX"
echo ""

# Clean previous dist
if [ -d "dist" ]; then
    rm -rf dist
fi

# Build (reuse existing build directory)
echo "Building binary..."
cmake -B build
cmake --build build

# Create distribution directory
echo ""
echo "Creating distribution package..."
mkdir -p dist/lib

# Copy binary
cp -v ./balikar dist/balikar

# Copy required conda libraries
echo ""
echo "Detecting required conda libraries..."
CONDA_LIBS=$(ldd ./balikar | grep "$CONDA_PREFIX" | awk '{print $1}')

if [ -z "$CONDA_LIBS" ]; then
    echo "No conda libraries detected, copying common ones..."
    CONDA_LIBS="libsolv.so.1 libsolvext.so.1 libz.so.1 libpcre2-posix.so.3"
fi

echo "Bundling libraries:"
for lib in $CONDA_LIBS; do
    if [ -f "$CONDA_PREFIX/lib/$lib" ]; then
        cp -v "$CONDA_PREFIX/lib/$lib" dist/lib/
    else
        echo "  WARNING: $lib not found in $CONDA_PREFIX/lib/"
    fi
done

# Check for transitive dependencies
echo ""
echo "Checking transitive dependencies from bundled libraries..."
cd dist
for lib in lib/*.so*; do
    # Get all missing dependencies
    TRANS_LIBS=$(ldd "$lib" 2>/dev/null | grep "not found" | awk '{print $1}')
    for tlib in $TRANS_LIBS; do
        if [ -f "$CONDA_PREFIX/lib/$tlib" ]; then
            echo "  Found missing transitive: $tlib"
            cp -v "$CONDA_PREFIX/lib/$tlib" lib/
        fi
    done
done
cd ..

# Copy distribution to root for git commit
echo ""
echo "Copying to root for git commit..."
cp -v dist/balikar ./balikar
mkdir -p lib
cp -v dist/lib/*.so* lib/
echo "Copied lib/ directory with $(ls lib/ | wc -l) libraries"

# Set RPATH for relative lib directory
echo ""
echo "Setting RPATH..."
patchelf --set-rpath '$ORIGIN/lib:$ORIGIN/../lib' dist/balikar 2>/dev/null || \
    echo "Note: patchelf not available, RPATH set during build"

echo ""
echo "========= Build Complete ========="
echo ""
echo "Distribution package created in: ./dist/"
echo "  dist/balikar       - Main binary"
echo "  dist/lib/          - Required libraries"
echo ""
echo "Files for git commit:"
echo "  ./balikar          - Main binary"
echo "  ./lib/             - Bundled libraries"
echo ""
echo "Verify dependencies:"
ldd ./balikar | head -10
echo ""
echo "To distribute:"
echo "1. Commit ./balikar and ./lib/ to git:"
echo "   git add balikar lib/"
echo "   git commit -m 'Update pre-built distribution'"
echo "2. On target system: git clone, then sh install_prebuilt.sh"
echo ""
echo "To test locally:"
echo "  cd dist && ./balikar --help"
