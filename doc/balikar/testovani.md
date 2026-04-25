# Testování

## Cíle testování a testovací scénáře

Cílem testování projektu Balikar je ověřit správnost klíčových subsystémů
(parser, solver, downloader, builder, installer) pomocí automatizovaných unit
testů a zároveň prověřit chování celé binárky při skutečném použití
prostřednictvím manuálního testování v izolovaném kontejnerovém prostředí.

Testovací strategie je rozdělena do dvou vrstev:

**Vrstva 1 — unit testy** ověřují jednotlivé C funkce v izolaci. Každá
testovací sada pokrývá jeden modul projektu a testy jsou spouštěny automaticky
příkazem `ctest`. Cílem je zachytit chyby na úrovni logiky konkrétní funkce
bez závislosti na síti, souborovém systému nebo externích binárních nástrojích.

**Vrstva 2 — manuální testování přes Docker** prověřuje end-to-end scénáře:
stahování metadat z GitHub repository, resolving závislostí, sestavení zdrojových
kódů, instalaci do systému i do alternativního kořenového adresáře (`--root`),
správu databáze nainstalovaných balíčků a mazání cache. Tyto scénáře nelze
plně pokrýt unit testy, protože vyžadují síťové připojení, archivační nástroje
a plnohodnotný Linux uživatelský prostor.

### Testovací scénáře

| Scénář | Pokrytí |
|--------|---------|
| Základní workflow | `--help`, `repos`, `repo-update`, `resolve`, `--dry-run install`, `download`, `build`, `install`, `clean-cache` |
| Sledování závislostí | Řetězec závislostí `ncurses → readline → bash`, `install_reason` v `installed.json`, `remove`, `autoremove` |
| Pokročilé volby | `--staging-dir`, `--build-tmp-dir`, `--root`, `download --no-deps`, síťové balíčky (`zlib`, `iana-etc`) |
| Izolace funkce | Zpracování JSON metadat, resolver závislostí (libsolv), stahování souborů s cache, validace chroot prostředí |

---

## Unit testování

### Technologie

Unit testy jsou napsány v jazyce C a využívají framework **Check** (libcheck).
Check je standardní C unit testovací framework s podporou test cases, test suites,
setup/teardown fixtur a CK_NORMAL výstupu. Testy jsou integrovány do build systému
**CMake** a spouštěny prostřednictvím **CTest**.

Speciální linker příznak `--wrap=uname` umožňuje mockovat systémové volání
`uname(2)` bez modifikace testovaného kódu — linker nahradí volání reálné
funkce za `__wrap_uname()` definovanou v testovacím souboru. Tato technika se
využívá v `test_system_helpers.c` pro simulaci různých architektur a chybových
stavů.

Všechny testovací soubory jsou kompilovány s přepínačem `-DUNIT_TESTS`, který
v produkčním kódu podmíněně deaktivuje volání `exit()` a jiné vedlejší účinky
nevhodné pro testovací prostředí.

### Spuštění testů

```bash
cd /cesta/k/proj6_balikar
cmake -B build
cmake --build build --target tests
ctest --test-dir build --output-on-failure
```

Nebo přímo:
```bash
./build/tests
```

### Struktura testů

Všechny testovací soubory jsou v adresáři `tests/` a sdílejí společný runner
`tests/main.c`, který vytváří `SRunner` pro každou suite a agreguje počet
selhání. CMake target `tests` kompiluje runner dohromady s testovacími soubory
i testovanými zdrojovými soubory projektu.

```
tests/
├── main.c                          ← společný runner (agreguje všechny suite)
├── tests-utils/
│   ├── test_cli_helpers.c          ← suite: CLI Helpers (4 testy)
│   └── test_system_helpers.c       ← suite: System Helpers (3 testy)
└── tests-src/
    ├── test_parser.c               ← suite: Parser (9 testů)
    ├── test_solver.c               ← suite: Solver (13 testů)
    ├── test_downloader.c           ← suite: Downloader (2 testy)
    ├── test_builder.c              ← suite: Builder (9 testů)
    └── test_installer.c            ← suite: Installer (10 testů)
```

