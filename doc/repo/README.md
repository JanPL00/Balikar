# Repository Management & Remote Repositories

Komplexní průvodce správou repozitářů v Balikaru včetně lokálních i vzdálených (remote) repozitářů.

## Přehled

Balikar podporuje:
- **Více repozitářů** s prioritním systémem
- **Vzdálené repozitáře** (GitHub, HTTP/HTTPS servery)
- **Lokální repozitáře** pro development
- **Automatické stahování** balíčků z internetu
- **SHA256 verifikaci** pro bezpečnost

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

Balikar používá `source[]` array pro definici zdrojových souborů balíčků:

```json
{
  "name": "gzip",
  "version": "1.14",
  "download_url": "https://raw.githubusercontent.com/USER/balikar-repository/main/g/gzip/gzip-1.14.tar.bz2",
  "source": [{
    "url": "https://ftp.gnu.org/gnu/gzip/gzip-1.14.tar.xz",
    "sha256": "4bf5a10f287501ee8e8ebe00ef62b2c2"
  }]
}
```

**Pole metadat:**
- **`download_url`**: URL předkompilovaného balíčku (budoucí použití pro binární balíčky)
- **`source`**: Array objektů se zdrojovými soubory
  - **`url`**: Přímý odkaz na zdrojový tarball (tar.gz, tar.xz, tar.bz2)
  - **`sha256`**: SHA-256 hash pro verifikaci integrity

**Zpětná kompatibilita:** Starý formát s `source` jako string je stále podporován.

### Workflow Stahování

```
Parser → Načte source[] → Downloader → Preferuje source[0].url
                                    ↓
                              Stáhne soubor
                                    ↓
                          Ověří SHA256 checksum
                                    ↓
                        Cache do /var/cache/balikar/
```

### Bezpečnost

**SHA256 verifikace:**
- Každý stažený soubor je automaticky ověřen
- **Shoda**: Soubor uložen do cache a použit
- **Neshoda**: Soubor smazán, instalace selže

**Doporučení:**
1. ✅ Vždy uvádějte SHA256 hash
2. ✅ Používejte HTTPS pro remote URL
3. ✅ Stahujte pouze z důvěryhodných zdrojů (GNU FTP, GitHub releases)

---

## CLI Příkazy

### Zobrazení Repozitářů

```bash
# Zobrazit všechny nakonfigurované repozitáře
balikar repos

# Alternativní příkaz
balikar repo list
```

**Výstup:**
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

### Přidání Repozitáře

```bash
# Přidat nový repozitář s prioritou
balikar repo add <name> <url> [priority]

# Příklad: Přidat testing repozitář s prioritou 80
balikar repo add testing https://testing.example.com/packages 80
```

Změny se ukládají do uživatelské konfigurace (`~/.config/balikar/repos.conf`).

### Odebrání Repozitáře

```bash
# Odebrat repozitář podle jména
balikar repo remove <name>

# Příklad
balikar repo remove testing
```

### Aktivace/Deaktivace

```bash
# Dočasně vypnout repozitář (zůstane v konfiguraci)
balikar repo disable <name>

# Znovu zapnout repozitář
balikar repo enable <name>
```

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
# 1. Přepnout na lokální repozitář (vyšší priorita)
balikar repo add local-dev ./repo 150

# 2. Po změnách v index.json regenerovat metadata
balikar repo-update --repo=./repo

# 3. Testovat balíček
balikar build mypackage --repo=./repo
```

### Správa Více Repozitářů

```bash
# Přidat production repozitář
balikar repo add production https://prod.example.com/packages 100

# Přidat testing repozitář s nižší prioritou
balikar repo add testing https://testing.example.com/packages 50

# Seznam všech repozitářů
balikar repos

# Dočasně přepnout na testing
balikar repo disable production
balikar repo enable testing
balikar repos  # testing je nyní primární

