# Repository Management & Remote Repositories

Komplexní průvodce správou repozitářů v Balikaru včetně lokálních i vzdálených (remote) repozitářů.

## Přehled

Balikar podporuje:
- **Více repozitářů** s prioritním systémem
- **Vzdálené repozitáře** (GitHub, HTTP/HTTPS servery)
- **Lokální repozitáře** pro development
- **Automatické stahování** balíčků z internetu

Podobně jako apt, pacman nebo conda, ale pro build-from-source balíčky z Linux From Scratch.

## Konfigurace Repozitářů

### Hierarchie Konfiguračních Souborů

Balikar načítá konfiguraci v následujícím pořadí:

1. **Uživatelská konfigurace** `~/.config/balikar/repos.conf` (nejvyšší priorita)
2. **Systémová konfigurace** `/etc/balikar/repos.conf` (fallback)
3. **Výchozí** `./repo` s prioritou 10 (hardcoded fallback)

Pokud existuje uživatelská konfigurace, systémová se ignoruje.

### Formát Konfiguračního Souboru

```ini
[repository-name]
url = <URL nebo cesta>
priority = <číslo>
enabled = <true|false>
```

**Parametry:**
- `url`: URL remote repozitáře (https://...) nebo cesta k lokálnímu repozitáři (./repo, /opt/packages)
- `priority`: Číslo priority (vyšší = vyšší priorita). Aktivní je repozitář s nejvyšší prioritou
- `enabled`: `true` nebo `false` - zda je repozitář aktivní

### Příklad Konfigurace

Viz [repos.conf.example](repos.conf.example)

---

## Remote Repositories

### Struktura Metadat Balíčku

Balikar používá `source[]` array pro definici zdrojových souborů balíčků s podporou více mirror URL:

```json
{
  "name": "gzip",
  "version": "1.14",
  "download_url": "https://raw.githubusercontent.com/JanPL00/balikar-repository/main/g/gzip/gzip-1.14.tar.bz2",
  "source": [
    {
      "url": "https://ftp.gnu.org/gnu/gzip/gzip-1.14.tar.xz"
    },
    {
      "url": "https://mirror.example.com/gnu/gzip/gzip-1.14.tar.xz"
    },
    {
      "url": "https://backup.mirror.org/gzip-1.14.tar.xz"
    }
  ]
}
```

**Pole metadat:**
- **`download_url`**: URL předkompilovaného balíčku (budoucí použití pro binární balíčky)
- **`source`**: Array objektů se zdrojovými soubory (podporuje více mirror URL)
  - **`url`**: Přímý odkaz na zdrojový tarball (tar.gz, tar.xz, tar.bz2)
  - Downloader zkouší URL postupně v pořadí až do úspěchu

**Zpětná kompatibilita:** Starý formát s `source` jako string je stále podporován.

### Workflow Stahování

```
Parser → Načte source[] → Downloader → Zkouší source[0].url
                                    ↓
                            Stahování selhalo?
                                    ↓
                        ANO → Zkusí source[1].url
                                    ↓
                            Stahování selhalo?
                                    ↓
                        ANO → Zkusí source[2].url
                                    ↓
                            ...další URL...
                                    ↓
                        Cache do /var/cache/balikar/downloads/
```

**Multi-source fallback:**
- Downloader automaticky zkouší všechny URL v pořadí
- Při selhání jedné URL pokračuje na další
- Zastaví se po prvním úspěšném stažení
- Pokud selžou všechny URL, zkusí lokální fallback z repository struktury

### Bezpečnost

**Vysoká dostupnost:**
- Více mirror URL zajišťuje dostupnost i při výpadku serveru
- Automatický fallback bez zásahu uživatele

---

## CLI Příkazy

### Zobrazení Repozitářů

```bash
# Zobrazit všechny nakonfigurované repozitáře a aktuálně aktivní
balikar repos

# Alternativní příkaz (zobrazí pouze seznam nakonfigurovaných repozitářů)
balikar repo list
```

**Výstup `balikar repos`:**
```
Configured repositories:
NAME                 PRIORITY   ENABLED    URL
------------------------------------------------------------------------
balikar-official     100        yes        https://raw.githubusercontent.com/JanPL00/balikar-repository/main
local-dev            50         yes        ./repo

Currently using: https://raw.githubusercontent.com/JanPL00/balikar-repository/main
Packages available: 11
```

### Aktualizace Metadata

```bash
# Aktualizovat metadata primárního repozitáře
balikar repo-update

# Aktualizovat metadata konkrétního repozitáře
balikar repo-update --repo=./repo
balikar repo-update --repo=https://example.com/packages
```

**Chování:**
- **Remote repozitáře** (http/https): Stáhne `repodata.json` z URL do cache (`/var/cache/balikar/downloads/`)
- **Lokální repozitáře**: Vygeneruje `repodata.json` ze všech `index.json` souborů v adresářové struktuře

### Správa Repozitářů (CLI)

> **Poznámka:** Subpříkazy `add`, `remove`, `enable`, `disable` jsou definovány v CLI, ale jejich implementace v `cmd_repo.c` je neúplná, aktuálně funguje pouze `balikar repo list`. Pro správu repozitářů upravte konfigurační soubor ručně.

```bash
# Přidat nový repozitář s prioritou (neimplementováno)
balikar repo add <name> <url> [priority]

# Odebrat repozitář (neimplementováno)
balikar repo remove <name>

# Dočasně vypnout repozitář (neimplementováno)
balikar repo disable <name>

# Znovu zapnout repozitář (neimplementováno)
balikar repo enable <name>
```

Změny konfiguraci provádějte ručně v `~/.config/balikar/repos.conf` nebo `/etc/balikar/repos.conf`.

## Workflow Příklady

### Použití Remote GitHub Repozitáře

```bash
# 1. Zkontrolovat aktuální konfiguraci
balikar repos

# 2. Aktualizovat metadata z GitHubu
balikar repo-update

# 3. Instalovat balíček z remote repozitáře
balikar build hello
```

### Vývoj s Lokálním Repozitářem

```bash
# 1. Přidat lokální repozitář do repos.conf s vyšší prioritou
# Editujte ~/.config/balikar/repos.conf

# 2. Po změnách v index.json regenerovat metadata
balikar repo-update --repo=./repo

# 3. Testovat balíček
balikar build mypackage --repo=./repo
```

### Správa Více Repozitářů

```bash
# Seznam všech repozitářů
balikar repos

# Přepnutí na jiný repozitář pomocí --repo override
balikar build mypackage --repo=https://example.com/packages

# Dočasné použití lokálního repo
balikar install gzip --repo=./local-dev
```

### Publikování Vlastního Repozitáře

#### 1. Struktura Adresářů

```
myrepo/
├── repodata.json
├── b/
│   └── bash/
│       └── bash-5.3/
│           ├── index.json
│           ├── bash-5.3.tar.bz2  (volitelný, pro distribuci)
│           ├── .build
│           └── .install/
│               └── install.sh
├── g/
│   └── gzip/
│       └── gzip-1.14/
│           ├── index.json
│           ├── .build
│           └── .install/
```

#### 2. Příklad index.json

```json
{
  "name": "bash",
  "version": "5.3",
  "arch": "x86_64",
  "depends": ["glibc", "readline"],
  "license": "GPLv3+",
  "download_url": "https://raw.githubusercontent.com/YOUR-USERNAME/YOUR-REPO/main/b/bash/bash-5.3.tar.bz2",
  "source": [{
    "url": "https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz"
  }],
  "extras": [],
  "mkdeps": [],
  "is_group": false,
  "no_package": false
}
```

#### 3. Vytvoření a Publikace

```bash
# 1. Vytvořit strukturu balíčků
mkdir -p myrepo/{b,c,g,h}/

# 2. Přidat balíčky do struktury <písmeno>/<název>/<název-verze>/
# Každý balíček obsahuje index.json a build skripty

# 3. Vygenerovat repodata.json
balikar repo-update --repo=./myrepo

# 4. Publikovat na GitHub/GitLab
cd myrepo
git init
git add .
git commit -m "Initial repository"
git remote add origin https://github.com/user/myrepo.git
git push -u origin main

# 5. Uživatelé mohou přidat váš repozitář do repos.conf
# [myrepo]
# url = https://raw.githubusercontent.com/user/myrepo/main
# priority = 90
# enabled = true
```

#### 4. Použití Remote Repozitáře

```bash
# Výchozí (podle repos.conf s nejvyšší prioritou)
balikar install gzip

# Override repozitáře
balikar install bash --repo=https://example.com/packages
balikar install mypackage --repo=./local-dev

# Dry-run (zobrazí co by se stáhlo)
balikar install gcc --dry-run
```

**Ukázka logů:**
```
[INFO] Trying source[0].url: https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz
[====>                ] 45% (5.2MB/11.5MB)
[INFO] Successfully downloaded from source[0]: https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz
[INFO] Package cached at: /var/cache/balikar/downloads/bash-5.3.tar.gz
```

**Při fallback na další URL:**
```
[INFO] Trying source[0].url: https://down-mirror.example.com/bash-5.3.tar.gz
[WARN] Download failed from source[0]: https://down-mirror.example.com/bash-5.3.tar.gz (Connection failed)
[INFO] Trying source[1].url: https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz
[====>                ] 45% (5.2MB/11.5MB)
[INFO] Successfully downloaded from source[1]: https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz
```

## Cache Management

Remote repozitáře cachují `repodata.json` v `/var/cache/balikar/downloads/`.

Pro vyčištění cache:
```bash
balikar clean-cache
```

## Prioritní Systém

Balikar vybírá repozitář s **nejvyšší prioritou** mezi aktivními (`enabled = true`).

Doporučené priority:
- **150+**: Lokální development repozitáře
- **100**: Production remote repozitáře
- **50-90**: Testing/staging repozitáře
- **10-40**: Fallback repozitáře
- **10**: Výchozí lokální ./repo

## Troubleshooting

### Repozitář se neaktualizuje

```bash
# Zkontrolovat jaký repozitář je aktivní
balikar repos

# Vynutit aktualizaci konkrétního repozitáře
balikar repo-update --repo=<url>

# Vyčistit cache a znovu stáhnout
balikar clean-cache
balikar repo-update
```

### Remote repozitář není dostupný

```bash
# Zkontrolovat aktivní repozitáře
balikar repos

# Dočasně přepnout na lokální repozitář pomocí --repo
balikar build mypackage --repo=./local-dev
```

---

## Implementační Detaily

### Moduly

- **`parser.c`**: Načítá `source[]` array z JSON a ukládá do libsolv repodata
- **`downloader.c`**: Iteruje přes všechny URL v `source[]` poli s automatickým fallbackem při selhání
- **`builder.c`**: Zpracovává stažené soubory bez změn (používá cache)
- **`config.c`**: Správa konfigurace repozitářů (načítání, ukládání, prioritizace)

### API Funkce

```c
// Repository Configuration (config.c)
void init_repo_config(void);
const char* get_primary_repo_url(void);
repo_config_t* get_enabled_repos(void);
int add_repository(const char *name, const char *url, int priority);
int remove_repository(const char *name);
int enable_repository(const char *name, int enabled);
int save_repo_config(void);
void list_repositories(void);
void free_repo_config(void);

// Parser (parser.c)
int update_repository_metadata(const char *repo_path);
Repo* load_repository(Pool *pool, const char *repo_path);
```

### Datové Struktury

```c
typedef struct repo_config {
    char *name;              // Název repozitáře
    char *url;               // URL nebo cesta
    int priority;            // Priorita (vyšší = preferovaný)
    int enabled;             // 1 = aktivní, 0 = zakázaný
    struct repo_config *next; // Linked list
} repo_config_t;
```

---

## Budoucí Vylepšení

- [ ] Plná implementace CLI subpříkazů `repo add/remove/enable/disable`
- [ ] GPG signature verification
- [ ] Repository snapshots pro reproducibility

---

## Související Dokumentace

- **[repos.conf.example](repos.conf.example)** - Anotovaný příklad konfigurace
- **[Parser Documentation](../balikar/parser.txt)** - Implementace parsování
- **[Downloader Documentation](../balikar/downloader.txt)** - Stahování a cache
- **[Main README](../../README.md)** - Přehled projektu

---

## Příklady z LFS 12.4

Oficiální Balikar repozitář obsahuje balíčky z Linux From Scratch 12.4:

```bash
# Zobrazit dostupné repozitáře
balikar repos

# Stáhnout a sestavit GCC 15.2.0
balikar build gcc

# Aktualizovat metadata repozitáře
balikar repo-update
```

**Oficiální repozitář:** https://github.com/JanPL00/balikar-repository  
**LFS dokumentace:** https://www.linuxfromscratch.org/lfs/view/stable/
