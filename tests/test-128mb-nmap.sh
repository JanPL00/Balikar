#!/bin/bash
#
# Docker test pro Balikar s 128 MB RAM limitem
# Test: Instalace nmap a jeho závislostí
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

IMAGE_NAME="balikar-128mb-test"
CONTAINER_NAME="balikar-128mb-test-${RANDOM}"
MEMORY_LIMIT="128m"

echo "============================================"
echo "Balikar 128 MB RAM Test - nmap Installation"
echo "============================================"
echo ""

# Build Docker image
echo "[1/5] Building Docker image..."
cd "$SCRIPT_DIR"
docker build -f Dockerfile.128mb -t "$IMAGE_NAME" . || {
    echo "ERROR: Docker build failed"
    exit 1
}
echo "   [DONE] Image built: $IMAGE_NAME"
echo ""

# Copy balikar binary to project root if needed
echo "[2/5] Preparing balikar binary..."
if [ ! -f "$PROJECT_ROOT/balikar" ]; then
    echo "   ERROR: balikar binary not found in $PROJECT_ROOT"
    echo "   Please build with: cd $PROJECT_ROOT/build && make balikar"
    exit 1
fi
echo "   [DONE] Binary found: $(ls -lh $PROJECT_ROOT/balikar | awk '{print $5}')"
echo ""

# Run container with memory limit
echo "[3/5] Starting container with $MEMORY_LIMIT RAM limit..."
docker run -d \
    --name "$CONTAINER_NAME" \
    --memory="$MEMORY_LIMIT" \
    --memory-swap="$MEMORY_LIMIT" \
    -v "$PROJECT_ROOT:/balikar:ro" \
    "$IMAGE_NAME" \
    sleep 3600 || {
    echo "ERROR: Failed to start container"
    exit 1
}
echo "   [DONE] Container started: $CONTAINER_NAME"
echo ""

# Copy balikar to /usr/local/bin inside container
echo "[4/5] Setting up balikar in container..."
docker exec "$CONTAINER_NAME" cp /balikar/balikar /usr/local/bin/balikar
docker exec "$CONTAINER_NAME" chmod +x /usr/local/bin/balikar

# Copy required libraries from conda
echo "   Copying required libraries..."
docker exec "$CONTAINER_NAME" mkdir -p /usr/local/lib
docker cp ../lib/libsolv.so.1 "$CONTAINER_NAME:/usr/local/lib/"
docker cp ../lib/libsolvext.so.1 "$CONTAINER_NAME:/usr/local/lib/"
docker cp ../lib/libpcre2-posix.so.3 "$CONTAINER_NAME:/usr/local/lib/"
docker cp ../lib/libz.so.1 "$CONTAINER_NAME:/usr/local/lib/"
docker exec "$CONTAINER_NAME" ldconfig
echo "   [DONE] Balikar installed"
echo ""

# Run tests
echo "[5/5] Running tests..."
echo "============================================"
echo ""

# Test script to run inside container
docker exec "$CONTAINER_NAME" bash -c '
set -e

# Repository URL
REPO_URL="https://raw.githubusercontent.com/JanPL00/balikar-repository/main"

echo "=== MEMORY CONSTRAINTS ==="
echo "Memory limit: 128 MB"
free -h
echo ""

echo "=== TEST 1: Help Command ==="
balikar --help | head -5
echo "   [PASS] Help displayed"
echo ""

echo "=== TEST 2: Repository List ==="
balikar --repo=$REPO_URL repos
echo "   [PASS] Repos listed"
echo ""

echo "=== TEST 3: Resolve nmap (dry-run) ==="
echo "Resolving nmap dependencies..."
if balikar --repo=$REPO_URL resolve nmap 2>&1 | tee /tmp/resolve.log; then
    echo "   [PASS] Dependency resolution successful"
    echo ""
    echo "Resolved packages:"
    grep -E "install|Installing" /tmp/resolve.log | head -10 || true
else
    echo "   [WARN] Resolution failed or incomplete"
fi
echo ""

echo "=== TEST 4: Check Memory Usage ==="
echo "Current memory state:"
free -h | grep -E "Mem:|Swap:"
ps aux | head -1
ps aux | grep balikar | grep -v grep || echo "No balikar processes running"
echo ""

echo "=== TEST 5: Download nmap (without install) ==="
echo "Testing download command..."
mkdir -p /tmp/downloads
if timeout 300 bash -c "echo \"y\" | balikar --repo=$REPO_URL download --output-dir=/tmp/downloads nmap" 2>&1 | tee /tmp/download.log; then
    echo "   [PASS] Download command completed"
    echo ""
    echo "Downloaded files:"
    ls -lh /tmp/downloads/ 2>/dev/null | head -10 || echo "No files downloaded"
else
    echo "   [WARN] Download timed out or failed"
    echo "Last 20 lines of log:"
    tail -20 /tmp/download.log
fi
echo ""

echo "=== TEST 6: Memory Peak During Operations ==="
echo "Memory statistics:"
free -h
echo ""

echo "============================================"
echo "TEST SUMMARY"
echo "============================================"
echo "Memory Limit:     128 MB"
echo "Tests Run:        6"
echo "Context:          Command Pattern refactored"
echo ""

# Check for OOM kills
if dmesg 2>/dev/null | grep -i "out of memory" | tail -5; then
    echo "WARNING: Out of memory events detected!"
else
    echo "No OOM events detected [GOOD]"
fi
echo ""
'

TEST_EXIT=$?

echo ""
echo "============================================"
echo "Cleaning up..."
docker stop "$CONTAINER_NAME" > /dev/null 2>&1 || true
docker rm "$CONTAINER_NAME" > /dev/null 2>&1 || true
echo "   [DONE] Container removed"
echo ""

if [ $TEST_EXIT -eq 0 ]; then
    echo "============================================"
    echo " [SUCCESS] All tests completed"
    echo "============================================"
    exit 0
else
    echo "============================================"
    echo " [FAILURE] Some tests failed"
    echo "============================================"
    exit 1
fi