# Vrátit zpět
balikar repo enable production
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
  "download_url": "https://raw.githubusercontent.com/USER/myrepo/main/b/bash/bash-5.3.tar.bz2",
  "source": [{
    "url": "https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz",
    "sha256": "977c8c0c5ae6309191e7768e28ebc951..."
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

# 5. Uživatelé mohou přidat váš repozitář
balikar repo add myrepo https://raw.githubusercontent.com/user/myrepo/main 90
```

#### 4. Použití Remote Repozitáře

```bash
# Výchozí (podle repos.conf s nejvyšší prioritou)
balikar install gzip

# Override repozitáře
balikar --repo=https://example.com/packages install bash
balikar --repo=./local-dev install mypackage

# Dry-run (zobrazí co by se stáhlo)
balikar install gcc --dry-run
```

**Ukázka logů:**
```
[INFO] Using source[0].url: https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz
[INFO] Using source[0].sha256: 977c8c0c5ae6309191e7768e28ebc951
[====>                ] 45% (5.2MB/11.5MB)
[INFO] Downloaded to /var/cache/balikar/downloads/bash-5.3.tar.gz
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

### Konflikt priorit

```bash
# Zobrazit všechny repozitáře s prioritami
balikar repos

# Upravit priority ručně v ~/.config/balikar/repos.conf
# nebo použít CLI příkazy
balikar repo remove lower-priority-repo
balikar repo add higher-priority-repo <url> 150
```

### Remote repozitář není dostupný

```bash
# Dočasně vypnout remote repozitář
balikar repo disable balikar-official

# Přepnout na lokální fallback
balikar repo enable local-dev

# Ověřit
balikar repos
```

---

## Implementační Detaily

### Moduly

- **`parser.c`**: Načítá `source[]` array z JSON a ukládá do libsolv repodata
- **`downloader.c`**: Preferuje `source[0].url` před `download_url`, ověřuje SHA256
- **`builder.c`**: Zpracovává stažené soubory bez změn (používá cache)
- **`config.c`**: Správa konfigurace repozitářů (načítání, ukládání, prioritizace)

### API Funkce

```c
// Repository Configuration (config.c)
int init_repo_config(void);
const char* get_primary_repo_url(void);
int add_repository(const char *name, const char *url, int priority);
int remove_repository(const char *name);
void list_repositories(void);

// Parser (parser.c)
json_t* get_custom_source_array(Repodata *data, Id p, Pool *pool);
int update_repository_metadata(const char *repo_path);
Repo* load_repository(Pool *pool, const char *repo_path);

// Downloader (downloader.c)
FILE* download_package(Solvable *s, const char *loc, Pool *pool, Repodata *data);
```

### Datové Struktury

```c
// Repository configuration
typedef struct repo_config {
    char *name;              // Repository name
    char *url;               // URL or path
    int priority;            // Priority (higher = preferred)
    int enabled;             // 1 = enabled, 0 = disabled
    struct repo_config *next; // Linked list
} repo_config_t;
```

---

## Budoucí Vylepšení

- [ ] Podpora více zdrojů v `source[]` array (fallback URLs)
- [ ] GPG signature verification
- [ ] Podpora mirrors (automatické přepínání při selhání)
- [ ] Delta updates (stahování pouze změn)
- [ ] Proxy/cache server podpora
- [ ] Repository snapshots pro reproducibility

---

## Související Dokumentace

- **[repos.conf.example](repos.conf.example)** - Anotovaný příklad konfigurace
- **[Architecture & Workflow](../balikar/README.md)** - Detailní workflow diagramy
- **[Parser Documentation](../balikar/parser.txt)** - Implementace parsování
- **[Downloader Documentation](../balikar/downloader.txt)** - Stahování a cache
- **[Main README](../../README.md)** - Přehled projektu

---

## Příklady z LFS 12.4

Oficiální Balikar repozitář obsahuje balíčky z Linux From Scratch 12.4:

```bash
# Zobrazit dostupné balíčky
balikar repos

# Stáhnout a sestavit GCC 15.2.0
balikar build gcc

# Zobrazit metadata včetně source URL
balikar show gcc
```

**Oficiální repozitář:** https://github.com/JanPL00/balikar-repository  
**LFS dokumentace:** https://www.linuxfromscratch.org/lfs/view/stable/
