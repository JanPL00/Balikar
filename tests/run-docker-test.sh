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

# Spuštění kontejneru s mount projektu
docker run -it --rm \
    --name balikar-test-container \
    -v "$PROJECT_ROOT:/balikar" \
    -w /balikar \
    balikar-test \
    /bin/bash

echo ""
echo "Container stopped."
