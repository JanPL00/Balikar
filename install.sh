#!/bin/bash
# Installation script for Balikar Package Manager

set -e

echo "=== Balikar Package Manager - Installation ==="
echo ""

# Check if running as root for system installation
if [ "$EUID" -eq 0 ]; then
    SYSTEM_INSTALL=true
    CONFIG_DIR="/etc/balikar"
    INSTALL_DIR="/usr/local/bin"
    echo "Installing system-wide..."
else
    SYSTEM_INSTALL=false
    CONFIG_DIR="$HOME/.config/balikar"
    INSTALL_DIR="$HOME/.local/bin"
    echo "Installing for current user..."
fi

# Create configuration directory
echo "Creating configuration directory: $CONFIG_DIR"
mkdir -p "$CONFIG_DIR"

# Copy configuration file
if [ -f "$CONFIG_DIR/repos.conf" ]; then
    echo "Configuration already exists at $CONFIG_DIR/repos.conf (keeping existing)"
else
    echo "Installing default configuration..."
    cp repos.conf "$CONFIG_DIR/repos.conf"
    chmod 644 "$CONFIG_DIR/repos.conf"
    echo "✓ Configuration installed: $CONFIG_DIR/repos.conf"
fi

# Build balikar
echo ""
echo "Building Balikar..."
if [ ! -d "build" ]; then
    cmake -B build
fi
cmake --build build

# Install binary
echo ""
echo "Installing binary to $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
cp build/balikar "$INSTALL_DIR/balikar"
chmod 755 "$INSTALL_DIR/balikar"

# Add to PATH if needed
if [ "$SYSTEM_INSTALL" = false ]; then
    if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
        echo ""
        echo "⚠️  Add $HOME/.local/bin to your PATH:"
        echo "    echo 'export PATH=\"\$HOME/.local/bin:\$PATH\"' >> ~/.bashrc"
        echo "    source ~/.bashrc"
    fi
fi

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Usage:"
echo "  balikar repo list              # List repositories"
echo "  balikar install <package>      # Install package"
echo "  balikar --help                 # Show help"
echo ""
echo "Configuration: $CONFIG_DIR/repos.conf"
echo "Binary: $INSTALL_DIR/balikar"
