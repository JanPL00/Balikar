# Balikar - Docker Tests

Izolované testovací prostředí pro Balikar package manager.
Jeden `Dockerfile` (Debian bookworm-slim) pokrývá všechny tři testovací skripty.

## Předpoklady

**Docker** verze 20.10 nebo novější.

**Sestavený balikar binary** (jedna ze dvou variant):



**Varianta A — dynamický build** (výchozí, rychlejší):
```bash
cd /cesta/k/proj6_balikar
mkdir -p build && cd build
cmake .. && make balikar
```
Výstup: `build/balikar` — vyžaduje mountování `libsolv.so.1` + `libsolvext.so.1` při každém `docker run`.



**Varianta B — statický build** (doporučeno, bez závislostí):
```bash
export CONDA_PREFIX=/usr/local
./run_scripts/build/build_static.sh
```
Výstup: `./balikar-static` — plně statická binárka, při `docker run` stačí namountovat jen ji.

**Přístup k internetu** pro stažení balíčků z GitHub repository
(`https://raw.githubusercontent.com/JanPL00/balikar-repository/main`).
Alternativa: mount lokálního balikar-repository (viz sekce níže).


---

## Sestavení Docker image

Příkaz spouštějte z **kořene projektu** (Dockerfile potřebuje `balikar` který je tam):

```bash
cd /cesta/k/proj6_balikar

# Pak sestavte image:
docker build -f tests/docker-tests/Dockerfile -t balikar-test .
```

---

---

## Spuštění

Binárka je zabudovaná v image — žádné mounts nejsou potřeba:
```bash
docker run --rm -it balikar-test
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

**Očekávaný výsledek:**

- `resolve nmap` zobrazí plný řetězec závislostí
- `--staging-dir` — obsah staging adresáře ověřitelný po buildu
- `--build-tmp-dir` — build probíhá ve zvolené tmp cestě
- `--no-staging` — instalace přímo bez `DESTDIR` mezikroku
- `--root` — soubory nainstalovány do alternativního kořenového adresáře
- `nmap` úspěšně sestaven a spustitelný

> **Poznámka:** Test 3 je nejdelší. Kompilace celé síťové sady (`zlib`, `openssl`, `libpcap`, `libssh2`, `nmap`) může trvat 20–60 minut. Pro ověření pouze resolveru použijte `--dry-run install nmap`.


## Předinstalovaný SW v kontejneru

| Balíček | Důvod předinstalace |
|---|---|
| `bash` | Potřeba pro `build.sh` skripty z repozitáře (`#!/bin/bash`) |
| `bzip2` | Potřeba pro extrakci `.tar.bz2` archivů (`tar xjf`) |
| `ca-certificates` | Ověření HTTPS certifikátů při stahování balíčků |
| `balikar` (statická) | Zabudovaná přímo v image (`COPY balikar-static`) |

SW pro kompilaci (`gcc`, `make`, `autoconf`, …) **není** předinstalován — instaluje ho balikar sám z balikar-repository jako `no_package=true` závislosti.

SW dostupný v balikar-repository (gzip, ncurses, bash, nmap, ...) **není** předinstalován — instaluje ho balikar jako součást testovaného workflow.

---


## Řešení možných problémů

Balikar byl vyvíjen na systému Debian 12 (Bookworm). V případě přetrvávajících problémů s kompilací/fungováním programu se doporučuje zkompilovat program na systému Debian 12.

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
