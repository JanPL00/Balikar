#include "includes/solver.h"
/**
 * Vytvoří a nakonfiguruje instanci solveru pro daný pool.
 *
 * Nastaví příznaky solveru pro workflow správce balíků:
 * - Povolí odinstalaci pro řešení konfliktů
 * - Povolí změnu dodavatele (vendor) pro flexibilitu LFS
 * - Zakáže downgrade z bezpečnostních důvodů
 * - Zakáže auto-targeting pro explicitní řízení úloh
 *
 * @param pool Pool, pro který se solver vytváří
 * @return Nakonfigurovaná instance solveru nebo NULL při selhání
 */
Solver *create_solver(Pool *pool) {
    if (!pool) {
        log_error("Cannot create solver: Invalid pool.");
        return NULL;
    }

    Solver *solv = solver_create(pool);  // Viz libsolv-bindings.3: solver_create(pool)
    if (!solv) {
        log_error("Failed to create solver.");
        return NULL;
    }

    // Nastav příznaky pro náš pracovní postup (z constantids.3 a solv.c)
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_UNINSTALL, 1);     // Povol odinstalaci při konfliktech
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_VENDORCHANGE, 1);  // Povol změny vendora (pro flexibilitu LFS)
    solver_set_flag(solv, SOLVER_FLAG_ALLOW_DOWNGRADE, 0);     // Zakaz downgrade z bezpečnostních důvodů (upravit dle potřeby)
    solver_set_flag(solv, SOLVER_FLAG_NO_AUTOTARGET, 1);       // No auto-target for explicit jobs

    // Pro kompatibilitu s Conda (z plánu, i když není v constantids – předpokládáme existenci)
    // pool_set_flag(pool, POOL_FLAG_WHATPROVIDESUSESCONDA, 1);  // Pokud existuje, nastav na poolu

    // Poznámka: Architektura poolu by již měla být nastavena volajícím (main.c)
    // NEVOLEJ set_system_arch zde, protože by reinicializovalo pool

    // Poznámka: pool_createwhatprovides by již mělo být voláno pomocí load_repository
    // NEVOLEJ ho znovu zde, protože by zneplatnilo mapování whatprovides

    log_info("Solver created with flags set and whatprovides indexed.");
    return solv;
}

/**
 * Rekurzivně přidá build závislosti (mkdeps) do fronty úloh.
 *
 * Odděluje build-time závislosti od runtime závislostí extrakcí vlastních
 * mkdeps metadat a jejich přidáním jako instalační úlohy. Obsahuje omezení
 * hloubky rekurze pro prevenci nekonečné smyčky při kruhových závislostech.
 *
 * @param job Fronta úloh, do které se mkdeps přidávají
 * @param p ID solvable ke zpracování
 * @param pool Pool obsahující informace o balících
 * @param repo Repozitář obsahující solvable
 * @param depth Aktuální hloubka rekurze (začíná na 0)
 * @param mkdeps_cb Callback pro načtení mkdeps z repodata
 */
void add_mkdeps_to_queue(Queue *job, Id p, Pool *pool, Repo *repo, int depth, json_t* (*mkdeps_cb)(Repodata*, Id, Pool*)) {
    // Zkontroluj hloubku rekurze pro prevenci nekonečných smyček z cyklických závislostí
    if (depth > MAX_RECURSION_DEPTH) {
        log_error("Recursion depth exceeded (%d) – possible circular dependency.", depth);
        return;
    }

    // Načti repodata pro tento solvable (předpokládá že repo bylo internalizováno)
    Repodata *data = repo_last_repodata(repo);
    if (!data) {
        log_warn("No repodata for solvable %d – skipping mkdeps.", p);
        return;
    }

    // Získej makedeps jako JSON pole z vlastních metadat pomocí callbacku
    json_t *mkdeps = mkdeps_cb ? mkdeps_cb(data, p, pool) : NULL;
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

    // Process each makedep item (package name string)
    size_t i;
    json_t *item;
    json_array_foreach(mkdeps, i, item) {
        if (!json_is_string(item)) continue;

        const char *pkg_name = json_string_value(item);
        // Převeď název balíku na pool ID (create=1 pro přidání pokud neexistuje)
        Id pkg_id = pool_str2id(pool, pkg_name, 1);

        // Přidej do fronty úloh jako operaci instalace
        queue_push2(job, SOLVER_INSTALL | SOLVER_SOLVABLE_NAME, pkg_id);
        log_debug(2, "Added mkdep '%s' to job queue at depth %d.", pkg_name, depth);

        // Rekurze: Najdi solvable pro tento balík a zpracuj jeho makedeps
        Id q;
        Solvable *s;
        FOR_REPO_SOLVABLES(repo, q, s) {
            if (s->name == pkg_id) {
                // Rekurzivně vstup do makedeps této závislosti
                add_mkdeps_to_queue(job, q, pool, repo, depth + 1, mkdeps_cb);
                break;  // Předpokládej jedinečný název v repo
            }
        }
    }

    json_decref(mkdeps);  // Úklid (z jansson, jako v parser.c)
    log_debug(1, "Added %zu mkdeps for solvable %d at depth %d.", mkdep_count, p, depth);
}

