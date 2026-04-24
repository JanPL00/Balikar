# Balikar - Docker Tests

Izolované testovací prostředí pro Balikar package manager.
Jeden `Dockerfile` (Debian bookworm-slim) pokrývá všechny tři testovací skripty.

## Obsah adresáře

| Soubor | Popis |
|---|---|
| `Dockerfile` | Minimální Debian bookworm-slim image pro všechny testy |
| `test1.sh` | Základní workflow (--help, repo-update, resolve, build, install, clean-cache) |
| `test2.sh` | Sledování závislostí, remove, autoremove, install --root |
| `test3.sh` | Pokročilé volby: --staging-dir, --build-tmp-dir, download, install chain |
| `ALPINE.md` | Návod pro spuštění na Alpine Linux (glibc compat workaroundy) |

## Předpoklady

**Docker** verze 20.10 nebo novější.

**Sestavený balikar binary:**

```bash
cd /cesta/k/proj6_balikar
mkdir -p build && cd build
cmake .. && make balikar
```

Po úspěšném buildu existuje soubor `proj6_balikar/build/balikar` nebo `proj6_balikar/balikar` (prebuilt).

**Přístup k internetu** pro stažení balíčků z GitHub repository
(`https://raw.githubusercontent.com/JanPL00/balikar-repository/main`).
Alternativa: mount lokálního balikar-repository (viz sekce níže).

---

## Sestavení Docker images

Všechny příkazy spouštějte z adresáře `docker-tests/`:

```bash
PROJ_ROOT=/cesta/k/proj6_balikar
cd "$PROJ_ROOT/tests/docker-tests"

docker build -t balikar-alpine .
```

---

## Test 1 — Základní workflow (128 MB)

**Doporučené příkazy z manuálu:**

- `balikar --help`
- `balikar repos`
- `balikar repo-update`
- `balikar -d resolve gzip`
- `balikar resolve hello`
- `balikar --dry-run install hello`
- `balikar download --no-deps --output-dir=/tmp/dl_test gzip`
- `balikar clean-cache --pkg=gzip`
- `balikar download --output-dir=/tmp/dl_test gzip`
- `balikar build hello`
- `balikar install gzip`
- `balikar clean-cache`

**Spuštění:**

```bash
docker run --rm \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib/libsolv.so.1:/usr/lib/libsolv.so.1:ro" \
    -v "${PROJ_ROOT}/lib/libsolvext.so.1:/usr/lib/libsolvext.so.1:ro" \
    -v "${PROJ_ROOT}/tests/docker-tests/test1.sh:/test/test1.sh:ro" \
    balikar-alpine \
    bash /test/test1.sh
```

**Očekávaný výsledek:** Všechny kroky projdou bez chyby. `hello` a `gzip` jsou úspěšně sestaveny a nainstalovány.

---

## Test 2 — Sledování závislostí (148 MB)

**Doporučené příkazy z manuálu:**

- `balikar resolve bash` — řetězec: `ncurses` → `readline` → `bash`
- `balikar -d resolve readline`
- `balikar --dry-run install bash`
- `balikar install bash --root=/test_root`
- ověření `install_reason` v `installed.json`
- `balikar remove bash --root=/test_root`
- `balikar autoremove --dry-run --root=/test_root`
- `balikar autoremove --root=/test_root`
- `balikar install readline --root=/test_root`
- `balikar remove ncurses --root=/test_root` — varování o závislosti

**Spuštění:**

```bash
docker run --rm \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib/libsolv.so.1:/usr/lib/libsolv.so.1:ro" \
    -v "${PROJ_ROOT}/lib/libsolvext.so.1:/usr/lib/libsolvext.so.1:ro" \
    -v "${PROJ_ROOT}/tests/docker-tests/test2.sh:/test/test2.sh:ro" \
    balikar-alpine \
    bash /test/test2.sh
```

**Očekávaný výsledek:**

- `bash` je nainstalován jako `"manual"`, `ncurses` a `readline` jako `"dependency"`
- `remove bash` navrhne autoremove pro osiřelé `ncurses` a `readline`
- `autoremove --dry-run` zobrazí plán bez akce
- `autoremove` skutečně odstraní osiřelé balíčky
- `remove ncurses` zobrazí varování (používá ho `readline`)

> **Poznámka:** Kompilace celého řetězce `ncurses` → `readline` → `bash` trvá 5–15 minut.

---

## Test 3 — Pokročilé volby a síťové balíčky (164 MB)

**Doporučené příkazy z manuálu:**

- `balikar resolve nmap` — řetězec: `zlib` + `openssl` + `libpcap` + `libssh2`
- `balikar --dry-run install nmap`
- `balikar download --no-deps --output-dir=/tmp/sources gzip`
- `balikar download --no-deps --output-dir=/tmp/sources zlib`
- `balikar download --output-dir=/tmp/sources gzip`
- `balikar clean-cache --pkg=gzip`
- `balikar --staging-dir=/tmp/stage build hello`
- `balikar --build-tmp-dir=/var/tmp/builds build gzip`
- `balikar --no-staging build hello`
- `balikar install gzip --root=/test_root`
- `balikar install zlib` / `openssl` / `libpcap` / `libssh2` / `nmap`
- `balikar clean-cache`