### Přehled testovacích sad

#### CLI Helpers (4 testy)

Testuje funkci `user_confirm()` v `src/utils/cli_helpers.c`. Vstup uživatele
je simulován zápisem do roury (`pipe`) a přesměrováním `stdin` na čtecí konec
roury — tím se eliminuje interaktivní blokování při testování.

| Test | Popis |
|------|-------|
| `test_user_confirm_yes` | Vstup `"y\n"` — funkce vrátí potvrzení |
| `test_user_confirm_no` | Vstup `"n\n"` — funkce vrátí zamítnutí |
| `test_user_confirm_default` | Vstup `"\n"` — funkce vrátí výchozí hodnotu |
| `test_user_confirm_invalid` | Vstup `"x\n"` — funkce se nezhroutí |

#### System Helpers (3 testy)

Testuje funkci `set_system_arch()` v `src/utils/system_helpers.c`. Reálné
volání `uname(2)` je nahrazeno pomocí linker `--wrap=uname` — mock vrací
kontrolovatelné hodnoty bez závislosti na hostitelském systému.

| Test | Popis |
|------|-------|
| `test_set_system_arch_normal` | Mock `uname` vrátí `"x86_64"` — arch je nastaven |
| `test_set_system_arch_fail` | Mock `uname` vrátí `-1` (chyba) — funkce zvládne selhání |
| `test_set_system_arch_invalid_pool` | Předán `NULL` pool — funkce se nezhroutí |

#### Parser (9 testů)

Testuje funkce v `src/balikar/parser.c` odpovědné za čtení JSON metadat
a načítání repository metadat do libsolv poolu. Testy vytvářejí dočasné
adresáře a soubory pomocí `mkdtemp()`/`mkstemp()` a po každém testu je mažou.

Klíčové testované funkce: `load_json()`, `repo_add_conda()`, `load_repository()`,
`generate_repodata()`.

| Test | Popis |
|------|-------|
| `test_load_json_valid` | Platný JSON soubor — vrátí správně parsovaný `json_t*` |
| `test_load_json_invalid` | Neexistující cesta — vrátí `NULL` |
| `test_load_json_malformed` | Syntakticky chybný JSON — vrátí `NULL` |
| `test_repo_add_conda_basic` | `repodata.json` s jedním balíčkem — solvable je přidán do repo |
| + 5 dalších | Načítání repo, integrace s pool, ověření závislostí v metadatech |

#### Solver (13 testů)

Testuje integraci s knihovnou **libsolv** v `src/balikar/solver.c`. Testy
vytvářejí plnohodnotné in-memory Pool/Repo struktury s testovacími balíčky
a jejich závislostmi a ověřují chování resolveru.

Klíčové testované funkce: `create_solver()`, `add_mkdeps_to_queue()`,
`find_install_order()`, `find_packages_to_remove()`.

| Test | Popis |
|------|-------|
| `test_create_solver_valid_pool` | Solver je vytvořen a ukazuje na předaný pool |
| `test_create_solver_null_pool` | `NULL` pool — vrátí `NULL` bez pádu |
| `test_create_solver_flags_set` | Ověří příznaky (`ALLOW_UNINSTALL=1`, `ALLOW_DOWNGRADE=0`) |
| `test_add_mkdeps_simple` | Balík s `mkdeps: [gcc, make]` — závislosti přidány do fronty |
| `test_add_mkdeps_no_mkdeps` | Balík bez `mkdeps` — fronta zůstane prázdná |
| + 8 dalších | Řešení závislostního grafu, pořadí instalace, detekce neřešitelných konfliktů |

#### Downloader (2 testy)

Testuje `download_package()` v `src/balikar/downloader.c`. Testy obcházejí
síťové stahování přímým použitím lokální cesty jako URL (downloader ji
rozpozná jako lokální soubor). Dočasný `$HOME` je nastaven přes `setenv()`
tak, aby cache adresář existoval uvnitř izolovaného dočasného adresáře.