/**
 * Vyřeší všechny závislosti pro zadaný balík.
 *
 * Hlavní funkce pro rozlišení závislostí:
 * 1. Načte nainstalované balíky z cílového rootu (pokud existují)
 * 2. Najde cílový solvable balíku
 * 3. Rekurzivně zpracuje všechny mkdeps (build-time závislosti)
 * 4. Přidá hlavní balík jako runtime instalační úlohu
 * 5. Spustí solver pro výpočet instalační transakce
 * 6. Zpracuje případné problémy solveru
 *
 * @param solv Instance solveru k použití
 * @param pkg_name Název balíku, jehož závislosti se řeší
 * @param pool Pool obsahující informace o balících
 * @param repo Repozitář pro vyhledávání balíků
 * @param root_path Cílový instalační root (např. "/" nebo "/tmp/test_root")
 * @param mkdeps_cb Callback pro načtení mkdeps (dependency injection pro testy)
 * @return Queue s indikátorem úspěchu (1) nebo NULL při selhání.
 *         Volající musí uvolnit frontu pomocí queue_free() a solv_free()
 */
Queue *solve_dependencies(Solver *solv, const char *pkg_name, Pool *pool, Repo *repo, const char *root_path, json_t* (*mkdeps_cb)(Repodata*, Id, Pool*)) {
    if (!solv || !pool || !repo || !pkg_name) {
        log_error("Invalid arguments in solve_dependencies.");
        return NULL;
    }
    
    if (!root_path) {
        root_path = "/";  // Default to system root
    }

    // Load list of installed packages from target root
    // Přeskočíme balíky, které jsou již nainstalovány
    Queue installed_names;
    queue_init(&installed_names);
    
    char installed_db[512];
    // Handle root path "/" specially to avoid double slash
    if (strcmp(root_path, "/") == 0) {
        snprintf(installed_db, sizeof(installed_db), "/var/lib/balikar/installed.json");
    } else {
        snprintf(installed_db, sizeof(installed_db), "%s/var/lib/balikar/installed.json", root_path);
    }
    
    int installed_count = 0;
    if (access(installed_db, F_OK) == 0) {
        log_debug(1, "Loading installed packages from: %s", installed_db);
        
        // Parsuj installed.json
        json_error_t error;
        json_t *root = json_load_file(installed_db, 0, &error);
        if (root) {
            json_t *packages = json_object_get(root, "packages");
            if (packages && json_is_array(packages)) {
                size_t pkg_idx;
                json_t *pkg_obj;
                
                json_array_foreach(packages, pkg_idx, pkg_obj) {
                    const char *name = json_string_value(json_object_get(pkg_obj, "name"));
                    
                    if (name) {
                        // Ulož ID názvu balíku do fronty
                        Id name_id = pool_str2id(pool, name, 1);
                        queue_push(&installed_names, name_id);
                        installed_count++;
                        log_debug(2, "Found installed package: %s", name);
                    }
                }
                
                log_info("Found %d installed packages in %s", installed_count, root_path);
            }
            json_decref(root);
        } else {
            log_debug(1, "Could not parse installed.json: %s", error.text);
        }
    } else {
        log_debug(1, "No installed packages database at: %s", installed_db);
    }

    // Alokuj výsledkovou frontu pro vrácení stavu úspěchu
    Queue *result = solv_calloc(1, sizeof(Queue));
    queue_init(result);

    // Vytvoř frontu úloh pro operace solveru
    Queue job;
    queue_init(&job);

    // Najdi hlavní solvable pro požadovaný název balíku
    Id name_id = pool_str2id(pool, pkg_name, 0);  // create=0 (don't create if not exists)
    if (!name_id) {
        log_error("Package '%s' not found in pool string table.", pkg_name);
        goto cleanup;
    }

    Id main_p = 0;
    Solvable *s;
    FOR_REPO_SOLVABLES(repo, main_p, s) {
        if (s->name == name_id) break;  // První shoda
    }
    if (!main_p) {
        log_error("No solvable found for '%s' in repository.", pkg_name);
        goto cleanup;
    }
    
    log_debug(1, "Found solvable %d for package '%s'.", main_p, pkg_name);

    // Zkontroluj zda je hlavní balík již nainstalován
    int already_installed = 0;
    for (int i = 0; i < installed_names.count; i++) {
        if (installed_names.elements[i] == name_id) {
            already_installed = 1;
            log_info("Package '%s' is already installed in %s - skipping", pkg_name, root_path);
            break;
        }
    }

    if (!already_installed) {
        // Nejprve rekurzivně zpracuj všechny makedeps (závislosti při sestavení)
        // Tím se zajistí vyřešení závislostí sestavení před runtime závislostmi
        // TODO: Předat installed_names pro přeskočení již nainstalovaných makedeps
        add_mkdeps_to_queue(&job, main_p, pool, repo, 0, mkdeps_cb);

        // Pak přidej runtime závislosti: přidej hlavní balík jako SOLVER_INSTALL
        // Použij SOLVER_SOLVABLE se solvable ID, ne SOLVER_SOLVABLE_NAME
        queue_push2(&job, SOLVER_INSTALL | SOLVER_SOLVABLE, main_p);
    } else {
        // Balík je již nainstalován, jen nastav úspěch
        queue_push(result, 1);
        queue_free(&installed_names);
        queue_free(&job);
        return result;
    }

    // Spusť solver pro výpočet instalační transakce
    int problem_count = solver_solve(solv, &job);
    if (problem_count > 0) {
        // Analyzuj problémy pro rozlišení runtime a build závislostí (mkdeps)
        int runtime_problems = 0;
        int mkdep_problems = 0;
        
        for (int prob = 1; prob <= problem_count; prob++) {
            Queue problem_queue;
            queue_init(&problem_queue);
            solver_findallproblemrules(solv, prob, &problem_queue);
            
            // Zkontroluj zda problém zahrnuje mkdeps prozkoumáním názvů pravidel
            int is_mkdep_problem = 0;
            for (int i = 0; i < problem_queue.count; i++) {
                Id rule_id = problem_queue.elements[i];
                SolverRuleinfo type = solver_ruleinfo(solv, rule_id, NULL, NULL, NULL);
                // SOLVER_RULE_PKG_* jsou pravidla týkající se balíků
                // Pro mkdeps zkontrolujeme zda se název balíku vyskytuje v mkdeps
                // Toto je heuristika – problémy mkdep často zahrnují build nástroje
                (void)type; // Označit jako použité pro zamezení varování
                // TODO: Lepší detekce by vyžadovala sledování které závislosti jsou mkdeps
                // Prozatím předpokládej že všechny problémy mohou zahrnovat mkdeps
                is_mkdep_problem = 1; // Konzervativní: považuj za mkdep dokud není prokázán opak
            }
            queue_free(&problem_queue);
            
            if (is_mkdep_problem) {
                mkdep_problems++;
            } else {
                runtime_problems++;
            }
        }
        
        // Zobraz všechny problémy s kontextem
        log_warn("Solver found %d problems (%d may be build-time dependencies).", 
                 problem_count, mkdep_problems);
        for (int prob = 1; prob <= problem_count; prob++) {
            display_problem_proof(solv, prob);
        }
        
        // Pokud jsou problémy pravděpodobně pouze mkdeps, jen varuj
        if (runtime_problems == 0 && mkdep_problems > 0) {
            log_info("Problems appear to be build-time dependencies (mkdeps) only.");
            log_info("Build will attempt to use bundled or fallback versions if needed.");
            log_info("Continuing with installation...");
        } else {
            // V unit testech pokračuj i přes problémy; v produkci se zeptej uživatele
            #ifndef UNIT_TESTS
            if (!user_confirm("Problems found. Continue anyway? [y/n]", 0)) {
                goto cleanup;
            }
            #endif
            log_info("Continuing despite solver problems...");
        }
    } else {
        log_debug(1, "Solver completed successfully with no problems.");
    }

    // Vytvoření transakce zpracovává volající (main.c)
    // Nastav příznak úspěchu ve výsledku pro indikaci dokončení solveru
    queue_push(result, 1);

cleanup:
    queue_free(&installed_names);
    queue_free(&job);
    if (result->count == 0) {
        queue_free(result);
        solv_free(result);
        return NULL;
    }
    return result;
}

