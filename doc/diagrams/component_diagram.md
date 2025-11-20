# Balikar Component Diagram

## Components Overview

### Main Executable
- **main.c**: Entry point and CLI orchestration
  - Commands: build, install, resolve, repos, download, clean-cache, repo

### Core Modules
- **parser.c**: Repository loading and metadata parsing
- **solver.c**: Dependency resolution using libsolv
- **downloader.c**: Package download with multi-source fallback
- **builder.c**: Package building and staging
- **installer.c**: Package installation and tracking (NEW)
- **config.c**: Repository configuration management
- **common.c**: Shared logging and utilities

### Utility Modules
- **cli_helpers.c**: User interaction and CLI utilities
- **system_helpers.c**: System detection (architecture, etc.)
- **print.c**: Transaction and usage formatting
- **debug.c**: Debugging and proof display

### External Dependencies
- **libsolv**: Pool, Solver, Transaction management
- **jansson**: JSON parsing
- **libcurl**: HTTP/HTTPS downloads
- **tar/libarchive**: Archive extraction

## Data Flow

```
User → main.c
  ↓
  → config.c → load repos.conf
  ↓
  → parser.c → load repository → /repo/repodata.json
  ↓
  → solver.c → resolve dependencies → Transaction
  ↓
  → downloader.c → download packages → /var/cache/balikar/
  ↓
  → builder.c → extract + build → $P/ (staging)
  ↓
  → installer.c → merge to chroot → /var/lib/balikar/installed.json
```

## Provided Interfaces (UML 2.5)

### IRepositoryLoader (parser.c)
```c
+ load_repository(pool: Pool*, repo_path: const char*): Repo*
+ load_json(filepath: const char*): json_t*
+ get_custom_source(data: Repodata*, p: Id, pool: Pool*): const char*
+ get_custom_source_array(data: Repodata*, p: Id, pool: Pool*): json_t*
+ get_custom_mkdeps(data: Repodata*, p: Id, pool: Pool*): json_t*
+ get_custom_extras(data: Repodata*, p: Id, pool: Pool*): json_t*
+ get_custom_is_group(data: Repodata*, p: Id, pool: Pool*): int
+ get_custom_no_package(data: Repodata*, p: Id, pool: Pool*): int
+ parse_depends_to_solvable(s: Solvable*, depends: json_t*, pool: Pool*): void
+ generate_repodata(repo_path: const char*): int
+ update_repository_metadata(repo_path: const char*): int
+ add_package_tar_bz2(tar_path: const char*, pool: Pool*, repo: Repo*): Id
+ internalize_repo(repo: Repo*): void
```

### IDependencyResolver (solver.c)
```c
+ create_solver(pool: Pool*): Solver*
+ solve_dependencies(solv: Solver*, pkg_name: const char*, pool: Pool*, repo: Repo*): Queue*
+ add_mkdeps_to_queue(job: Queue*, p: Id, pool: Pool*, repo: Repo*, depth: int): void
+ display_problem_proof(solv: Solver*, problem: int): void
```

### IPackageDownloader (downloader.c)
```c
+ download_package(s: Solvable*, loc: const char*, pool: Pool*, data: Repodata*, repo_path: const char*): FILE*
+ download_extras(extras: json_t*, pkg_name: const char*, version: const char*, repo_base_url: const char*, tmp_dir: const char*): void
+ download_package_to_directory(s: Solvable*, loc: const char*, pool: Pool*, data: Repodata*, repo_path: const char*, output_dir: const char*): int
+ get_cache_dir(): char*
```

### IPackageBuilder (builder.c)
```c
+ commit_element(trans: Transaction*, data: Repodata*, p: Id, fp: FILE*, pool: Pool*): int
+ find_source_path(tmp_dir: const char*): char*
+ apply_extras(extras: json_t*, pkg_name: const char*, source_dir: const char*): void
+ set_staging_enabled(enabled: int): void
+ set_staging_dir(dir: const char*): void
+ set_build_tmp_dir(dir: const char*): void
+ run_with_chroot(root: const char*, func: build_callback, data: void*): int
```

### IPackageInstaller (installer.c) - NEW
```c
+ install_package(pkg_name: const char*, pkg_version: const char*, staging_dir: const char*, chroot_path: const char*): int
+ is_package_installed(pkg_name: const char*): int
+ get_installed_packages(): json_t*
+ erase_package(pkg_name: const char*, chroot_path: const char*): int
+ validate_chroot(chroot_path: const char*): int
```

### IConfigManager (config.c)
```c
+ init_repo_config(): void
+ get_primary_repo_url(): const char*
+ get_enabled_repos(): repo_config_t*
+ add_repository(name: const char*, url: const char*, priority: int): int
+ remove_repository(name: const char*): int
+ enable_repository(name: const char*, enabled: int): int
+ save_repo_config(): int
+ list_repositories(): void
+ free_repo_config(): void
```

### ILogger (common.c)
```c
+ log_error(fmt: const char*, ...): void
+ log_warn(fmt: const char*, ...): void
+ log_info(fmt: const char*, ...): void
+ log_debug(level: int, fmt: const char*, ...): void
+ get_pool_arch(pool: const Pool*): const char*
```