| Test | Popis |
|------|-------|
| `test_download_package_local_success` | Lokální soubor — vrátí otevřený `FILE*`, obsah odpovídá originálnímu souboru |
| `test_download_package_cache_reuse` | Druhé volání se stejným souborem — vrátí data z cache bez opakovaného kopírování |

#### Builder (9 testů)

Testuje funkce v `src/balikar/builder.c` zodpovědné za vyhledání zdrojového
adresáře, zpracování extras a konfiguraci build prostředí.

| Test | Popis |
|------|-------|
| `test_set_staging_enabled` | `set_staging_enabled(0/1)` — funkce existuje a nevyhazuje chybu |
| `test_set_staging_dir` | `set_staging_dir("/cesta")` / `NULL` — správně nastaví a resetuje cestu |
| `test_set_build_tmp_dir` | `set_build_tmp_dir("/cesta")` / `NULL` — správně nastaví a resetuje cestu |
| `test_find_source_path` | Adresář s podadresářem `mysource-1.0` — funkce ho najde |
| `test_find_source_path_nested_with_build` | Struktura `pkg-1.0/src/build.sh` — vrátí `src` jako zdrojový adresář |
| `test_apply_extras` | `extras` pole s `patch1.patch` pro neexistující cestu — funkce se nezhroutí |
| `test_commit_element_simple` | Smoke test konfigurace staging |
| `test_commit_element_is_group` | Minimalistický test group workflow |
| `test_run_with_chroot_basic` | `run_with_chroot(NULL, NULL, NULL)` — vrátí `-1` bez pádu |

#### Installer (10 testů)

Testuje funkce v `src/balikar/installer.c` zodpovědné za validaci chroot
prostředí, správu databáze nainstalovaných balíčků a detekci závislostí.

| Test | Popis |
|------|-------|
| `test_validate_chroot_basic` | Adresářová struktura `usr/bin`, `lib`, `bin` — validace proběhne úspěšně |
| `test_validate_chroot_nonexistent` | Neexistující cesta — vrátí `0` (nevalidní) |
| `test_is_package_installed` | Neexistující balíček — vrátí `0` nebo `1`, nikoliv `-1` |
| `test_get_installed_packages` | Volání pro kořen systému — vrátí JSON pole |
| `test_install_package_basic` | Staging adresář s dummy binárkou — zkopíruje do chroot |
| `test_install_with_script` | Staging s install skriptem — skript je spuštěn |
| `test_find_reverse_deps_basic` | Balík `app` závisí na `lib` — reverse dep je detekována |
| `test_find_orphaned_packages_basic` | Závislost `lib` po `remove app` — detekována jako osiřelá |
| `test_find_orphans_none_when_dep_needed` | `lib` stále potřebná jiným balíkem — není detekována jako osiřelá |
| `test_find_reverse_deps_null_args` | `NULL` argumenty — funkce se nezhroutí |

---

## Manuální testování

### Prostředí

Manuální testování probíhá v Docker kontejneru postaveném na image
`debian:bookworm-slim`. Tato volba zajišťuje:

- nativní **glibc** — testovaná binárka `balikar` je dynamicky linkována
  a vyžaduje glibc (žádné compat shims),
- minimální obraz bez build nástrojů (gcc, make, autoconf nejsou
  předinstalovány — balikar je instaluje sám z repository jako součást
  testovaného workflow),
- reprodukovatelné prostředí nezávislé na hostitelském systému.

Jeden `Dockerfile` v `tests/docker-tests/` nahrazuje původní tři oddělené
obrazy. Obraz obsahuje pouze runtime závislosti binárky:

| Balíček | Důvod |
|---------|-------|
| `libcurl4` | libcurl.so.4 — přímá runtime závislost binárky |
| `libjansson4` | libjansson.so.4 — přímá runtime závislost binárky |
| `libpcre2-posix3` | libpcre2-posix.so.3 — závislost libsolv.so.1 |
| `ca-certificates` | ověření HTTPS certifikátů při stahování balíčků |

Knihovny `libsolv.so.1` a `libsolvext.so.1` nejsou dostupné v Debian
repozitářích — jsou bind-mountovány z adresáře `proj/lib/` při každém
spuštění kontejneru.

