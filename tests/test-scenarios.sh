#!/bin/bash
# Automatické testovací scénáře pro Balikar v Docker prostředí

set -e

BALIKAR="/balikar/build/balikar"
TEST_ROOT="/test_root"

# Barvy pro výstup
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Balikar Package Manager - Automated Test Suite${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo ""

# Funkce pro zobrazení kroku
step() {
    echo -e "\n${YELLOW}▶ $1${NC}\n"
}

# Funkce pro úspěch
success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Funkce pro chybu
error() {
    echo -e "${RED}✗ $1${NC}"
}

# Funkce pro info
info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Kontrola, zda balikar existuje
if [ ! -f "$BALIKAR" ]; then
    error "Balikar binary not found at $BALIKAR"
    echo "Please build balikar first:"
    echo "  cd /balikar && mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

# Test 1: Základní funkčnost
step "TEST 1: Základní příkazy"
$BALIKAR --help > /dev/null && success "Help command works" || error "Help command failed"

# Cleanup předchozích testů
step "Cleanup: Odstraňuji předchozí test_root"
rm -rf "$TEST_ROOT"
mkdir -p "$TEST_ROOT"
success "Test root cleaned: $TEST_ROOT"

# Test 2: Instalace balíčku s dependency tracking
step "TEST 2: Instalace bash s dependency tracking"
echo "y" | $BALIKAR install --root="$TEST_ROOT" bash 2>&1 | tee /tmp/test2.log

if [ -f "$TEST_ROOT/var/lib/balikar/installed.json" ]; then
    success "installed.json vytvořen"
    
    # Kontrola install_reason
    info "Kontrola install_reason polí:"
    cat "$TEST_ROOT/var/lib/balikar/installed.json" | grep -E '"name"|"install_reason"' | head -20
    
    # Ověření, že bash má reason=manual
    if grep -q '"name": "bash"' "$TEST_ROOT/var/lib/balikar/installed.json"; then
        success "bash je nainstalován"
        
        # Další řádek po bash by měl obsahovat install_reason
        if grep -A 1 '"name": "bash"' "$TEST_ROOT/var/lib/balikar/installed.json" | grep -q '"install_reason": "manual"'; then
            success "bash má install_reason=manual"
        else
            error "bash nemá správný install_reason"
        fi
    else
        error "bash není v installed.json"
    fi
    
    # Ověření, že glibc má reason=dependency
    if grep -A 1 '"name": "glibc"' "$TEST_ROOT/var/lib/balikar/installed.json" | grep -q '"install_reason": "dependency"'; then
        success "glibc má install_reason=dependency"
    else
        error "glibc nemá správný install_reason"
    fi
else
    error "installed.json nebyl vytvořen"
fi

# Test 3: Remove s detekcí osiřelých
step "TEST 3: Odstranění bash a detekce osiřelých"
$BALIKAR remove --root="$TEST_ROOT" bash 2>&1 | tee /tmp/test3.log

if grep -q "no longer required" /tmp/test3.log; then
    success "Detekce osiřelých závislostí funguje"
    info "Osiřelé balíčky:"
    grep -A 5 "no longer required" /tmp/test3.log
else
    error "Detekce osiřelých závislostí nefunguje"
fi

# Test 4: Autoremove
step "TEST 4: Autoremove osiřelých závislostí"
echo "y" | $BALIKAR autoremove --root="$TEST_ROOT" 2>&1 | tee /tmp/test4.log

if grep -q "orphaned package" /tmp/test4.log; then
    success "Autoremove našel osiřelé balíčky"
    
    # Opakovat autoremove dokud nejsou žádné osiřelé
    MAX_ITERATIONS=10
    iteration=1
    while [ $iteration -le $MAX_ITERATIONS ]; do
        info "Autoremove iterace $iteration"
        echo "y" | $BALIKAR autoremove --root="$TEST_ROOT" 2>&1 | tee /tmp/test4_iter$iteration.log
        
        if grep -q "No orphaned packages" /tmp/test4_iter$iteration.log; then
            success "Všechny osiřelé balíčky odstraněny po $iteration iteracích"
            break
        fi
        
        iteration=$((iteration + 1))
    done
    
    if [ $iteration -gt $MAX_ITERATIONS ]; then
        error "Autoremove neodstranil všechny osiřelé po $MAX_ITERATIONS iteracích"
    fi
else
    error "Autoremove nenašel žádné osiřelé balíčky"
fi

# Test 5: Reverse dependencies warning
step "TEST 5: Reverse dependencies - ochrana proti rozbití"
info "Znovu instaluji bash..."
echo "y" | $BALIKAR install --root="$TEST_ROOT" bash > /dev/null 2>&1

info "Pokus o odstranění glibc (mělo by varovat)..."
echo "n" | $BALIKAR remove --root="$TEST_ROOT" glibc 2>&1 | tee /tmp/test5.log

if grep -q "required by the following installed packages" /tmp/test5.log; then
    success "Reverse dependencies warning funguje"
    info "Závislé balíčky:"
    grep -A 10 "required by" /tmp/test5.log | head -15
else
    error "Reverse dependencies warning nefunguje"
fi

if grep -q "Removal cancelled" /tmp/test5.log; then
    success "Odstranění správně zrušeno"
else
    error "Odstranění by mělo být zrušeno"
fi

# Test 6: Více manuálních balíčků se sdílenou závislostí
step "TEST 6: Sdílené závislosti mezi balíčky"
info "Instaluji gzip (také potřebuje glibc)..."
echo "y" | $BALIKAR install --root="$TEST_ROOT" gzip > /dev/null 2>&1

info "Odstraňuji bash..."
$BALIKAR remove --root="$TEST_ROOT" bash 2>&1 | tee /tmp/test6.log

if grep -q "no longer required" /tmp/test6.log; then
    success "Detekce osiřelých po odstranění bash"
    
    # glibc by NEMĚL být osiřelý (gzip ho stále potřebuje)
    info "Kontrola že glibc NENÍ osiřelý..."
    echo "y" | $BALIKAR autoremove --root="$TEST_ROOT" 2>&1 | tee /tmp/test6_autoremove.log
    
    if ! grep -q '"glibc"' /tmp/test6_autoremove.log || grep -q "No orphaned packages" /tmp/test6_autoremove.log; then
        success "glibc správně NENÍ mezi osiřelými (gzip ho stále potřebuje)"
    else
        error "glibc by neměl být osiřelý"
    fi
else
    info "Žádné osiřelé závislosti (možná readline už není potřeba)"
fi

# Test 7: Dry-run autoremove
step "TEST 7: Dry-run režim"
$BALIKAR autoremove --dry-run --root="$TEST_ROOT" 2>&1 | tee /tmp/test7.log

if grep -q "DRY-RUN" /tmp/test7.log || grep -q "No orphaned packages" /tmp/test7.log; then
    success "Dry-run režim funguje"
else
    error "Dry-run režim nefunguje"
fi

# Finální report
echo ""
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Test Summary${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo ""

info "Test logs jsou uloženy v /tmp/test*.log"
info "Finální stav installed.json:"
if [ -f "$TEST_ROOT/var/lib/balikar/installed.json" ]; then
    cat "$TEST_ROOT/var/lib/balikar/installed.json" | grep '"name"' | wc -l | xargs echo "Počet nainstalovaných balíčků:"
    cat "$TEST_ROOT/var/lib/balikar/installed.json" | grep '"name"' | sed 's/.*"name": "\(.*\)".*/  - \1/'
else
    echo "installed.json neexistuje nebo je prázdný"
fi

echo ""
echo -e "${GREEN}✓ Všechny testy dokončeny!${NC}"
echo ""
echo "Pro ruční testování:"
echo "  $BALIKAR --help"
echo "  $BALIKAR install --root=$TEST_ROOT <package>"
echo "  $BALIKAR remove --root=$TEST_ROOT <package>"
echo "  $BALIKAR autoremove --root=$TEST_ROOT"
echo ""