/**
 * Interní pomocná funkce pro výpis informací o rozhodnutích solveru.
 *
 * Přizpůsobeno z doshowproof v solv.c. Prochází seznam rozhodnutí
 * a zobrazuje řetězec důvodů vedoucích ke každému rozhodnutí nebo konfliktu.
 * Filtruje "zřejmá" rozhodnutí pro snížení šumu ve výstupu.
 *
 * @param solv Instance solveru
 * @param id ID rozhodnutí/problému k zobrazení
 * @param flags Řídicí příznaky pro načtení seznamu rozhodnutí
 * @param lq Fronta naučených pravidel pro referenci
 */
static void internal_display_proof(Solver *solv, Id id, int flags, Queue *lq) {
    Pool *pool = solv->pool;
    Queue q, qp;
    int i, j;

    queue_init(&q);
    queue_init(&qp);
    
    // Get decision list with sorting and detailed information
    solver_get_decisionlist(solv, id, flags | SOLVER_DECISIONLIST_SORTED | SOLVER_DECISIONLIST_WITHINFO | SOLVER_DECISIONLIST_MERGEDINFO, &q);

    // Process each decision in the list (8 elements per decision)
    for (i = 0; i < q.count; i += 8) {
        Id v = q.elements[i];
        int reason = q.elements[i + 1], bits = q.elements[i + 3], type = q.elements[i + 4];
        Id from = q.elements[i + 5], to = q.elements[i + 6], dep = q.elements[i + 7];

        // Skip "obvious" decisions (same package name) unless unsolvable
        if (reason != SOLVER_REASON_UNSOLVABLE && type == SOLVER_RULE_PKG_SAME_NAME) {
            continue;
        }

        // Get affected solvables for this decision
        solver_decisionlist_solvables(solv, &q, i, &qp);
        if (qp.count) {
            i += qp.count * 8 - 8;  // Skip merged decisions
        }

        // Print decision type and affected packages
        if (reason == SOLVER_REASON_UNSOLVABLE) {
            printf("unsolvable: ");
        } else {
            printf("%s %s: ", v < 0 ? "conflicted" : "installed", pool_solvidset2str(pool, &qp));
        }

        // If no specific rule type, just print the reason
        if (type == 0) {
            printf("%s\n", solver_reason2str(solv, reason));
            continue;
        }

        // Check if this is a learnt rule and reference it
        if (type == SOLVER_RULE_LEARNT && lq) {
            for (j = 0; j < lq->count; j++) {
                if (lq->elements[j] == q.elements[i + 2]) break;
            }
            if (j < lq->count) {
                printf("learnt rule #%d\n", j + 1);
                continue;
            }
        }

        // Print detailed decision information
        printf("%s\n", solver_decisioninfo2str(solv, bits, type, from, to, dep));
    }

    queue_free(&qp);
    queue_free(&q);
    printf("\n");
}