**Spuštění:**

```bash
docker run --rm \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib/libsolv.so.1:/usr/lib/libsolv.so.1:ro" \
    -v "${PROJ_ROOT}/lib/libsolvext.so.1:/usr/lib/libsolvext.so.1:ro" \
    -v "${PROJ_ROOT}/tests/docker-tests/test3.sh:/test/test3.sh:ro" \
    balikar-alpine \
    bash /test/test3.sh
```

**Očekávaný výsledek:**

- `resolve nmap` zobrazí plný řetězec závislostí
- `--staging-dir` — obsah staging adresáře ověřitelný po buildu
- `--build-tmp-dir` — build probíhá ve zvolené tmp cestě
- `--no-staging` — instalace přímo bez `DESTDIR` mezikroku
- `--root` — soubory nainstalovány do alternativního kořenového adresáře
- `nmap` úspěšně sestaven a spustitelný

> **Poznámka:** Test 3 je nejdelší. Kompilace celé síťové sady (`zlib`, `openssl`, `libpcap`, `libssh2`, `nmap`) může trvat 20–60 minut. Pro ověření pouze resolveru použijte `--dry-run install nmap`.

---

## Použití lokálního repository (offline režim)

```bash
REPO_PATH=/cesta/k/balikar-repository

docker run --rm -it \
    --memory=128m \
    --memory-swap=128m \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib/libsolv.so.1:/usr/lib/libsolv.so.1:ro" \
    -v "${PROJ_ROOT}/lib/libsolvext.so.1:/usr/lib/libsolvext.so.1:ro" \
    -v "${REPO_PATH}:/repo:ro" \
    balikar-alpine \
    /bin/bash -c "
        printf '[local]\nurl = /repo\npriority = 100\nenabled = true\n' \
            > /etc/balikar/repos.conf
        /test/run_tests.sh
    "
```

---

## Interaktivní spuštění (bez automatického skriptu)

```bash
docker run --rm -it \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib/libsolv.so.1:/usr/lib/libsolv.so.1:ro" \
    -v "${PROJ_ROOT}/lib/libsolvext.so.1:/usr/lib/libsolvext.so.1:ro" \
    balikar-alpine \
    /bin/bash
```

---

## Předinstalovaný SW v kontejnerech

Všechny tři kontejnery sdílí společnou množinu minimálních balíčků. SW dostupný v balikar-repository (gzip, ncurses, bash, nmap, gettext, bison, ...) **není** předinstalován — instaluje ho balikar jako součást testovaného workflow.

| Balíček | Důvod předinstalace |
|---|---|
| `libcurl4` | Runtime balikar — libcurl.so.4 (přímá dep binárky) |
| `libjansson4` | Runtime balikar — libjansson.so.4 (přímá dep binárky) |
| `libpcre2-posix3` | Dep libsolv.so.1 — libpcre2-posix.so.3 (libsolv není v Debian repech) |
| `ca-certificates` | Ověření HTTPS certifikátů při stahování balíčků |

`libsolv.so.1` a `libsolvext.so.1` **nejsou** v Debian repech — mountují se z `proj/lib/` při každém `docker run` (viz příkazy výše).

SW pro kompilaci (`gcc`, `make`, `autoconf`, …) **není** předinstalován — instaluje ho balikar sám z balikar-repository jako `no_package=true` závislosti.

---

## Řešení problémů

**`balikar: command not found`**
Zkontrolujte, že balikar binary je namountováno na `/usr/local/bin/balikar`.

```bash
ls -l /usr/local/bin/balikar
```

**`error while loading shared libraries`**
Namountujte `libsolv.so.1` a `libsolvext.so.1` z `proj/lib/` (nejsou v Debian repech):

```bash
-v "${PROJ_ROOT}/lib/libsolv.so.1:/usr/lib/libsolv.so.1:ro"
-v "${PROJ_ROOT}/lib/libsolvext.so.1:/usr/lib/libsolvext.so.1:ro"
```

**`Package not found in pool`**
Repository se nenačítá. Ověření:

```bash
balikar repos
```

Pro offline režim viz sekci *Použití lokálního repository*.

**Kontejner se ukončuje z důvodu OOM**
Zvyšte RAM limit o 16–32 MB. Základní kompilace `ncurses`, `readline` a `bash` vyžaduje přibližně 128–148 MB. `openssl` a `nmap` vyžadují 148–164 MB.

**Build selže s `make: command not found`**
Ověřte, že `make` je dostupný v kontejneru:

```bash
which make
```

**Download selže (síťová chyba)**
Zkontrolujte přístup k internetu. Pro offline testování použijte lokální repository mount.