### Sestavení a spuštění

```bash
PROJ=/cesta/k/proj6_balikar
cd "$PROJ/tests/docker-tests"

# Sestavení obrazu
docker build -t balikar-alpine .

# Spuštění testu
docker run --rm \
    -v "$PROJ/balikar":/usr/local/bin/balikar:ro \
    -v "$PROJ/lib/libsolv.so.1":/usr/lib/libsolv.so.1:ro \
    -v "$PROJ/lib/libsolvext.so.1":/usr/lib/libsolvext.so.1:ro \
    -v "$PROJ/tests/docker-tests/test1.sh":/test/test1.sh:ro \
    balikar-alpine \
    bash /test/test1.sh
```

Binárka `balikar` a testovací skript jsou mountovány jako read-only svazky.
Kontejner při každém spuštění začíná v čistém stavu — žádné zbytky z předchozích
testů.

### Test 1 — Základní workflow (13 kroků)

Pokrývá základní příkazy popsané v manuálu. Ověřuje, že binárka je dostupná
v `PATH`, načítá metadata z GitHub repository (`repo-update`), resolvuje
závislosti, stahuje archivy a provede plný install cycle balíčku `gzip`.

Pokryté příkazy: `--help`, `repos`, `repo-update`, `-d resolve gzip`,
`resolve hello`, `--dry-run install hello`, `download --no-deps --output-dir`,
`clean-cache --pkg`, `download --output-dir`, `build hello`, `install gzip`,
`clean-cache`.

### Test 2 — Sledování závislostí (12 kroků)

Ověřuje správnost evidence závislostí v databázi nainstalovaných balíčků
(`/var/lib/balikar/installed.json`). Testuje pole `install_reason` — balíčky
instalované jako přímý požadavek uživatele mají hodnotu `"manual"`, balíčky
instalované jako závislosti mají hodnotu `"dependency"`.

Test dále ověřuje příkazy `remove` (odebrání balíčku z databáze),
`autoremove --dry-run` (zobrazení plánu bez akce) a `autoremove` (skutečné
odebrání osiřelých závislostí). Příkaz `install --root` testuje instalaci
do alternativního kořenového adresáře s izolovanou databází.

Řetězec `ncurses → readline → bash` je demonstrován příkazy `resolve` a
`--dry-run install` — plný build tohoto řetězce není součástí automatizovaného
testu, protože ncurses vytváří symlink `/usr/lib/terminfo`, který koliduje se
systémovým adresářem v Debian kontejneru.

### Test 3 — Pokročilé volby (15 kroků)

Pokrývá přepínače `--staging-dir` (vlastní cesta pro staging adresář),
`--build-tmp-dir` (vlastní dočasný adresář pro build) a `--root` (instalace
do alternativního kořene). Testuje stahování balíčků s různými kombinacemi
přepínačů (`download --no-deps`, `download --output-dir`).

Složitý závislostní řetězec `zlib → openssl → libpcap → libssh2 → nmap` je
ověřen pomocí `resolve nmap` a `--dry-run install nmap`. Samotná kompilace
celého řetězce je testována jen pro `zlib` a `iana-etc` (malé balíčky bez
náročných build závislostí). `openssl` a `nmap` vyžadují pro kompilaci perl
a jsou vhodné spíše pro prostředí s neomezenou pamětí.

### Oprava zjištěná během testování

Při testování na Alpine Linuxu (busybox tar) bylo zjištěno, že příkaz pro
extrakci `build.sh` z archivů repository používal přepínač `--wildcards`
(vzor `*/build.sh`), který busybox tar nepodporuje. Oprava v
`src/balikar/downloader.c` nahradila wildcard přesnou cestou ve formátu
`pkg-name-version/build.sh`. GNU tar (Debian) přesnou cestu podporuje
stejně dobře.

---

## Výsledky testování

### Unit testy

```
ctest --test-dir build --output-on-failure

Test project /cesta/k/proj6_balikar/build
    Start 1: unit_tests
1/1 Test #1: unit_tests .......................   Passed

100% tests passed, 0 tests failed out of 1
```