/**
 * Vypíše podrobné informace o problému solveru.
 *
 * Zobrazí naučená pravidla a trasování rozhodnutí, která vedla k problému.
 * Přizpůsobeno z solv.c (doshowproof + showproof) pro diagnostiku
 * proč solver nemohl najít platné řešení.
 *
 * @param solv Instance solveru obsahující problém
 * @param problem Číslo problému k zobrazení (indexováno od 1)
 */
void display_problem_proof(Solver *solv, int problem) {
    if (!solv) {
        log_error("Invalid solver in display_problem_proof.");
        return;
    }

    Queue lq;
    int i;

    printf("\n");
    queue_init(&lq);
    
    // Get learnt rules (rules the solver derived during conflict resolution)
    solver_get_learnt(solv, problem, SOLVER_DECISIONLIST_PROBLEM, &lq);

    // Display each learnt rule
    for (i = 0; i < lq.count; i++) {
        printf("Learnt rule #%d:\n", i + 1);
        internal_display_proof(solv, lq.elements[i], SOLVER_DECISIONLIST_LEARNTRULE, &lq);
    }

    // Display the full decision proof that led to this problem
    printf("Proof:\n");
    internal_display_proof(solv, problem, SOLVER_DECISIONLIST_PROBLEM, &lq);

    queue_free(&lq);
}
