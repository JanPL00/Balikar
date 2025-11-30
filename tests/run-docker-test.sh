#!/bin/bash
# Build a spuštění Docker kontejneru pro testování balikaru

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TESTS_DIR="$SCRIPT_DIR"

echo "=== Balikar Docker Test Environment ==="
echo ""

# Kontrola, zda Docker běží
if ! docker info &> /dev/null; then
    echo "ERROR: Docker není spuštěn nebo není nainstalován"
    exit 1
fi

# Build Docker image
echo "1. Building Docker image..."
docker build -t balikar-test -f "$TESTS_DIR/Dockerfile" "$PROJECT_ROOT"

echo ""
echo "2. Docker image built successfully!"
echo ""
echo "Starting container..."
echo ""

# Detekce balikar-repository
REPO_DIR="$(dirname "$PROJECT_ROOT")/balikar-repository"
if [ ! -d "$REPO_DIR" ]; then
    echo "WARNING: Repository not found at $REPO_DIR"
    echo "Tests may fail without a repository."
fi

# Spuštění kontejneru s mount projektu a repozitáře
MOUNT_REPO=""
if [ -d "$REPO_DIR" ]; then
    MOUNT_REPO="-v $REPO_DIR:/repo"
    echo "Mounting repository: $REPO_DIR -> /repo"
fi

docker run -it --rm \
    --name balikar-test-container \
    -v "$PROJECT_ROOT:/balikar" \
    $MOUNT_REPO \
    -w /balikar \
    -e CONDA_PREFIX=/usr \
    -e PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
    -e LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu \
    balikar-test \
    /bin/bash -c '
    echo "=== Auto-compiling Balikar ==="
    cd /balikar
    rm -rf build
    mkdir -p build && cd build
    echo "Running cmake..."
    cmake -DCONDA_PREFIX=/usr .. || exit 1
    echo ""
    echo "Running make..."
    make -j$(nproc) || exit 1
    echo ""
    echo "=== Compilation successful! ==="
    echo ""
    echo "Binary: /balikar/build/balikar"
    echo ""
    echo "To run automated tests:"
    echo "  /balikar/tests/test-scenarios.sh"
    echo ""
    echo "Or test manually:"
    echo "  ./balikar --help"
    echo "  ./balikar install --root=/test_root bash"
    echo ""
    exec /bin/bash
    '

echo ""
echo "Container stopped."
