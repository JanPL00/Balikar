#!/bin/sh
# Installation script for pre-built Balikar Package Manager
# Use this script to install already built balikar binary

set -e

echo "========= Balikar Package Manager - Pre-built Installation ========="
echo ""

# Check if balikar binary exists
if [ ! -f "balikar" ]; then
    echo "ERROR: Pre-built binary not found at ./balikar"
    echo "Please build Balikar first:"
    echo "  cmake -B build"
    echo "  cmake --build build"
    echo ""
    echo "Or use the full build_and_install.sh script instead."
    exit 1
fi

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
    echo "Creating package database directory: $DB_DIR"
    mkdir -p "$DB_DIR"
    chmod 755 "$DB_DIR"
    echo "Database directory created: $DB_DIR"
else
    DB_DIR="$HOME/.local/share/balikar"
    echo "Creating package database directory: $DB_DIR"
    mkdir -p "$DB_DIR"
    echo "Database directory created: $DB_DIR"
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

# Install bundled libraries if present
if [ -d "lib" ] && [ "$(ls -A lib 2>/dev/null)" ]; then
    echo ""
    echo "Installing bundled libraries..."
    
    if [ "$SYSTEM_INSTALL" = true ]; then
        LIB_DIR="/usr/local/lib/balikar"
    else
        LIB_DIR="$HOME/.local/lib/balikar"
    fi
    
    mkdir -p "$LIB_DIR"
    cp -v lib/*.so* "$LIB_DIR/" 2>/dev/null || true
    echo "Libraries installed to: $LIB_DIR"
    ls -lh "$LIB_DIR/"
else
    echo ""
    echo "Note: No bundled libraries found (lib/ directory missing)"
    echo "Binary will use system libraries"
fi

# Install binary
echo ""
echo "Installing binary to $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
cp -v balikar "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR/balikar"

# Set RPATH to find bundled libraries
# Use absolute path since binary is in /usr/local/bin and libs in /usr/local/lib/balikar
if [ -d "lib" ] && [ "$(ls -A lib 2>/dev/null)" ]; then
    if command -v patchelf >/dev/null 2>&1; then
        echo "Setting RPATH with patchelf..."
        if [ "$SYSTEM_INSTALL" = true ]; then
            patchelf --set-rpath '/usr/local/lib/balikar' "$INSTALL_DIR/balikar"
            echo "RPATH set to: /usr/local/lib/balikar"
        else
            patchelf --set-rpath "$HOME/.local/lib/balikar" "$INSTALL_DIR/balikar"
            echo "RPATH set to: $HOME/.local/lib/balikar"
        fi
    else
        echo "WARNING: patchelf not found, cannot update RPATH"
        echo "Install patchelf: sudo apt install patchelf"
        echo ""
        echo "Alternative: Add library path to ld.so.conf"
        if [ "$SYSTEM_INSTALL" = true ]; then
            echo "  echo '/usr/local/lib/balikar' | sudo tee /etc/ld.so.conf.d/balikar.conf"
            echo "  sudo ldconfig"
        fi
    fi
fi

echo "Binary successfully installed to: $INSTALL_DIR/balikar"
ls -lh "$INSTALL_DIR/balikar"

# If patchelf failed and we have bundled libs, configure ld.so
if [ -d "lib" ] && [ "$(ls -A lib 2>/dev/null)" ]; then
    if ! command -v patchelf >/dev/null 2>&1 && [ "$SYSTEM_INSTALL" = true ]; then
        echo ""
        echo "Configuring dynamic linker for bundled libraries..."
        echo "/usr/local/lib/balikar" > /etc/ld.so.conf.d/balikar.conf
        ldconfig
        echo "Added /usr/local/lib/balikar to ld.so.conf"
    fi
fi

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
echo "Database: $DB_DIR"
