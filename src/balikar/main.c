//
// Created by placeja5 on 11/2/25.
//
#include "repo_parser.h"
#include "../utils/includes/system_helpers.h"
#include "../common/common.h"

int main(int argc, char **argv) {
    if (argc != 2) {
            log_error("Usage: %s /path/to/repo", argv[0]);
            return 1;
        }
        const char *repo_path = argv[1];

        // Inicializuj Pool
        Pool *pool = pool_create();
        if (!pool) {
            log_error("Failed to create pool.");
            return 1;
        }
        set_system_arch(pool);  // Nastav default arch (z system_helpers.c)

        // Vytvoř Repo
        Repo *repo = repo_create(pool, "test_repo");
        if (!repo) {
            log_error("Failed to create repo.");
            pool_free(pool);
            return 1;
        }

        // Spusť generate_repodata (načte index.json, vytvoří Solvables, custom metadata, vygeneruje repodata.json)
        if (generate_repodata(repo_path, pool, repo) != 0) {
            log_error("Failed to generate repodata for %s.", repo_path);
            repo_free(repo, 1);
            pool_free(pool);
            return 1;
        }

        // Internalizuj repo (pro indexaci whatprovides a lookup)
        repo_internalize(repo);

        // Test: Iteruj Solvables a ověř custom metadata
        log_info("Testing loaded solvables...");
        int solvable_count = 0;
        for (Id p = repo->start; p < repo->end; p++) {
            Solvable *s = pool->solvables + p;
            if (!s->repo || s->repo != repo) continue;  // Skip nepatřící

            solvable_count++;
            const char *name = pool_id2str(pool, s->name);
            const char *arch_str = pool_id2str(pool, s->arch);
            const char *evr = pool_id2str(pool, s->evr);

            log_info("Solvable #%d: Name=%s, EVR=%s, Arch=%s", solvable_count, name ? name : "unknown", evr ? evr : "unknown", arch_str ? arch_str : "unknown");

            // Ověř custom metadata z Repodata (pro všechny Repodata v repo)
            for (Id dataid = 1; dataid < repo->nrepodata; dataid++) {
                Repodata *data = repo_id2repodata(repo, dataid);
                if (!data) continue;

                // Lookup mkdeps (jako string z json_dumps)
                Id mkdeps_key = pool_str2id(pool, "makedeps", 0);  // 0 = no create if missing
                const char *mkdeps_str = repodata_lookup_str(data, p, mkdeps_key);
                if (mkdeps_str) {
                    log_info("  - mkdeps: %s", mkdeps_str);
                }

                // Lookup download_url (source)
                Id source_key = pool_str2id(pool, "download_url", 0);
                const char *source_url = repodata_lookup_str(data, p, source_key);
                if (source_url) {
                    log_info("  - source: %s", source_url);
                }

                // Lookup extras
                Id extras_key = pool_str2id(pool, "extras", 0);
                const char *extras_str = repodata_lookup_str(data, p, extras_key);
                if (extras_str) {
                    log_info("  - extras: %s", extras_str);
                }

                // Standardní: License a Summary
                const char *license = repodata_lookup_str(data, p, SOLVABLE_LICENSE);
                if (license) {
                    log_info("  - License: %s", license);
                }
                const char *summary = repodata_lookup_str(data, p, SOLVABLE_SUMMARY);
                if (summary) {
                    log_info("  - Summary: %s", summary);
                }
            }
        }

        if (solvable_count == 0) {
            log_warn("No solvables loaded – check if index.json exists in %s.", repo_path);
        } else {
            log_info("Test successful: %d solvables loaded.", solvable_count);
        }

        // Cleanup
        repo_free(repo, 1);
        pool_free(pool);

        return 0;
}