| Suite | Testů | Výsledek |
|-------|-------|----------|
| CLI Helpers | 4 | ✓ všechny prošly |
| System Helpers | 3 | ✓ všechny prošly |
| Parser | 9 | ✓ všechny prošly |
| Solver | 13 | ✓ všechny prošly |
| Downloader | 2 | ✓ všechny prošly |
| Builder | 9 | ✓ všechny prošly |
| Installer | 10 | ✓ všechny prošly |
| **Celkem** | **50** | **50/50 prošlo** |

### Docker testy

| Skript | Kroků | Výsledek |
|--------|-------|----------|
| `test1.sh` | 13 | 13/13 [OK] |
| `test2.sh` | 12 | 12/12 [OK] |
| `test3.sh` | 15 | 15/15 [OK] |
| **Celkem** | **40** | **40/40 [OK]** |

### Analýza paměti

Valgrind analýza příkazu `balikar --help` a `balikar repos`:

```
LEAK SUMMARY:
  definitely lost: 0 bytes in 0 blocks
  indirectly lost: 0 bytes in 0 blocks
  possibly lost:   0 bytes in 0 blocks
  still reachable: 192 bytes in 1 blocks  ← CommandContext, uvolněn při exit()
ERROR SUMMARY: 0 errors from 0 contexts
```

Celkový heap usage při příkazu `--help`: 1 293 alokací, 1 292 uvolnění.
Jeden blok 192 B (CommandContext) je reachable přes globální ukazatel a uvolněn
standardním exit handlerem — nejedná se o leak.

### Výkonnostní metriky

| Měření | Hodnota |
|--------|---------|
| Průměrný čas spuštění (`--help`, 10 měření) | 6 ms |
| Průměrný čas dispatchingu příkazu (`repos`, 5 měření) | 6 ms |
| Velikost binárky (dynamická, s debug) | ~394 KB (403 KB s debug) |
| Velikost binárky (dynamická, stripped) | ~180 KB |
| Velikost binárky (statická, s debug) | **~7,0 MB** |
| Velikost binárky (statická, stripped) | **~6,6 MB** |
| Overhead Command Pattern (statický registr) | ~2 KB |

### Známá omezení

- Příkazy `repo add`, `repo remove`, `repo enable`, `repo disable` jsou
  implementovány částečně — potřebují přímý přístup k `argc`/`argv`, který
  aktuální `CommandContext` neposkytuje. Správu repozitářů lze provádět přímou
  editací konfiguračního souboru `/etc/balikar/repos.conf`.

- Kompilace řetězce `ncurses → readline → bash` v Debian kontejneru selhává
  kvůli konfliktu symlinkového `/usr/lib/terminfo` se systémovým adresářem.
  Funkčnost resolveru závislostí pro tento řetězec je ověřena příkazy
  `resolve` a `--dry-run install`.

- Kompilace `openssl`, `libpcap`, `libssh2` a `nmap` vyžaduje pro perl
  codegen více paměti, než je k dispozici v minimálním testovacím kontejneru.
  Závislostní řetězec je ověřen prostřednictvím `resolve nmap`
  a `--dry-run install nmap`.

---

## Technické specifikace

### Nástroje použité pro měření

Všechna měření níže byla provedena v prostředí **Docker kontejneru**
(`debian:bookworm-slim`) s bind-mountovanou binárkou, čímž se izoluje
hostitelský systém a eliminují vedlejší efekty způsobené jinak spuštěnými
procesy. Použité nástroje:

- **GNU time** (`/usr/bin/time -f "wall=%e s RSS_peak=%M kB CPU=%P"`) —
  měření wall-time, špičkové RSS paměti (Maximum Resident Set Size) a
  procentuálního zatížení CPU pro každý příkaz. Dostupný jako balíček `time`
  v Debianu; odlišný od shellové vestavěné funkce `time`, která tyto podrobnosti
  neposkytuje.

- **`/proc/self/status`** (`VmRSS`) — čtení aktuální RSS paměti bash procesu
  uvnitř kontejneru před a po spuštění příkazu.

