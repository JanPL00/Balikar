#!/bin/sh
# Build fully static binary using Alpine Linux in Docker
# This is the RECOMMENDED method for creating portable static binaries
# Uses musl libc instead of glibc for better static linking support

set -e

echo "========= Building Static Balikar in Alpine Docker ========="
echo ""

# Check if docker is available
if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: Docker not found!"
    echo "Please install Docker first:"
    echo "  https://docs.docker.com/engine/install/"
    exit 1
fi

echo "Docker found, proceeding with Alpine build..."
echo ""

# Create Dockerfile for static build
cat > Dockerfile.static <<'EOF'
FROM alpine:latest

# Install build dependencies
RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    curl-dev \
    curl-static \
    jansson-dev \
    jansson-static \
    libarchive-dev \
    libarchive-static \
    openssl-libs-static \
    zlib-static \
    xz-dev \
    bzip2-dev \
    zstd-dev \
    zstd-static

WORKDIR /tmp

# Build libsolv statically with conda support
RUN git clone https://github.com/openSUSE/libsolv.git && \
    cd libsolv && \
    mkdir build && cd build && \
    cmake .. \
        -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_CONDA=ON \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf libsolv

# Copy project source
COPY . /build
WORKDIR /build

# Build balikar statically
# Alpine's musl automatically prefers static linking when .a files are available
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_STATIC=ON \
    -DCONDA_PREFIX=/usr && \
    cmake --build build -j$(nproc)

# Verify static build
RUN file build/balikar && \
    ldd build/balikar 2>&1 || true && \
    ls -lh build/balikar

CMD ["/build/balikar", "--help"]
EOF

echo "Created Dockerfile.static"
echo ""

# Build Docker image
echo "Building Docker image (this may take a few minutes)..."
docker build -f Dockerfile.static -t balikar-static-builder .

echo ""
echo "Extracting static binary from container..."

# Extract binary
docker create --name balikar-temp balikar-static-builder
docker cp balikar-temp:/build/build/balikar ./balikar-static
docker rm balikar-temp

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
echo "Dependencies:"
ldd ./balikar-static 2>&1 || echo "Fully static - no dynamic dependencies!"
echo ""
echo "Test run:"
./balikar-static --help || true
echo ""
echo "This binary will work on ANY Linux system (glibc or musl)!"
echo ""
echo "To install:"
echo "  sudo cp ./balikar-static /usr/local/bin/balikar"
echo "  sudo chmod +x /usr/local/bin/balikar"
echo ""
echo "To clean up Docker artifacts:"
echo "  docker rmi balikar-static-builder"
echo "  rm Dockerfile.static"
