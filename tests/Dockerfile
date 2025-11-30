# Dockerfile pro testování Balikar Package Manageru
# Ubuntu prostředí s libsolv podporou

FROM ubuntu:24.04

# Metadata
LABEL description="Balikar Package Manager - Testing Environment"
LABEL version="1.0"

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Instalace build závislostí pro balikar
RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    cmake \
    make \
    pkg-config \
    libjansson-dev \
    libcurl4-openssl-dev \
    libarchive-dev \
    check \
    bash \
    git \
    wget \
    zlib1g-dev \
    libexpat1-dev \
    && rm -rf /var/lib/apt/lists/*

# Instalace build-essential a nástroje pro kompilaci balíčků (glibc, bash, atd.)
RUN apt-get update && apt-get install -y \
    build-essential \
    bison \
    gawk \
    texinfo \
    python3 \
    python3-pip \
    file \
    patch \
    diffutils \
    findutils \
    grep \
    sed \
    tar \
    gzip \
    bzip2 \
    xz-utils \
    coreutils \
    && rm -rf /var/lib/apt/lists/*

# Build libsolv from source with conda support
RUN cd /tmp && \
    wget https://github.com/openSUSE/libsolv/archive/refs/tags/0.7.24.tar.gz && \
    tar xzf 0.7.24.tar.gz && \
    cd libsolv-0.7.24 && \
    mkdir build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr \
          -DLIB_INSTALL_DIR=/usr/lib/x86_64-linux-gnu \
          -DENABLE_CONDA=ON \
          -DENABLE_COMPLEX_DEPS=ON \
          .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd / && rm -rf /tmp/libsolv-0.7.24 /tmp/0.7.24.tar.gz

# Vytvoření adresářové struktury pro balikar
RUN mkdir -p /var/cache/balikar/downloads \
    && mkdir -p /var/lib/balikar \
    && mkdir -p /etc/balikar \
    && mkdir -p /usr/local/bin

# Environment variables pro balikar build
ENV CONDA_PREFIX=/usr \
    PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
    LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

# Nastavení pracovního adresáře
WORKDIR /balikar

# Výchozí příkaz
CMD ["/bin/bash"]