- **`file` + `ldd`** (z hostitelského systému) — zjištění formátu ELF binárky
  a úplného stromu dynamických závislostí.

- **`strip --strip-debug`** — vytvoření verze binárky bez debug symbolů pro
  srovnání velikosti produkčního nasazení.

- **`curl -w "size=%{size_download}B time=%{time_total}s speed=%{speed_download}B/s"`** —
  měření přenesených dat, doby stahování a průměrné rychlosti pro každý HTTP
  požadavek na GitHub repository (raw.githubusercontent.com).

- **Python 3** (skript v kapitole) — analýza `repodata.json` a výpočet hloubky
  závislostních řetězců BFS/DFS průchodem grafu závislostí.

#### Příkazy použité pro měření

```bash
# Velikost binárky
ls -lh balikar
strip --strip-debug -o balikar-stripped balikar && ls -lh balikar-stripped

# Dynamické závislosti
file balikar
ldd balikar

# Hloubka závislostních řetězců v repozitáři
python3 dep_analysis.py  # viz skript níže

# Paměť a CPU uvnitř Docker kontejneru
docker run --rm \
  -v "$PROJ/balikar":/usr/local/bin/balikar:ro \
  -v "$PROJ/lib/libsolv.so.1":/usr/lib/libsolv.so.1:ro \
  -v "$PROJ/lib/libsolvext.so.1":/usr/lib/libsolvext.so.1:ro \
  balikar-alpine \
  bash -c '
    apt-get install -y --no-install-recommends time > /dev/null 2>&1
    balikar repo-update > /dev/null 2>&1
    /usr/bin/time -f "  wall=%e s  RSS_peak=%M kB  CPU=%P" balikar resolve libtool 2>&1
  '

# Síťový provoz
curl -s -w "size=%{size_download}B time=%{time_total}s speed=%{speed_download}B/s\n" \
  -o /dev/null \
  "https://raw.githubusercontent.com/JanPL00/balikar-repository/main/repodata.json"
```

#### Skript pro analýzu závislostních řetězců

```python
import json

with open("/home/placeja5/Balikar/balikar-repository/repodata.json") as f:
    data = json.load(f)
packages = data.get("packages", {})

def dep_name(d): return d.split()[0]
pkg_map = {}
for tarname, info in packages.items():
    name = info.get("name")
    if name:
        pkg_map[name] = [dep_name(d) for d in info.get("depends", [])]

def max_depth(start, visited=None, depth=0):
    if visited is None: visited = set()
    if start not in pkg_map or start in visited: return depth
    visited = visited | {start}
    deps = pkg_map[start]
    return depth if not deps else max(max_depth(d, visited, depth+1) for d in deps)

results = sorted([(max_depth(p), p) for p in pkg_map], reverse=True)
for depth, name in results[:10]:
    print(f"  hloubka {depth}: {name}")
```

---

### Velikost binárky na úložišti

Binárka je kompilována ve dvou konfiguracích s výrazně odlišnými výsledky.

#### Dynamicky linkovaná binárka (výchozí build)

```bash
$ cmake -B build && cmake --build build
$ ls -lh balikar
```

| Varianta | Velikost na disku |
|----------|------------------|
| S debug symboly (`-g`, not stripped) | **394 KB** (402 904 B) |
| Stripped (`strip --strip-debug`) | **180 KB** (183 896 B) |

Debug symboly (`-g`) jsou ve výchozí konfiguraci CMake zachovány — binárka
je označena jako `with debug_info, not stripped`. Pro produkční nasazení
postačuje stripped verze.

Binárka je formátu **ELF 64-bit LSB PIE** (Position Independent Executable),
interpretovaný `/lib64/ld-linux-x86-64.so.2`, určen pro GNU/Linux 3.2.0 nebo
novější.

```
$ file balikar
balikar: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
         dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2,
         BuildID[sha1]=7c06a16b..., for GNU/Linux 3.2.0,
         with debug_info, not stripped
```

#### Bundlované sdílené knihovny (`proj/lib/`)

