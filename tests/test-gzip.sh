#!/bin/bash
# Jednoduchý test instalace gzip v Docker prostředí

set -e

echo "========================================="
echo "Balikar Test - gzip Installation"
echo "========================================="

# Build Docker image
echo ""
echo "[1/4] Building minimal Debian 12 image..."
docker build -f Dockerfile.minimal -t balikar-minimal-test .

# Run container
echo ""
echo "[2/4] Starting container..."
docker run --rm -it \
    -v $(cd .. && pwd)/balikar:/usr/local/bin/balikar:ro \
    -v $(cd .. && pwd)/lib:/usr/local/lib/balikar:ro \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    --name balikar-gzip-test \
    balikar-minimal-test \
    bash -c '
echo ""
echo "[2.5/4] Verifying balikar installation..."
balikar --help > /dev/null 2>&1 || { echo "ERROR: balikar not found!"; exit 1; }
echo "Balikar is ready!"

echo ""
echo "[3/4] Installing gzip..."
echo ""

echo "y" | balikar install gzip

echo ""
echo "[4/4] Verifying installation..."
which gzip && gzip --version | head -2

echo ""
echo "========================================="
echo "Test Results:"
echo "========================================="
echo "Gzip installation successful!"
echo ""
gzip --version | head -3

echo ""
echo "Test completed successfully!"
'
