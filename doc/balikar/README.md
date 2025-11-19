# Balikar Architecture & Workflow

Přehled architektury a workflow balíčkového manageru Balikar.

## Architektura Systému

```
┌─────────────────────────────────────────────────────────────────┐
│                         BALIKAR CLI                              │
│                      (src/balikar/main.c)                        │
└────────────┬────────────────────────────────────────────────────┘
             │
             ├─── Repository Configuration (src/common/config.c)
             │    • Loads repos.conf (user > system > default)
             │    • Manages repository priorities
             │    • Returns primary repository URL
             │
             ├─── Parser (src/balikar/parser.c)
             │    • Downloads/generates repodata.json
             │    • Parses package metadata
             │    • Creates libsolv Pool/Repo structures
             │    • Loads source[] arrays and custom fields
             │
             ├─── Solver (src/balikar/solver.c)
             │    • Resolves dependencies using libsolv
             │    • Handles makedeps recursion
             │    • Creates transaction queue
             │
             ├─── Downloader (src/balikar/downloader.c)
             │    • Downloads packages with multi-source fallback
             │    • Tries all URLs in source[] array until success
             │    • Manages cache (/var/cache/balikar/)
             │
             └─── Builder (src/balikar/builder.c)
                  • Extracts packages to /tmp
                  • Executes .build scripts
                  • Installs to system or DESTDIR
```

## Workflow Diagramy

### 1. Inicializace a Načtení Repozitáře

```
┌─────────────┐
│ balikar CLI │
└──────┬──────┘
       │
       ├─► init_repo_config()
       │   └─► Checks ~/.config/balikar/repos.conf
       │       └─► Falls back to /etc/balikar/repos.conf
       │           └─► Falls back to ./repo (priority 10)
       │
       ├─► get_primary_repo_url()
       │   └─► Returns highest priority enabled repository
       │
       ├─► pool_create()
       │   └─► Creates libsolv Pool
       │       └─► set_system_arch(pool)
       │
       └─► load_repository(pool, repo_url)
           ├─► is_remote_url()?
           │   ├─[YES]─► download_repodata(url, cache)
           │   │         └─► Downloads to /var/cache/balikar/downloads/
           │   │
           │   └─[NO]──► Check if repodata.json exists
           │             └─[NO]─► generate_repodata(local_path)
           │                       └─► Scans index.json files
           │
           ├─► repo_add_conda(repo, repodata.json)
           │   └─► Populates libsolv structures
           │
           └─► add_custom_fields_to_repodata()
               └─► Adds source[], extras, download_url, etc.
```

### 2. Build/Install Workflow

```
┌────────────────────────┐
│ balikar build <package>│
└───────────┬────────────┘
            │
            ├─► create_solver(pool, repo)
            │   └─► Configures libsolv Solver
            │
            ├─► solve_package_recursive(package, solver, pool, repo)
            │   ├─► Find package solvable in pool
            │   ├─► Get makedeps from custom fields
            │   ├─► Recursively solve makedeps
            │   ├─► Add package to job queue
            │   ├─► solver_solve(solver, queue)
            │   └─► Return transaction
            │
            ├─► Print transaction summary
            │   └─► User confirmation [y/n]
            │
            ├─[YES]─► execute_transaction(trans, pool, repo)
            │         │
            │         └─► For each package in transaction:
            │             │
            │             ├─► download_package(solvable)
            │             │   ├─► Check cache first
            │             │   ├─[MISS]─► Try each URL in source[] array:
            │             │   │          ├─► Try source[0].url
            │             │   │          ├─[FAIL]─► Try source[1].url
            │             │   │          ├─[FAIL]─► Try source[2].url...
            │             │   │          └─[SUCCESS]─► curl to /var/cache/balikar/
            │             │   └─► Return FILE* to package
            │             │
            │             ├─► build_package(package_file)
            │             │   ├─► Extract to /tmp/balikar_build_<name>_XXXXXX
            │             │   ├─► Load extras from metadata
            │             │   ├─► Download extras to tmp_dir
            │             │   ├─► Execute .build script
            │             │   │   └─► Sets DESTDIR=$P
            │             │   │       └─► Script installs to $P/
            │             │   └─► Cleanup /tmp directory
            │             │
            │             └─► [If install command] install_to_system()
            │
            └─[NO]──► Exit
```

### 3. Repository Update Workflow