Dynamická binárka vyžaduje čtyři knihovny, které nejsou součástí standardních
distribucí. Jsou distribuovány společně s projektem v adresáři `lib/`:

| Knihovna | Velikost | Poznámka |
|----------|----------|----------|
| `libsolv.so.1` | **690 KB** | Jádro dependency resolveru (libsolv, conda formát) |
| `libsolvext.so.1` | **111 KB** | Rozšíření libsolv (zpracování archivů) |
| `libz.so.1` | **107 KB** | Kompresní knihovna zlib |
| `libpcre2-posix.so.3` | **17 KB** | POSIX regulární výrazy (dep libsolv) |
| **Celkem** | **925 KB** | |

Celková velikost dynamické distribuce (binárka + bundlované knihovny):
~394 KB + ~925 KB = **~1,3 MB** (stripped: ~180 KB + ~925 KB = **~1,1 MB**).

#### Staticky linkovaná binárka (`build_static.sh`)

Statická binárka sestavená skriptem `build_static.sh` na hostitelském systému
(Debian bookworm, glibc, OpenSSL 3) nebo v Alpine prostředí (viz
`tests/docker-tests/alpine/Dockerfile.alpine`) nevyžaduje žádné bundlované
`.so` soubory. Výsledná binárka je soběstačná.

Statický build byl proveden a změřen na hostitelském systému:

```bash
$ bash build_static.sh
$ ls -lh balikar-static
$ strip --strip-debug -o balikar-static-stripped balikar-static
$ ls -lh balikar-static-stripped
```

| Varianta | Velikost na disku |
|----------|------------------|
| S debug symboly (not stripped) | **7,0 MB** (7 252 704 B) |
| Stripped (`strip --strip-debug`) | **6,6 MB** (6 870 728 B) |

```
$ file balikar-static
balikar-static: ELF 64-bit LSB executable, x86-64, version 1 (GNU/Linux),
                statically linked, BuildID[sha1]=fed3100d...,
                for GNU/Linux 3.2.0, with debug_info, not stripped
```

Statická binárka je **ELF 64-bit LSB** (bez PIE — `-static` deaktivuje position
independence), `statically linked`. Výstup `ldd` je `not a dynamic executable`.

Větší velikost oproti dynamické verzi (394 KB → 7,0 MB) je způsobena tím, že
statická binárka obsahuje kompletní kód všech závislostí: libsolv, libsolvext,
libcurl (HTTP/HTTPS), libssl + libcrypto (OpenSSL), libjansson, libarchive a zlib.

#### Celkový strom dynamických závislostí

`ldd` odhaluje úplný strom závislostí při runtime:

```
$ ldd balikar
  libsolv.so.1    → proj/lib/libsolv.so.1     (bundlovaná)
  libsolvext.so.1 → proj/lib/libsolvext.so.1  (bundlovaná)
  libcurl.so.4    → /lib/x86_64-linux-gnu/    (systémová)
  libjansson.so.4 → /lib/x86_64-linux-gnu/    (systémová)
  libc.so.6       → /lib/x86_64-linux-gnu/    (glibc, systémová)
  libpcre2-posix.so.3 → proj/lib/             (bundlovaná)
  libz.so.1       → proj/lib/                 (bundlovaná)
  + 30 tranzitivních závislostí libcurl (libssl, libcrypto, libnghttp2,
    libidn2, libgssapi_krb5, libkrb5, libsasl2, libgnutls, …)
```

Tranzitivní závislosti libcurl jsou standardní systémové knihovny dostupné
v každé moderní glibc distribuci — Debian bookworm, Ubuntu 22.04+, Fedora 36+.

---

### Paměťová náročnost

Měřeno nástrojem **GNU time** (`RSS_peak` = Maximum Resident Set Size)
uvnitř Docker kontejneru (`debian:bookworm-slim`). Repozitář obsahuje
**52 balíčků** s maximální hloubkou závislostního řetězce **6 úrovní**.

#### Závislostní řetězce v repozitáři

Analýzou `repodata.json` byly zjištěny tyto maximální hloubky:

