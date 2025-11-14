
// Created by placeja5 on 11/12/25.
// Kostra pro solver.c podle akčního plánu (subkrok 2.1).
// Adaptováno z solv.c a libsolv manuálů (pool_create, solver_create, queue_init, solver_solve, transaction_create atd.).
// Zaměřeno na: create_solver (s flags pro Conda/LFS), rekurze pro mkdeps (add_mkdeps_to_queue s depth limitem),
// solve_dependencies (hlavní funkce: mkdeps rekurze + runtime jobs, solver_solve, transaction handling).
// display_problem_proof adaptováno z solv.c (doshowproof + showproof).
// Použití: Integruj s main.c (načti pool/repo z parser.c, zavolej solve_dependencies, print transaction z utils/print.c).
//

#include "includes/solver.h"

// Privátní helper pro rekurzi (interní, ne v .h)
static void internal_display_proof(Solver *solv, Id id, int flags, Queue *lq);

// Vytvoření solveru s nastavením flags a arch (adaptace z solv.c a pool.3 man)
Solver *create_solver(Pool *pool) {
    if (!pool) {
        log_error("Cannot create solver: Invalid pool.");
        return NULL;
    }

    Solver *solv = solver_create(pool);  // Z libsolv-bindings.3: solver_create(pool)
    if (!solv) {
        log_error("Failed to create solver.");
        return NULL;
    }

    // Nastav flags pro náš workflow (z constantids.3 a solv.c)
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_UNINSTALL, 1);     // Povol uninstall pro konflikty
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_VENDORCHANGE, 1);  // Povol vendor změny (pro LFS flexibilitu)
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_DOWNGRADE, 0);     // Zakáž downgrade pro bezpečnost (uprav podle potřeby)
    solver_set_flag(solv, SOLVER_FLAG_NO_AUTOTARGET, 1);       // Žádný auto-target pro explicitní jobs

    // Para Conda kompatibilitu (z plánu, i když není v constantids – předpokládám existenci)
    // pool_set_flag(pool, POOL_FLAG_WHATPROVIDESUSESCONDA, 1);  // Pokud existuje, nastav na pool

    // Note: Pool arch should already be set by caller (main.c)
    // Do NOT call set_system_arch here as it reinitializes the pool

    // Note: pool_createwhatprovides should already be called by load_repository
    // Do NOT call it again here as it would invalidate the whatprovides mapping

    log_info("Solver created with flags set and whatprovides indexed.");
    return solv;
}

// Rekurzní přidání mkdeps do job queue (separace build-time deps, z plánu)
// Používá get_custom_mkdeps z parser.c (vrací json_t array), pool_str2id, FOR_PROVIDES (z pool.3)
void add_mkdeps_to_queue(Queue *job, Id p, Pool *pool, Repo *repo, int depth) {
    if (depth > MAX_RECURSION_DEPTH) {
        log_error("Recursion depth exceeded (%d) – possible circular dependency.", depth);
        return;  // Nebo exit(1) pro striktní chybu
    }

    // Načti Repodata (z bindings.3: repo_last_repodata nebo repo_add_repodata)
    Repodata *data = repo_last_repodata(repo);  // Předpokládáme, že repo má data (po internalize v parser.c)
    if (!data) {
        log_warn("No repodata for solvable %d – skipping mkdeps.", p);
        return;
    }

    // Získej mkdeps jako json_t* array (z parser.c)
    json_t *mkdeps = get_custom_mkdeps(data, p, pool);
    if (!mkdeps || !json_is_array(mkdeps)) {
        log_debug(2, "No mkdeps for solvable %d.", p);
        if (mkdeps) json_decref(mkdeps);
        return;
    }

    size_t mkdep_count = json_array_size(mkdeps);
    if (mkdep_count == 0) {
        log_debug(2, "Empty mkdeps array for solvable %d.", p);
        json_decref(mkdeps);
        return;
    }

    // Pro každý mkdep item (string pkg name)
    size_t i;
    json_t *item;
    json_array_foreach(mkdeps, i, item) {
        if (!json_is_string(item)) continue;

        const char *pkg_name = json_string_value(item);
        Id pkg_id = pool_str2id(pool, pkg_name, 1);  // Z pool.3: create=1 pro nový Id

        // Přidej do job jako SOLVER_INSTALL | SOLVER_SOLVABLE_NAME (z constantids.3 a bindings.3)
        queue_push2(job, SOLVER_INSTALL | SOLVER_SOLVABLE_NAME, pkg_id);
        log_debug(2, "Added mkdep '%s' to job queue at depth %d.", pkg_name, depth);

        // Rekurze: Najdi Solvable pro tento pkg a zavolej rekurzivně
        Id q;
        Solvable *s;
        FOR_REPO_SOLVABLES(repo, q, s) {
            if (s->name == pkg_id) {  // Match name Id
                add_mkdeps_to_queue(job, q, pool, repo, depth + 1);
                break;  // Předpokládáme unikátní name v repo
            }
        }
    }

    json_decref(mkdeps);  // Cleanup (z jansson, jak v parser.c)
    log_debug(1, "Added %zu mkdeps for solvable %d at depth %d.", mkdep_count, p, depth);
}

