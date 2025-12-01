#!/bin/bash
# Simplified test pro dependency tracking - bez skutečného buildu
# Testuje pouze install_reason tracking a dependency management

set -e

BALIKAR="/balikar/build/balikar"
TEST_ROOT="/test_root_simple"
REPO_PATH="/repo"

# Barvy
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Dependency Tracking Test (Simplified - No Build)${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo ""

step() { echo -e "\n${YELLOW}$1${NC}\n"; }
success() { echo -e "${GREEN}$1${NC}"; }
error() { echo -e "${RED}$1${NC}"; }
info() { echo -e "${BLUE}$1${NC}"; }

# Cleanup
step "Příprava: Vytváření mock installed.json s předinstalovanými balíčky"
rm -rf "$TEST_ROOT"
mkdir -p "$TEST_ROOT/var/lib/balikar"

# Vytvoříme mock installed.json s již "nainstalovanými" balíčky
# Simulujeme že bash, glibc, readline, ncurses jsou již nainstalované
cat > "$TEST_ROOT/var/lib/balikar/installed.json" << 'EOF'
{
  "packages": [
    {
      "name": "glibc",
      "version": "2.42",
      "install_reason": "dependency",
      "installed_at": "2025-11-30T20:00:00Z",
      "files": []
    },
    {
      "name": "ncurses",
      "version": "6.5",
      "install_reason": "dependency",
      "installed_at": "2025-11-30T20:00:01Z",
      "files": []
    },
    {
      "name": "readline",
      "version": "8.2",
      "install_reason": "dependency",
      "installed_at": "2025-11-30T20:00:02Z",
      "files": []
    },
    {
      "name": "bash",
      "version": "5.2",
      "install_reason": "manual",
      "installed_at": "2025-11-30T20:00:03Z",
      "files": []
    }
  ]
}
EOF

success "Mock installed.json vytvořen"
info "Obsah:"
cat "$TEST_ROOT/var/lib/balikar/installed.json" | grep -E '"name"|"install_reason"'

# Test 1: Kontrola installed.json struktury
step "TEST 1: Kontrola install_reason polí"

bash_reason=$(cat "$TEST_ROOT/var/lib/balikar/installed.json" | grep -A5 '"name": "bash"' | grep install_reason | cut -d'"' -f4)
glibc_reason=$(cat "$TEST_ROOT/var/lib/balikar/installed.json" | grep -A5 '"name": "glibc"' | grep install_reason | cut -d'"' -f4)

if [ "$bash_reason" = "manual" ]; then
    success "bash má install_reason=manual"
else
    error "bash nemá správný install_reason (očekáváno: manual, je: $bash_reason)"
fi

if [ "$glibc_reason" = "dependency" ]; then
    success "glibc má install_reason=dependency"
else
    error "glibc nemá správný install_reason (očekáváno: dependency, je: $glibc_reason)"
fi

# Test 2: Remove bash - měl by detekovat osiřelé
step "TEST 2: Odstranění bash a detekce osiřelých"

# Restore full installed.json for this test
cat > "$TEST_ROOT/var/lib/balikar/installed.json" << 'EOFRESTORE'
{
  "packages": [
    {
      "name": "glibc",
      "version": "2.42",
      "install_reason": "dependency",
      "installed_at": "2025-11-30T20:00:00Z",
      "files": []
    },
    {
      "name": "ncurses",
      "version": "6.5",
      "install_reason": "dependency",
      "installed_at": "2025-11-30T20:00:01Z",
      "files": []
    },
    {
      "name": "readline",
      "version": "8.2",
      "install_reason": "dependency",
      "installed_at": "2025-11-30T20:00:02Z",
      "files": []
    },
    {
      "name": "bash",
      "version": "5.2",
      "install_reason": "manual",
      "installed_at": "2025-11-30T20:00:03Z",
      "files": []
    }
  ]
}
EOFRESTORE

$BALIKAR remove --root="$TEST_ROOT" --repo="$REPO_PATH" bash 2>&1 | tee /tmp/test_remove.log

if grep -q "no longer required" /tmp/test_remove.log; then
    success "Detekce osiřelých závislostí funguje"
    info "Osiřelé balíčky:"
    grep -A 5 "no longer required" /tmp/test_remove.log || true
else
    error "Detekce osiřelých závislostí nefunguje"
    info "Log output:"
    cat /tmp/test_remove.log
fi

# Test 3: Autoremove
step "TEST 3: Autoremove osiřelých závislostí"

echo "y" | $BALIKAR autoremove --root="$TEST_ROOT" --repo="$REPO_PATH" 2>&1 | tee /tmp/test_autoremove.log

if grep -q -E "(orphaned package|Removing)" /tmp/test_autoremove.log; then
    success "Autoremove našel a odstranil osiřelé balíčky"
else
    info "Autoremove nenašel osiřelé balíčky (možná již odstraněny)"
fi

# Test 4: Reverse dependencies - přidáme bash zpět a zkusíme odstranit glibc
step "TEST 4: Reverse dependencies - ochrana proti rozbití"

# Přidáme bash zpět do installed.json
cat > "$TEST_ROOT/var/lib/balikar/installed.json" << 'EOF'
{
  "packages": [
    {
      "name": "glibc",
      "version": "2.42",
      "install_reason": "dependency",
      "installed_at": "2025-11-30T20:00:00Z",
      "files": []
    },
    {
      "name": "bash",
      "version": "5.2",
      "install_reason": "manual",
      "installed_at": "2025-11-30T20:00:03Z",
      "files": []
    }
  ]
}
EOF

info "Pokus o odstranění glibc (mělo by varovat)..."
echo "n" | $BALIKAR remove --root="$TEST_ROOT" --repo="$REPO_PATH" glibc 2>&1 | tee /tmp/test_reverse_deps.log

if grep -q "required by" /tmp/test_reverse_deps.log; then
    success "Reverse dependencies warning funguje"
    info "Závislé balíčky:"
    grep -A 5 "required by" /tmp/test_reverse_deps.log || true
else
    error "Reverse dependencies warning nefunguje"
    info "Log output:"
    cat /tmp/test_reverse_deps.log
fi

# Finální report
echo ""
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Test Summary${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo ""

if [ -f "$TEST_ROOT/var/lib/balikar/installed.json" ]; then
    info "Finální stav installed.json:"
    cat "$TEST_ROOT/var/lib/balikar/installed.json" | grep '"name"' | wc -l | xargs echo "Počet nainstalovaných balíčků:"
    cat "$TEST_ROOT/var/lib/balikar/installed.json" | grep '"name"' | sed 's/.*"name": "\(.*\)".*/  - \1/' || true
fi

echo ""
echo -e "${GREEN}Dependency tracking testy dokončeny!${NC}"
echo ""
