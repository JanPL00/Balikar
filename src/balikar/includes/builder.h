//
// Created by placeja5 on 11/14/25.
//

#ifndef IMPL_BUILDER_H
#define IMPL_BUILDER_H

#include "../../common/common.h"
#include "parser.h"      // get_custom_*
#include "downloader.h"  // download_extras
#include "solver.h"      // Transaction usage
#include "../../utils/includes/cli_helpers.h"

// Veřejné API builderu

// Nastavení staging režimu (DESTDIR). 1 = povoleno, 0 = vypnuto.
void set_staging_enabled(int enabled);

// Nastavení vlastní cesty pro staging (DESTDIR). Pokud NULL, použije se STAGING_DIR z common.h.
void set_staging_dir(const char *dir);

// Nastavení vlastního tmp adresáře pro build (extrakce, patches). Pokud NULL, použije /tmp.
void set_build_tmp_dir(const char *dir);

// Izolace buildu v chroot (pro bezpečnost). Pokud root != NULL, fork + chroot před buildem.
// Callback build_func dostává build_data jako argument.
typedef int (*build_callback)(void *data);
int run_with_chroot(const char *root, build_callback func, void *data);

// Hlavní funkce pro build jednoho prvku transakce.
// Vrací 0 při úspěchu, nenulově při chybě.
int commit_element(Transaction *trans, Repodata *data, Id p, FILE *fp, Pool *pool);

// Vyhledá první podadresář (source dir) po extrakci tar archivu.
// Volající musí uvolnit vrácenou mallocovanou cestu.
char *find_source_path(const char *tmp_dir);

// Aplikuje patch soubory z extras cache (cache/extras/<pkg_name>). Ignoruje chyby.
void apply_extras(json_t *extras, const char *pkg_name, const char *source_dir);

#endif //IMPL_BUILDER_H