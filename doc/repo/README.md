# Repository Management

Balikar podporuje více repozitářů s prioritami, což umožňuje flexibilní správu zdrojů balíčků.

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

```bash
# 1. Vytvořit strukturu balíčků
mkdir -p myrepo/{b,c,g,h}/

# 2. Přidat balíčky do struktury <písmeno>/<název>/<název-verze>/
# Každý balíček obsahuje:
#   - index.json (metadata)
#   - <název-verze>.tar.bz2 (archiv)

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

## Související Dokumentace

- [REMOTE_REPOSITORY.md](../REMOTE_REPOSITORY.md) - Detaily o remote repozitářích a `source[]` array
- [repos.conf.example](repos.conf.example) - Příklad konfiguračního souboru s komentáři
- [balikar/parser.txt](../balikar/parser.txt) - Implementace parsování repodata
