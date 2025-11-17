# Podpora pro vzdálené repozitáře v Balikaru

## Přehled

Balikar nyní podporuje stahování balíčků ze vzdálených repozitářů (GitHub, HTTP/HTTPS servery) místo pouze lokálních souborů. Tato funkce umožňuje distribuci balíčků přes internet podobně jako apt, pacman nebo conda.

## Struktura metadat balíčku

### Nový formát: `source[]` array

Každý balíček může obsahovat pole `source` s informacemi o zdrojových souborech:

```json
{
  "name": "gzip",
  "version": "1.14",
  "download_url": "https://github.com/USER/balikar-repo/raw/main/repo/g/gzip/gzip-1.14.tar.bz2",
  "source": [{
    "url": "https://ftp.gnu.org/gnu/gzip/gzip-1.14.tar.xz",
    "sha256": "4bf5a10f287501ee8e8ebe00ef62b2c2"
  }]
}
```

### Pole metadat

- **`download_url`**: URL předkompilovaného balíčku (pro budoucí použití s binárními balíčky)
- **`source`**: Array objektů se zdrojovými soubory
  - **`url`**: Přímý odkaz na zdrojový tarball (tar.gz, tar.xz, tar.bz2)
  - **`sha256`**: SHA-256 hash pro verifikaci integrity souboru

### Zpětná kompatibilita

Balikar stále podporuje starý formát s `source` jako řetězcem:

```json
{
  "source": "repo/path/to/package.tar.bz2"
}
```

## Workflow stahování

1. **Parser načte metadata**: Při načítání repozitáře parser ukládá `source[]` array do libsolv repodata
2. **Downloader preferuje `source[0].url`**: Při stahování balíčku se použije první URL ze `source[]` array
3. **SHA256 verifikace**: Stažený soubor se automaticky ověří proti `sha256` z metadat
4. **Cache**: Ověřené soubory se ukládají do `/var/cache/balikar/downloads/` pro budoucí použití

## Příklad použití

### Použití vzdáleného repozitáře

**Výchozí nastavení** (podle repos.conf):
```bash
# Automaticky používá repozitář s nejvyšší prioritou
./balikar install gzip

# Dry-run (pouze zobrazí co by se stáhlo)
./balikar install gcc --dry-run
```

**Override repozitáře:**
```bash
# Použít konkrétní repozitář
./balikar --repo ./repo install gzip

# Použít lokální vývojový repozitář
./balikar --repo /path/to/my/repo install bash
```

### Ukázka logů

```
[INFO] Using source[0].url: https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz
[INFO] Using source[0].sha256: 977c8c0c5ae6309191e7768e28ebc951
[====>                ] 45% (5.2MB/11.5MB)
[INFO] Downloaded/cached https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz to /var/cache/balikar/downloads/bash-5.3.tar.gz
```

## Vytvoření vlastního vzdáleného repozitáře

### 1. Struktura adresářů

```
repo/
├── repodata.json
├── b/
│   └── bash/
│       └── bash-5.3/
│           ├── index.json
│           ├── .build
│           └── .install/
│               └── install.sh
├── g/
│   └── gzip/
│       └── gzip-1.14/
│           ├── index.json
│           ├── .build
│           └── .install/
│               └── install.sh
```

### 2. Příklad index.json

```json
{
  "name": "bash",
  "version": "5.3",
  "arch": "x86_64",
  "depends": ["glibc", "readline"],
  "license": "GPLv3+",
  "download_url": "https://github.com/USER/balikar-repo/raw/main/repo/b/bash/bash-5.3.tar.bz2",
  "source": [{
    "url": "https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz",
    "sha256": "977c8c0c5ae6309191e7768e28ebc951"
  }],
  "extras": [],
  "mkdeps": [],
  "is_group": false,
  "no_package": false
}
```

### 3. Publikace na GitHub

```bash
# V adresáři repo/
git init
git add .
git commit -m "Initial commit: Balikar package repository"
git remote add origin https://github.com/USER/balikar-repository.git
git branch -M main
git push -u origin main
```

## Použití vzdáleného repozitáře

### Konfigurace repozitářů

Balikar podporuje správu více repozitářů s prioritami podobně jako apt nebo pacman.

**Konfigurační soubory:**
- Uživatelská konfigurace: `~/.config/balikar/repos.conf` (má přednost)
- Systémová konfigurace: `/etc/balikar/repos.conf` (fallback)

**Formát konfigurace:**
```ini
[repository-name]
url = <path-or-url>
priority = <number>  # Vyšší = preferované
enabled = true/false
```

### Správa repozitářů příkazem

```bash
# Zobrazit nakonfigurované repozitáře
./balikar repo list

# Přidat nový repozitář
./balikar repo add my-repo https://example.com/repo 75

# Povolit/zakázat repozitář
./balikar repo enable my-repo
./balikar repo disable local-dev

# Odstranit repozitář
./balikar repo remove my-repo
```

## Bezpečnost

### SHA256 verifikace

Každý stažený soubor je automaticky ověřen pomocí SHA256 hashe:

- **Shoda**: Soubor je uložen do cache a použit pro build
- **Neshoda**: Soubor je smazán a instalace selže s chybou

```
[ERROR] Local file SHA mismatch: /var/cache/balikar/downloads/bash-5.3.tar.gz
```

### Doporučení

1. **Vždy uvádějte SHA256**: Bez něj nelze ověřit integritu stažených souborů
2. **Používejte HTTPS**: Pro prevenci man-in-the-middle útoků
3. **Ověřte zdroje**: Stahujte pouze z důvěryhodných serverů (GNU FTP, GitHub releases)

## Implementační detaily

### Moduly

- **`parser.c`**: Načítá `source[]` array z JSON a ukládá do libsolv repodata
- **`downloader.c`**: Preferuje `source[0].url` před `download_url`, ověřuje SHA256
- **`builder.c`**: Zpracovává stažené soubory bez změn (používá cache)

### API funkce

```c
// Parser
json_t* get_custom_source_array(Repodata *data, Id p, Pool *pool);

// Downloader (interně používá get_custom_source_array)
FILE *download_package(Solvable *s, const char *loc, Pool *pool, Repodata *data);
```

## Budoucí vylepšení

- [ ] Podpora více zdrojů v `source[]` array (fallback URLs)
- [ ] GPG signature verification
- [ ] Podpora mirrors (automatické přepínání při selhání)
- [ ] Delta updates (stahování pouze změn)
- [ ] Proxy/cache server podpora

## Příklady z LFS 12.4

Balíčky v repozitáři jsou založeny na Linux From Scratch 12.4:

```bash
# Stáhne a sestaví GCC 15.2.0
./balikar install gcc

# Zobrazí metadata včetně source URL
./balikar show gcc
```

Více informací: https://www.linuxfromscratch.org/lfs/view/stable/