| Balíček | Hloubka | Plný řetězec |
|---------|---------|--------------|
| `libtool` | **6** | libtool → autoconf → perl → m4 → bash → readline → ncurses |
| `automake` | **5** | automake → autoconf → perl → m4 → bash → readline → ncurses |
| `bison` | 4 | bison → gettext → … |
| `autoconf` | 4 | autoconf → perl → m4 → bash |
| `nmap` | 2 | nmap → libpcap → openssl → zlib |

Pro balíček `libtool` (nejhlubší řetězec) resolver identifikuje 8 unikátních
balíčků k instalaci: `perl`, `ncurses`, `readline`, `bash`, `m4`, `autoconf`,
`automake`, `libtool`.

#### Naměřené hodnoty RSS paměti

| Příkaz | RSS špičková paměť | Wall-time | CPU |
|--------|-------------------|-----------|-----|
| `balikar --help` | **8 192 kB** (8 MB) | < 1 ms | 33 % |
| `balikar repos` | **8 320 kB** (8,1 MB) | < 1 ms | 50 % |
| `balikar resolve libtool` (hloubka 6, 8 balíků) | **9 088 kB** (8,9 MB) | < 1 ms | 75 % |
| `balikar --dry-run install libtool` | **9 088 kB** (8,9 MB) | 10 ms | 60 % |
| `balikar repo-update` (HTTP stažení repodata) | **13 312 kB** (13 MB) | 80 ms | 55 % |

Hodnota RSS zahrnuje samotný proces `balikar` i načtené sdílené knihovny
(`libsolv`, `libcurl`, `libjansson`, glibc). Libsolv pool s 52 balíčky
zabírá přibližně **900 kB** paměti navíc oproti klidovému stavu (`--help`).

Špičková paměť při `repo-update` (13 MB) je způsobena inicializací celého
libcurl TLS stacku (OpenSSL, Kerberos, GSSAPI) pro HTTPS spojení — po
dokončení stahování je tato paměť uvolněna.

---

### Síťové zatížení

Veškerá komunikace probíhá přes **HTTPS** na server
`raw.githubusercontent.com` (GitHub CDN). Protokol je HTTP/2 (libcurl +
libnghttp2). Přihlášení není vyžadováno — repository je veřejné.

Měřeno nástrojem `curl -w` z hostitelského systému:

```bash
curl -s -w "size=%{size_download}B time=%{time_total}s speed=%{speed_download}B/s\n" \
  -o /dev/null \
  "https://raw.githubusercontent.com/JanPL00/balikar-repository/main/repodata.json"
```

#### Stahování metadat repozitáře (`repo-update`)

| Soubor | Velikost | Čas | Průměrná rychlost |
|--------|----------|-----|-------------------|
| `repodata.json` | **53 920 B** (52,7 KB) | 0,07 s | ~780 KB/s |

`repo-update` stahuje jediný soubor — `repodata.json` — a ukládá ho do
cache `/var/cache/balikar/downloads/repodata.json`. Při opakovaném volání
(bez smazání cache) je soubor načten z disku bez síťového požadavku.

#### Stahování archivů balíčků (`download`, `install`)

Každý balíček je distribuován jako **tar.bz2 archiv** obsahující
`index.json`, `build.sh`, `.install/install.sh` a zdrojový tarball.
Velikost se výrazně liší podle balíčku:

| Archiv | Velikost na disku | Čas stažení | Průměrná rychlost |
|--------|------------------|-------------|-------------------|
| `hello-1.0.tar.bz2` | **607 B** | 0,22 s | ~2,7 KB/s |
| `gzip-1.14.tar.bz2` | **871 KB** | 0,13 s | ~6,6 MB/s |

Latence (0,07–0,22 s) je dána primárně vzdáleností od GitHub CDN, nikoliv
přenosovou kapacitou. Malý soubor `hello` trvá déle než velký `gzip`
zejména proto, že TLS handshake tvoří dominantní část doby spojení.

Stažené archivy jsou cachovány v `/var/cache/balikar/downloads/`. Při
instalaci balíčku, jehož archiv je již v cache, nedochází k žádnému
síťovému provozu.
