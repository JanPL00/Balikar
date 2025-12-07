# Manuální Testování Balíčků v Docker Kontejneru

Návod pro interaktivní testování balikaru v minimálním Debian 12 prostředí.

## Quick Start

### 1. Build Docker Image

```bash
cd /home/placeja5/Balikar/proj6_balikar/tests
docker build -f Dockerfile.minimal -t balikar-minimal-test .
```

### 2. Spusť Interaktivní Kontejner

**Jednorázový test (kontejner se smaže po odpojení):**
```bash
docker run --rm -it \
    -v /home/placeja5/Balikar/proj6_balikar/balikar:/usr/local/bin/balikar:ro \
    -v /home/placeja5/Balikar/proj6_balikar/lib:/usr/local/lib/balikar:ro \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    balikar-minimal-test bash
```

**Perzistentní kontejner (přežije odpojení):**
```bash
docker run -dit \
    --name balikar-test \
    -v /home/placeja5/Balikar/proj6_balikar/balikar:/usr/local/bin/balikar:ro \
    -v /home/placeja5/Balikar/proj6_balikar/lib:/usr/local/lib/balikar:ro \
    -e LD_LIBRARY_PATH=/usr/local/lib/balikar \
    balikar-minimal-test bash

# Připoj se do běžícího kontejneru
docker attach balikar-test

# Odpoj se BEZ ukončení: Ctrl+P, Ctrl+Q
# Připoj se znovu:
docker attach balikar-test
```

## Testování Příkazů

### Základní Příkazy

```bash
# Nápověda
balikar --help

# Aktualizace repo cache
balikar repo-update

# Zobrazení závislostí
balikar resolve nmap
```

### Repository Management

```bash
# Zobrazit nakonfigurované repozitáře
balikar repos
# Expected: Seznam repozitářů s prioritami, URL a enabled stavem
# Currently using: <URL s nejvyšší prioritou>

# Aktualizace remote repozitáře (stáhne repodata.json)
balikar repo-update
# Expected: Successfully downloaded repodata.json

# Aktualizace lokálního repozitáře (regeneruje repodata.json ze všech index.json)
balikar repo-update --repo=./repo
# Expected: repodata.json generated at ./repo/repodata.json

# Přidat nový repozitář
balikar repo add test-repo https://test.example.com/packages 80
# Expected: Added repository: test-repo
# Verify: balikar repos (should show test-repo with priority 80)

# Deaktivovat repozitář (zůstane v konfiguraci, ale nebude se používat)
balikar repo disable balikar-official
# Expected: Repository 'balikar-official' disabled
# Verify: balikar repos (repozitář s další nejvyšší prioritou se stane primárním)

# Aktivovat repozitář
balikar repo enable balikar-official
# Expected: Repository 'balikar-official' enabled
# Verify: balikar repos (repozitář se vrátí jako primární pokud má nejvyšší prioritu)

# Odebrat repozitář
balikar repo remove test-repo
# Expected: Removed repository: test-repo
# Verify: balikar repos (test-repo by neměl být v seznamu)

# Změna priority repozitáře (odebrat a znovu přidat s novou prioritou)
balikar repo remove local-dev
balikar repo add local-dev /path/to/repo 150
# Expected: local-dev má nyní prioritu 150 (stane se primárním repozitářem)
# Verify: balikar repos (local-dev by měl být "Currently using")
```

### Build & Install

```bash
# Build balíčku (bez instalace)
balikar build zlib

# Instalace balíčku (build + install)
balikar install nmap

# Odebrání balíčku
balikar remove nmap
```

### Testování Nmap

Kompletní test včetně závislostí (~5 min):
```bash
balikar install nmap
nmap --version
nmap -p 22,80,443 localhost
```

### Kontrola Instalace

```bash
# Seznam nainstalovaných balíčků
cat /var/lib/balikar/installed.json | grep -o '"name":"[^"]*"' | cut -d'"' -f4
```

## Testovací Scénáře

### Scénář 1: Vlastní Lokální Repozitář

Vytvoření vlastního repozitáře s balíčky stažených z GitHubu.

```bash
# 1. Vytvořit strukturu repozitáře
mkdir -p /tmp/my-repo/{h,z}

# 2. Stáhnout repository archivy z GitHubu
cd /tmp/my-repo/h
curl -L -o hello-1.0.tar.bz2 "https://raw.githubusercontent.com/JanPL00/balikar-repository/main/h/hello/hello-1.0.tar.bz2"
tar -xf hello-1.0.tar.bz2

cd /tmp/my-repo/z
curl -L -o zlib-1.3.1.tar.bz2 "https://raw.githubusercontent.com/JanPL00/balikar-repository/main/z/zlib/zlib-1.3.1.tar.bz2"
tar -xf zlib-1.3.1.tar.bz2

# 3. Vygenerovat repodata.json
balikar repo-update --repo=/tmp/my-repo
# Expected: repodata.json generated

# 4. Přidat jako primární repozitář
balikar repo add my-custom /tmp/my-repo 200
balikar repos
# Expected: Currently using /tmp/my-repo

# 5. Testovat instalaci z vlastního repozitáře
balikar install hello
# Expected: Successfully installed hello

# 6. Vyčistit
balikar repo remove my-custom
rm -rf /tmp/my-repo
```

### Scénář 2: Embedded Source Detection

```bash
# 1. Vyčistit cache a instalovat gzip (má embedded source)
sudo rm -rf /var/cache/balikar/downloads/gzip*
sudo balikar install gzip

# 2. Zkontrolovat logy - mělo by zobrazit:
# [INFO] Using embedded source from repository archive: gzip-1.14.tar.xz

# 3. Ověřit cache
ls -lh /var/cache/balikar/downloads/gzip*
# Expected: gzip-1.14.tar.xz (embedded) a gzip-1.14.tar.bz2 (repo)


## Dependency Chain (nmap)

```
nmap
├── libpcap
├── openssl
├── libssh2
│   ├── openssl
│   └── zlib
└── zlib
```

## Správa Kontejnerů

```bash
# Seznam běžících kontejnerů
docker ps

# Seznam všech kontejnerů
docker ps -a

# Zastavení kontejneru
docker stop balikar-test

# Restart kontejneru
docker start balikar-test

# Smazání kontejneru
docker rm -f balikar-test

# Smazání image
docker rmi balikar-minimal-test
```

## Tipy

- **Odpojení bez ukončení:** `Ctrl+P` → `Ctrl+Q`
- **Build logy:** `/tmp/balikar_build_*/` (během buildu)
- **Cache:** `/var/cache/balikar/downloads/`
- **Installed DB:** `/var/lib/balikar/installed.json`


