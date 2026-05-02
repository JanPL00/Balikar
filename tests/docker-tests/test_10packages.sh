#!/bin/bash
# Test installation of 10 packages via balikar in clean Docker container.
# Run inside container: bash /test/test_10packages.sh

set +e
# Order chosen so that packages whose installation could overlay shared
# objects in /usr come last (xz overlays /usr/local/bin/xz only, but we
# play it safe and run it after the autotools/perl chain).
PACKAGES=(hello zlib gzip patch m4 perl autoconf automake libtool xz)

# Force cache refresh so we pull the freshly pushed repodata.json
rm -f /var/cache/balikar/downloads/repodata.json
# Wipe any previously cached source tarballs/extras so we always exercise
# the freshly-pushed package metadata and build scripts.
rm -rf /var/cache/balikar/downloads/extras
rm -f  /var/cache/balikar/downloads/*.tar.* 2>/dev/null || true

PASS=()
FAIL=()
LOGDIR=/test/logs
mkdir -p "$LOGDIR"

for pkg in "${PACKAGES[@]}"; do
    echo "============================================================"
    echo ">>> Installing $pkg"
    echo "============================================================"
    # Pipe 'y' answers to auto-confirm prompts
    if yes y | balikar install "$pkg" >"$LOGDIR/$pkg.log" 2>&1; then
        echo "[OK] $pkg"
        PASS+=("$pkg")
    else
        echo "[FAIL] $pkg (exit $?)"
        tail -40 "$LOGDIR/$pkg.log"
        FAIL+=("$pkg")
    fi
done

echo
echo "============================================================"
echo "SUMMARY"
echo "============================================================"
echo "Passed (${#PASS[@]}/10): ${PASS[*]}"
echo "Failed (${#FAIL[@]}/10): ${FAIL[*]}"
[[ ${#FAIL[@]} -eq 0 ]]