```
┌──────────────────────┐
│ balikar repo-update  │
└──────────┬───────────┘
           │
           ├─► init_repo_config()
           │   └─► Get primary repository URL
           │
           ├─► update_repository_metadata(repo_url)
           │   │
           │   ├─► is_remote_url()?
           │   │   │
           │   │   ├─[YES]─► download_repodata(url, cache)
           │   │   │         ├─► Downloads fresh repodata.json
           │   │   │         └─► Caches to /var/cache/balikar/
           │   │   │
           │   │   └─[NO]──► generate_repodata(local_path)
           │   │             ├─► Scans all <letter>/<name>/<name-version>/
           │   │             ├─► Loads each index.json

           │   │             ├─► Merges all metadata
           │   │             └─► Writes repodata.json
           │   │
           │   └─► Log success
           │
           └─► load_repository(pool, repo_url)
               └─► Reload with fresh metadata
```

### 4. Repository Management

```
┌──────────────────────────┐
│ balikar repo add <name>  │
│     <url> <priority>     │
└────────────┬─────────────┘
             │
             ├─► init_repo_config()
             │   └─► Load current config
             │
             ├─► add_repository(name, url, priority)
             │   └─► Append to repo_config_t list
             │
             ├─► save_repo_config()
             │   └─► Write to ~/.config/balikar/repos.conf
             │
             └─► Log confirmation

┌─────────────────────────┐
│ balikar repos           │
└────────────┬────────────┘
             │
             ├─► init_repo_config()
             │   └─► Load current config
             │
             ├─► list_repositories()
             │   └─► Print formatted table:
             │       NAME | PRIORITY | ENABLED | URL
             │
             └─► Show current primary repository
```

## Datové Struktury

### Repository Configuration (config.h)

```c
typedef struct repo_config {
    char *name;              // Repository name
    char *url;               // URL or path
    int priority;            // Priority (higher = preferred)
    int enabled;             // 1 = enabled, 0 = disabled
    struct repo_config *next; // Linked list
} repo_config_t;
```

### Package Metadata (repodata.json)

```json
{
  "packages": {
    "hello-1.0-0.tar.bz2": {
      "name": "hello",
      "version": "1.0",
      "build": "0",
      "depends": [],
      "source": [
        {
          "url": "https://ftp.gnu.org/gnu/hello/hello-1.0.tar.gz"
        }
      ],
      "download_url": "https://raw.githubusercontent.com/.../hello-1.0.tar.bz2",
      "extras": [],
      "is_group": false,
      "no_package": false
    }
  }
}
```

### libsolv Integration

Balikar používá **libsolv** (knihovna z openSUSE) pro dependency resolution:

- **Pool**: Globální kontext obsahující všechny balíčky
- **Repo**: Repozitář balíčků v Pool
- **Solvable**: Jednotlivý balíček s metadaty
- **Solver**: Engine pro řešení závislostí
- **Transaction**: Seznam operací k provedení

## Moduly a Dokumentace

### Core Modules

- **[main.c](main.txt)** - CLI entry point, command dispatcher
- **[parser.c](parser.txt)** - Repository parsing, metadata loading
- **[solver.c](solver.txt)** - Dependency resolution, libsolv integration
- **[downloader.c](downloader.txt)** - Package downloading, verification
- **[builder.c](builder.txt)** - Package building, installation

### Common Modules

- **config.c** - Repository configuration management
- **common.c** - Shared utilities, logging

### Utility Modules

- **[cli_helpers.c](../utils/cli_helpers.txt)** - CLI help, user prompts
- **[string_helpers.c](../utils/string_helpers.txt)** - String manipulation
- **[system_helpers.c](../utils/system_helpers.txt)** - System detection
- **[print.c](../utils/print.txt)** - Formatted output
- **[debug.c](../utils/debug.txt)** - Debug logging

## Konfigurace a Cache

### Konfigurační Soubory

```
~/.config/balikar/repos.conf      # User configuration
/etc/balikar/repos.conf           # System configuration
```

### Cache Adresáře

```
/var/cache/balikar/downloads/     # Downloaded packages and repodata.json
/tmp/balikar_build_<name>_XXXXXX  # Temporary build directories
```

## Závislosti

- **libsolv / libsolvext** - Dependency resolution
- **jansson** - JSON parsing
- **libcurl** - HTTP downloads

- **libarchive** - tar.bz2 extraction

## Bezpečnost


- **HTTPS** - Doporučeno pro remote repozitáře
- **Source integrity** - `source[]` pole obsahuje checksums pro upstream zdroje

## Contributing

Pro přidání nové funkcionality:

1. Upravte příslušný modul ve `src/`
2. Aktualizujte dokumentaci v `doc/balikar/<module>.txt`
3. Přidejte testy do `tests/tests-src/`
4. Aktualizujte tento README s workflow diagramy

## Související Dokumentace

- **[Repository Management](../repo/README.md)** - Kompletní průvodce: lokální/remote repozitáře, konfigurace, CLI
- **[Configuration Example](../repo/repos.conf.example)** - Anotovaný příklad konfigurace
- **[Manual](../manual.txt)** - Obecná dokumentace
- **[Main README](../../README.md)** - Přehled projektu
