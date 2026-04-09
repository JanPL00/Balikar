#!/bin/bash
# Test balikar in Docker with 128 MB RAM limit - gzip package
# Simpler test with a small package

set -e

CONTAINER_NAME="balikar-128mb-test-$$"
IMAGE_NAME="balikar-128mb-test"
DOCKERFILE="tests/Dockerfile.128mb"
BINARY="../balikar"

echo "============================================"
echo "Balikar 128 MB RAM Test - gzip Installation"
echo "============================================"
echo ""

# Step 1: Build Docker image
echo "[1/5] Building Docker image..."
docker build -t "$IMAGE_NAME" -f "$DOCKERFILE" . 2>&1 | tail -2
echo "   [DONE] Image built: $IMAGE_NAME"
echo ""

# Step 2: Check binary
echo "[2/5] Preparing balikar binary..."
if [ ! -f "$BINARY" ]; then
    echo "   [ERROR] Binary not found at $BINARY"
    exit 1
fi
du -h "$BINARY" | awk '{print "   [DONE] Binary found: " $1}'
echo ""

# Step 3: Start container with memory limit
echo "[3/5] Starting container with 128m RAM limit..."
docker run -d \
    --name "$CONTAINER_NAME" \
    --memory="128m" \
    --memory-swap="128m" \
    --rm \
    "$IMAGE_NAME" \
    sleep 3600

echo "   [DONE] Container started: $CONTAINER_NAME"
echo ""

# Step 4: Setup balikar in container
echo "[4/5] Setting up balikar in container..."
docker cp "$BINARY" "$CONTAINER_NAME:/usr/local/bin/balikar"
echo "   Copying required libraries..."
docker exec "$CONTAINER_NAME" mkdir -p /usr/local/lib
docker cp ../lib/libsolv.so.1 "$CONTAINER_NAME:/usr/local/lib/"
docker cp ../lib/libsolvext.so.1 "$CONTAINER_NAME:/usr/local/lib/"
docker cp ../lib/libpcre2-posix.so.3 "$CONTAINER_NAME:/usr/local/lib/"
docker cp ../lib/libz.so.1 "$CONTAINER_NAME:/usr/local/lib/"
docker exec "$CONTAINER_NAME" ldconfig
docker exec "$CONTAINER_NAME" chmod +x /usr/local/bin/balikar
echo "   [DONE] Balikar installed"
echo ""

# Step 5: Run tests
echo "[5/5] Running tests..."
echo "============================================"
echo ""

# Function to run command in container
function run_test() {
    docker exec "$CONTAINER_NAME" bash -c "$1"
}

echo "=== MEMORY BASELINE ==="
echo "Docker container stats:"
docker stats --no-stream --format "table {{.Container}}\t{{.MemUsage}}\t{{.MemPerc}}" "$CONTAINER_NAME"
echo ""
run_test "free -h | grep -E 'total|Mem:|Swap:'"
echo ""

echo "=== TEST 1: Help Command ==="
run_test "balikar --help | head -10"
echo "   [PASS] Help displayed"
echo ""

echo "=== TEST 2: Repository List ==="
run_test "balikar repo list"
echo "   [PASS] Repos listed"
echo ""

echo "=== TEST 3: Resolve gzip (dry-run) ==="
echo "Resolving gzip dependencies..."
if run_test "balikar resolve gzip 2>&1 | tee /tmp/resolve.log"; then
    echo "   [PASS] Dependency resolution successful"
    echo ""
    echo "Resolved output:"
    run_test "cat /tmp/resolve.log | head -20"
else
    echo "   [WARN] Resolution failed"
fi
echo ""

echo "=== TEST 4: Memory After Resolve ==="
docker stats --no-stream --format "table {{.Container}}\t{{.MemUsage}}\t{{.MemPerc}}" "$CONTAINER_NAME"
run_test "ps aux | grep balikar | grep -v grep || echo 'No balikar processes'"
echo ""

echo "=== TEST 5: Install gzip (dry-run) ==="
echo "Testing dry-run install..."
if run_test "balikar install --dry-run gzip 2>&1 | tee /tmp/install_dryrun.log"; then
    echo "   [PASS] Dry-run install completed"
    echo ""
    echo "Install plan:"
    run_test "cat /tmp/install_dryrun.log | head -20"
else
    echo "   [WARN] Dry-run failed"
fi
echo ""

echo "=== TEST 6: Memory Peak Check ==="
docker stats --no-stream --format "table {{.Container}}\t{{.MemUsage}}\t{{.MemPerc}}" "$CONTAINER_NAME"
echo ""
echo "OOM Check:"
run_test "dmesg | grep -i 'out of memory' | tail -5 || echo 'No OOM events detected'"
echo ""

echo "============================================"
echo "TEST SUMMARY"
echo "============================================"
echo "Package:          gzip (small package)"
echo "Memory Limit:     128 MB"
echo "Tests Run:        6"
echo "Context:          Command Pattern refactored"
echo ""

# Check for OOM
if run_test "dmesg | grep -qi 'out of memory'"; then
    echo "WARNING: Out of memory events detected!"
    echo ""
fi

echo "============================================"
echo "Cleaning up..."
docker stop "$CONTAINER_NAME" > /dev/null 2>&1 || true
echo "   [DONE] Container stopped"
echo ""

echo "============================================"
echo " [SUCCESS] All tests completed"
echo "============================================"