### ICLIHelper (cli_helpers.c)
```c
+ print_usage(exit_code: int): void
+ user_confirm(prompt: const char*, default_yes: int): int
```

### IPrinter (print.c)
```c
+ display_transaction(trans: Transaction*): void
+ print_disk_usage_changes(trans: Transaction*): void
```

### IDebugger (debug.c)
```c
+ display_proof(solv: Solver*, id: Id, flags: int, lq: Queue*): void
+ display_problem_proof(solv: Solver*, problem: int): void
```

### ISystemHelper (system_helpers.c)
```c
+ set_system_arch(pool: Pool*): void
```

## Key Workflows

### Build Command
1. Parse repository metadata (parser)
2. Resolve dependencies (solver)
3. Download source packages (downloader)
4. Extract and build each package (builder)
5. Stage to $P or custom staging directory

### Install Command
1. Parse repository metadata (parser)
2. Resolve dependencies (solver)
3. Download source packages (downloader)
4. **Build directly into chroot** (builder with staging_dir=chroot)
5. Record installation in database (installer)

**Key Fix:** When using `install` command, `$P` is set to chroot path before build,
so packages are built directly into the target system.

### Download Command
1. Parse repository metadata (parser)
2. Resolve dependencies (solver)
3. Download packages to local repository structure (downloader)
4. No build or installation

### Resolve Command
1. Parse repository metadata (parser)
2. Resolve dependencies (solver)
3. Display transaction without execution

### Repository Management
- `repo list`: List all configured repositories
- `repo add/remove`: Modify repository configuration
- `repo enable/disable`: Toggle repository status

## Data Storage

### Repositories
- **Location:** `./repo/` or custom via CLI/config
- **Structure:** `<letter>/<package>/<package>-<version>/`
- **Metadata:** `repodata.json` (aggregated), `index.json` (per-package)

### Download Cache
- **Location:** `/var/cache/balikar/downloads/`
- **Contents:** Downloaded tar.xz archives, SHA256 checksums, extras (patches)

### Build Temporary
- **Location:** `/tmp/balikar_build_<pkg>_XXXXXX/`
- **Contents:** Extracted sources, build artifacts
- **Lifecycle:** Created before build, deleted after

### Staging Directory
- **Location:** `$P` or custom via `--staging-dir`
- **Contents:** Built packages ready for installation (DESTDIR)
- **Purpose:** Isolated build output before merge to chroot

### Installation Database
- **Location:** `/var/lib/balikar/installed.json`
- **Contents:** Installed packages, versions, file manifests, timestamps
- **Purpose:** Track installed packages for listing and removal

## Build Isolation

### Staging Mode (Default for build command)
- `DESTDIR=$P` set during build
- `$P` environment variable points to staging directory
- Files installed to staging directory
- Prevents pollution of build system

### Direct Chroot Mode (install command)
- `staging_dir` is set to `chroot_path` before build
- `$P` points directly to chroot
- Files installed directly into target system
- No separate merge step needed

### Chroot Isolation (Optional)
- `run_with_chroot()` for isolated builds
- Fork + chroot before build execution
- Enhanced security for untrusted packages

## Configuration Files

### Repository Configuration
- **User:** `~/.config/balikar/repos.conf`
- **System:** `/etc/balikar/repos.conf`
- **Fallback:** `./repos.conf`

Format:
```ini
[repo_name]
url = /path/or/url
priority = 100
enabled = 1
```

### Installed Packages Database
- **Location:** `/var/lib/balikar/installed.json`
- **Format:** JSON with package metadata and file manifests

Example:
```json
{
  "packages": {
    "glibc-2.42": {
      "name": "glibc",
      "version": "2.42",
      "install_date": "2025-11-20T10:30:00Z",
      "files": [
        "/usr/lib/libc.so.6",
        "/usr/bin/ldd",
        ...
      ]
    }
  }
}
```

## Component Dependencies

### Module Dependency Chain
```
main.c
  ├─→ config.c (repo configuration)
  ├─→ parser.c (metadata loading)
  │    └─→ config.c
  ├─→ solver.c (dependency resolution)
  │    └─→ parser.c
  ├─→ downloader.c (package download)
  │    └─→ parser.c
  ├─→ builder.c (package build)
  │    ├─→ parser.c
  │    ├─→ downloader.c
  │    └─→ solver.c
  └─→ installer.c (package installation)
       ├─→ builder.c
       └─→ parser.c
```

### All modules use
- **common.c**: Logging functions
- **cli_helpers.c**: User interaction
- **system_helpers.c**: System detection
- **print.c**: Formatted output
- **debug.c**: Debugging utilities

## External Library Usage

- **libsolv**: Used by parser.c, solver.c for pool/repo/solver operations
- **jansson**: Used by parser.c, installer.c for JSON parsing
- **libcurl**: Used by downloader.c for HTTP downloads
- **libarchive/tar**: Used by builder.c for archive extraction

## See Also
- [component_diagram.puml](component_diagram.puml) - Full PlantUML diagram with UML 2.5 interfaces
- [sequence_diagram.puml](sequence_diagram.puml) - Sequence diagrams for workflows
- [../balikar/](../balikar/) - Module documentation
- [../repo/](../repo/) - Repository format documentation
