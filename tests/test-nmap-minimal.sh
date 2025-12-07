#!/bin/bash
# Test instalace nmap v minimálním Debian 12 prostředí
# Otestuje celý dependency chain: glibc -> zlib -> openssl, libpcap, libssh2 -> nmap

set -e

echo "========================================="
echo "Balikar Minimal Test - nmap Installation"
echo "========================================="

# Build Docker image
echo ""
echo "[1/5] Building minimal Debian 12 image..."
docker build -f Dockerfile.minimal -t balikar-minimal-test .

# Run container
echo ""
echo "[2/5] Starting container..."
docker run --rm -it \
    -v $(cd .. && pwd)/balikar:/usr/local/bin/balikar:ro \
    -v $(cd .. && pwd)/lib:/usr/local/lib/balikar:ro \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    --name balikar-minimal-test \
    balikar-minimal-test \
    bash -c '
echo ""
echo "[3/5] Verifying balikar installation..."
balikar --help > /dev/null 2>&1 || { echo "ERROR: balikar not found!"; exit 1; }
echo "Balikar is ready!"

echo ""
echo "[4/5] Installing nmap (will install all dependencies)..."
echo "This will install: glibc -> zlib -> openssl, libpcap, libssh2 -> nmap"
echo ""
echo "Note: gcc build dependencies have unresolved deps (mpfr, mpc) - this is OK for build-time only deps"
echo ""

echo "y" | balikar install nmap

echo ""
echo "[5/5] Verifying installation..."
nmap --version

echo ""
echo "========================================="
echo "Test Results:"
echo "========================================="

# Check installed packages
echo "Installed packages:"
balikar list

echo ""
echo "Test completed successfully!"
'
