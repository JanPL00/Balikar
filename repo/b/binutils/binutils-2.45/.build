#!/bin/bash
set -e

# Binutils 2.45 build script based on LFS 12.4

# Create build directory
mkdir -v build
cd build

# Configure
../configure --prefix=/usr       \
             --sysconfdir=/etc   \
             --enable-gold       \
             --enable-ld=default \
             --enable-plugins    \
             --enable-shared     \
             --disable-werror    \
             --enable-64-bit-bfd \
             --enable-new-dtags  \
             --with-system-zlib  \
             --enable-default-hash-style=gnu

# Build
make tooldir=/usr

# Install
make tooldir=/usr install

# Remove static libraries
rm -fv /usr/lib/lib{bfd,ctf,ctf-nobfd,gprofng,opcodes,sframe}.a
