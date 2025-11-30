# Docker Testing Environment pro Balikar

Tento adresář obsahuje Docker prostředí pro testování Balikar package manageru v čistém, izolovaném systému.

## Soubory

- `Dockerfile` - Definice Docker image s minimálním Alpine Linux
- `run-docker-test.sh` - Skript pro build a spuštění test kontejneru
- `test-scenarios.sh` - Automatizované testovací scénáře

## Použití

### Rychlý Start

```bash
cd tests/
./run-docker-test.sh
```

Tento příkaz:
1. Sestaví Docker image s Alpine Linux
2. Namountuje projekt do `/balikar`
3. Spustí interaktivní bash shell

### V Kontejneru

Po spuštění kontejneru jste v čistém Alpine Linux prostředí:

```bash
# 1. Zkompilovat balikar
cd /balikar
mkdir -p build && cd build
cmake ..
make

# 2. Otestovat základní funkce
./balikar --help

# 3. Testovat instalaci balíčků
./balikar install --root=/test_root gzip
./balikar remove --root=/test_root gzip
./balikar autoremove --root=/test_root

# 4. Zkontrolovat installed.json
cat /test_root/var/lib/balikar/installed.json
```

## Automatizované Testování

Po zkompilování můžete spustit automatizovanou testovací sadu:

```bash
/balikar/tests/test-scenarios.sh
```

Tento skript otestuje:
- ✓ Základní funkčnost (help, version)
- ✓ Instalaci balíčků s `install_reason` tracking
- ✓ Detekci osiřelých závislostí po odstranění
- ✓ Autoremove funkčnost (iterativní)
- ✓ Varování před odstraněním sdílených závislostí
- ✓ Sdílené závislosti mezi více balíčky
- ✓ Dry-run režim

Test logy jsou uloženy v `/tmp/test*.log`.

## Manuální Testovací Scénáře

Pokud chcete testovat ručně:

### Scénář 1: Čistá Instalace
```bash
# Vytvořit čistý test root
mkdir -p /test_root

# Instalovat bash s automatickým sledováním závislostí
./build/balikar install --root=/test_root bash

# Zkontrolovat install_reason
cat /test_root/var/lib/balikar/installed.json | grep -E '"name"|"install_reason"'
```

### Scénář 2: Remove a Autoremove
```bash
# Odstranit bash
./build/balikar remove --root=/test_root bash

# Mělo by zobrazit osiřelé závislosti
./build/balikar autoremove --root=/test_root

# Opakovat dokud nejsou žádné osiřelé
./build/balikar autoremove --root=/test_root
```

### Scénář 3: Reverse Dependencies
```bash
# Instalovat více balíčků
./build/balikar install --root=/test_root gcc
./build/balikar install --root=/test_root bash

# Pokusit se odstranit glibc (mělo by varovat)
./build/balikar remove --root=/test_root glibc
```

### Scénář 4: Local Package
```bash
# Stáhnout balíček
./build/balikar download --output-dir=/tmp/packages --no-deps gzip

# Instalovat z lokálního souboru
./build/balikar install --root=/test_root \
    --local-package=/tmp/packages/g/gzip/gzip-1.14/gzip-1.14.tar.xz
```

## Prostředí

- **OS**: Alpine Linux (latest)
- **Package Manager**: apk (nekonfliktuje s balikarem)
- **Nainstalované**: gcc, g++, cmake, make, jansson, curl, libarchive
- **Čisté**: Žádné user-space balíčky předinstalovány

## Výhody Docker Testování

1. **Izolace** - Žádné ovlivnění host systému
2. **Opakovatelnost** - Vždy stejné výchozí prostředí
3. **Bezpečnost** - Testování destructive operací
4. **Rychlost** - Alpine je malý a rychlý
5. **CI/CD Ready** - Lze použít v automatizovaných testech

## Cleanup

```bash
# Ukončit kontejner
exit

# Smazat Docker image (pokud je potřeba)
docker rmi balikar-test

# Smazat všechny test kontejnery
docker ps -a | grep balikar-test | awk '{print $1}' | xargs docker rm
```

## Troubleshooting

### libsolv není dostupný v Alpine
Alpine může mít problémy s libsolv. Řešení:
```bash
# V kontejneru
apk add libsolv-dev
```

### Conda konflikt
Pokud máte conda na hostu:
```bash
# Deaktivovat conda před spuštěním
conda deactivate
./run-docker-test.sh
```

### Chybí závislosti při buildu
```bash
# Přidat do Dockerfile:
RUN apk add --no-cache <missing-package>-dev
```

## Advanced Usage

### Mount vlastní repository
```bash
docker run -it --rm \
    -v "$PWD:/balikar" \
    -v "/path/to/custom/repo:/custom_repo" \
    balikar-test \
    /bin/bash
```

### Spustit s konkrétním příkazem
```bash
docker run -it --rm \
    -v "$PWD:/balikar" \
    balikar-test \
    /bin/bash -c "cd /balikar/build && ./balikar --help"
```

### Debug mode
```bash
docker run -it --rm \
    -v "$PWD:/balikar" \
    --cap-add=SYS_PTRACE \
    balikar-test \
    /bin/bash
```

## See Also

- [Main README](../README.md)
- [Manual](../doc/manual.txt)
- [Dependency Tracking](../doc/balikar/dependency_tracking.txt)
