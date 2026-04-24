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

docker build -f Dockerfile.test1 -t balikar-test1 .
docker build -f Dockerfile.test2 -t balikar-test2 .
docker build -f Dockerfile.test3 -t balikar-test3 .
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
docker run --rm -it \
    --memory=128m \
    --memory-swap=128m \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib:/usr/local/lib/balikar:ro" \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    balikar-test1
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
docker run --rm -it \
    --memory=148m \
    --memory-swap=148m \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib:/usr/local/lib/balikar:ro" \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    balikar-test2
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
docker run --rm -it \
    --memory=164m \
    --memory-swap=164m \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib:/usr/local/lib/balikar:ro" \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    balikar-test3
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
    -v "${PROJ_ROOT}/lib:/usr/local/lib/balikar:ro" \
    -v "${REPO_PATH}:/repo:ro" \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    balikar-test1 \
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
    --memory=128m \
    --memory-swap=128m \
    -v "${PROJ_ROOT}/balikar:/usr/local/bin/balikar:ro" \
    -v "${PROJ_ROOT}/lib:/usr/local/lib/balikar:ro" \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    balikar-test1 \
    /bin/bash
```

---

## Předinstalovaný SW v kontejnerech

Všechny tři kontejnery sdílí společnou množinu minimálních balíčků. SW dostupný v balikar-repository (gzip, ncurses, bash, nmap, gettext, bison, ...) **není** předinstalován — instaluje ho balikar jako součást testovaného workflow.

| Balíček | Důvod předinstalace |
|---|---|
| `libcurl4` | Runtime balikar (není v balikar-repository) |
| `libjansson4` | Runtime balikar (není v balikar-repository) |
| `libarchive13` | Runtime balikar (není v balikar-repository) |
| `ca-certificates` | HTTPS stažení balíčků (není v balikar-repository) |
| `xz-utils` | Extrakce `.tar.xz` archivů |
| `bzip2` | Extrakce `.tar.bz2` archivů |
| `gcc`, `g++` | Kompilátor C/C++ (`no_package=true` v repository) |
| `make` | Spouštěč Makefile (`no_package=true` v repository) |
| `autoconf`, `automake` | Generování configure skriptů (není v balikar-repository) |

**Kontejner test2** přidá navíc: `gawk`
**Kontejner test3** přidá navíc: `gawk`, `perl`, `python3`, `libtool`, `flex`, `linux-libc-dev`

---

## Řešení problémů

**`balikar: command not found`**
Zkontrolujte, že balikar binary je namountováno na `/usr/local/bin/balikar`.

```bash
ls -l /usr/local/bin/balikar
```

**`error while loading shared libraries`**
Zkontrolujte, že je nastaven `LD_LIBRARY_PATH` a namountováno `/usr/local/lib/balikar` ze složky `lib/`:

```bash
-v "${PROJ_ROOT}/lib:/usr/local/lib/balikar:ro"
-e LD_LIBRARY_PATH=/usr/local/lib/balikar
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
