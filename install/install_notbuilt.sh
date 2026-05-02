#!/bin/sh
# Installation script for Balikar Package Manager

set -e

echo "========= Balikar Package Manager - Installation ========="
echo ""

# Check if running as root for system installation
# Use 'id -u' for POSIX compatibility (works in sh, bash, ash, etc.)
if [ "$(id -u)" -eq 0 ]; then
    SYSTEM_INSTALL=true
    CONFIG_DIR="/etc/balikar"
    INSTALL_DIR="/usr/local/bin"
    echo "Installing system-wide (running as root)..."
else
    SYSTEM_INSTALL=false
    CONFIG_DIR="$HOME/.config/balikar"
    INSTALL_DIR="$HOME/.local/bin"
    echo "Installing for current user..."
fi

# Create configuration directory
echo "Creating configuration directory: $CONFIG_DIR"
mkdir -p "$CONFIG_DIR"

# Create database directory for installed packages tracking
if [ "$SYSTEM_INSTALL" = true ]; then
    DB_DIR="/var/lib/balikar"
    CACHE_DIR="/var/cache/balikar"
    echo "Creating package database directory: $DB_DIR"
    mkdir -p "$DB_DIR"
    chmod 755 "$DB_DIR"
    echo "Database directory created: $DB_DIR"
    
    echo "Creating cache directory: $CACHE_DIR"
    mkdir -p "$CACHE_DIR/downloads"
    chmod 755 "$CACHE_DIR" "$CACHE_DIR/downloads"
    echo "Cache directory created: $CACHE_DIR"
else
    DB_DIR="$HOME/.local/share/balikar"
    CACHE_DIR="$HOME/.cache/balikar"
    echo "Creating package database directory: $DB_DIR"
    mkdir -p "$DB_DIR"
    echo "Database directory created: $DB_DIR"
    
    echo "Creating cache directory: $CACHE_DIR"
    mkdir -p "$CACHE_DIR/downloads"
    echo "Cache directory created: $CACHE_DIR"
fi

# Copy configuration file
if [ -f "$CONFIG_DIR/repos.conf" ]; then
    echo "Configuration already exists at $CONFIG_DIR/repos.conf (keeping existing)"
else
    echo "Installing default configuration..."
    cp repos.conf "$CONFIG_DIR/repos.conf"
    chmod 644 "$CONFIG_DIR/repos.conf"
    echo "Configuration installed: $CONFIG_DIR/repos.conf"
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

# Add to PATH if needed (user installation only)
if [ "$SYSTEM_INSTALL" = false ]; then
    PATH_ADDED=false
    
    # Check if already in PATH
    if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
        echo ""
        echo "Adding $HOME/.local/bin to PATH..."
        
        # Detect shell and add to appropriate rc file
        if [ -n "$BASH_VERSION" ]; then
            # Bash
            RC_FILE="$HOME/.bashrc"
            if ! grep -q "export PATH=\"\$HOME/.local/bin:\$PATH\"" "$RC_FILE" 2>/dev/null; then
                echo "" >> "$RC_FILE"
                echo "# Added by Balikar installer" >> "$RC_FILE"
                echo "export PATH=\"\$HOME/.local/bin:\$PATH\"" >> "$RC_FILE"
                PATH_ADDED=true
                echo "Added to $RC_FILE"
            fi
        elif [ -n "$ZSH_VERSION" ]; then
            # Zsh
            RC_FILE="$HOME/.zshrc"
            if ! grep -q "export PATH=\"\$HOME/.local/bin:\$PATH\"" "$RC_FILE" 2>/dev/null; then
                echo "" >> "$RC_FILE"
                echo "# Added by Balikar installer" >> "$RC_FILE"
                echo "export PATH=\"\$HOME/.local/bin:\$PATH\"" >> "$RC_FILE"
                PATH_ADDED=true
                echo "Added to $RC_FILE"
            fi
        else
            # Generic shell
            RC_FILE="$HOME/.profile"
            if ! grep -q "export PATH=\"\$HOME/.local/bin:\$PATH\"" "$RC_FILE" 2>/dev/null; then
                echo "" >> "$RC_FILE"
                echo "# Added by Balikar installer" >> "$RC_FILE"
                echo "export PATH=\"\$HOME/.local/bin:\$PATH\"" >> "$RC_FILE"
                PATH_ADDED=true
                echo "Added to $RC_FILE"
            fi
        fi
        
        if [ "$PATH_ADDED" = true ]; then
            echo ""
            echo "WARNING: Restart your shell or run:"
            echo "    source $RC_FILE"
        else
            echo "PATH already configured"
        fi
    else
        echo "$HOME/.local/bin already in PATH"
    fi
    
    # Temporarily add to current session
    export PATH="$HOME/.local/bin:$PATH"
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
