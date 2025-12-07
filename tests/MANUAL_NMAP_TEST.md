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