// Hlavní funkce: Resolvuj deps pro pkg_name (mkdeps rekurze + runtime, solver_solve, transaction)
// Vrátí Queue* installedresult (z bindings.3: transaction_installedresult), caller musí free
Queue *solve_dependencies(Solver *solv, const char *pkg_name, Pool *pool, Repo *repo) {
    if (!solv || !pool || !repo || !pkg_name) {
        log_error("Invalid arguments in solve_dependencies.");
        return NULL;
    }

    Queue *result = solv_calloc(1, sizeof(Queue));  // Výstup: alloc + init (z bindings.3 pro Queue)
    queue_init(result);

    // Vytvoř job queue (z solv.c)
    Queue job;
    queue_init(&job);

    // Najdi hlavní Solvable pro pkg_name
    Id name_id = pool_str2id(pool, pkg_name, 0);  // create=0, pokud neexistuje -> 0
    if (!name_id) {
        log_error("Package '%s' not found in pool string table.", pkg_name);
        goto cleanup;
    }

    Id main_p = 0;
    Solvable *s;
    FOR_REPO_SOLVABLES(repo, main_p, s) {
        if (s->name == name_id) break;  // První match
    }
    if (!main_p) {
        log_error("No solvable found for '%s' in repository.", pkg_name);
        goto cleanup;
    }
    
    log_debug(1, "Found solvable %d for package '%s'.", main_p, pkg_name);

    // Nejdřív rekurze pro mkdeps (build-time, separace od runtime)
    add_mkdeps_to_queue(&job, main_p, pool, repo, 0);

    // Pak runtime deps: Přidej hlavní pkg jako SOLVER_INSTALL (z constantids.3)
    // Použij SOLVER_SOLVABLE s Id solvable (main_p), ne SOLVER_SOLVABLE_NAME
    queue_push2(&job, SOLVER_INSTALL | SOLVER_SOLVABLE, main_p);

    // Spusť solver (z bindings.3: solver_solve(solv, &job))
    int problem_count = solver_solve(solv, &job);  // Vrátí počet problémů
    if (problem_count > 0) {
        log_warn("Solver found %d problems.", problem_count);
        for (int prob = 1; prob <= problem_count; prob++) {
            display_problem_proof(solv, prob);  // Zobraz proof (adaptace z solv.c)
        }
        // V testech chceme pokračovat i s problémy, v produkci by se ptal uživatele
        #ifndef UNIT_TESTS
        if (!user_confirm("Problems found. Continue anyway? [y/n]", 0)) {
            goto cleanup;
        }
        #endif
        log_info("Continuing despite solver problems...");
    } else {
        log_debug(1, "Solver completed successfully with no problems.");
    }

    // Note: Transaction is created by caller (main.c), not here
    // Just set a success flag in result to indicate solver completed
    queue_push(result, 1);  // Dummy value to indicate success

cleanup:
    queue_free(&job);
    if (result->count == 0) {  // Pokud prázdné, free a return NULL
        queue_free(result);
        solv_free(result);
        return NULL;
    }
    return result;  // Caller: queue_free(result) po použití
}

// Adaptace display_problem_proof z solv.c (doshowproof + showproof)
// Používá queue_init/free, solver_get_learnt, solver_get_decisionlist, solver_decisioninfo2str atd. (z bindings.3)
void display_problem_proof(Solver *solv, int problem) {
    if (!solv) {
        log_error("Invalid solver in display_problem_proof.");
        return;
    }

    Queue lq;
    int i;

    printf("\n");
    queue_init(&lq);
    solver_get_learnt(solv, problem, SOLVER_DECISIONLIST_PROBLEM, &lq);

    for (i = 0; i < lq.count; i++) {
        printf("Learnt rule #%d:\n", i + 1);
        internal_display_proof(solv, lq.elements[i], SOLVER_DECISIONLIST_LEARNTRULE, &lq);
    }

    printf("Proof:\n");
    internal_display_proof(solv, problem, SOLVER_DECISIONLIST_PROBLEM, &lq);

    queue_free(&lq);
}

// Interní helper pro display_proof (adaptace doshowproof z solv.c)
// Používá queue_init/free, solver_get_decisionlist, solver_decisioninfo2str, pool_solvidset2str atd.
static void internal_display_proof(Solver *solv, Id id, int flags, Queue *lq) {
    Pool *pool = solv->pool;
    Queue q, qp;
    int i, j;

    queue_init(&q);
    queue_init(&qp);
    solver_get_decisionlist(solv, id, flags | SOLVER_DECISIONLIST_SORTED | SOLVER_DECISIONLIST_WITHINFO | SOLVER_DECISIONLIST_MERGEDINFO, &q);

    for (i = 0; i < q.count; i += 8) {
        Id v = q.elements[i];
        int reason = q.elements[i + 1], bits = q.elements[i + 3], type = q.elements[i + 4];
        Id from = q.elements[i + 5], to = q.elements[i + 6], dep = q.elements[i + 7];

        if (reason != SOLVER_REASON_UNSOLVABLE && type == SOLVER_RULE_PKG_SAME_NAME) {
            continue;  // Skip "obvious" decisions (z solv.c)
        }

        solver_decisionlist_solvables(solv, &q, i, &qp);
        if (qp.count) {
            i += qp.count * 8 - 8;  // Skip merged
        }

        if (reason == SOLVER_REASON_UNSOLVABLE) {
            printf("unsolvable: ");
        } else {
            printf("%s %s: ", v < 0 ? "conflicted" : "installed", pool_solvidset2str(pool, &qp));
        }

        if (type == 0) {
            printf("%s\n", solver_reason2str(solv, reason));
            continue;
        }

        if (type == SOLVER_RULE_LEARNT && lq) {
            for (j = 0; j < lq->count; j++) {
                if (lq->elements[j] == q.elements[i + 2]) break;
            }
            if (j < lq->count) {
                printf("learnt rule #%d\n", j + 1);
                continue;
            }
        }

        printf("%s\n", solver_decisioninfo2str(solv, bits, type, from, to, dep));
    }

    queue_free(&qp);
    queue_free(&q);
    printf("\n");
}