//
// all_tests.c - Jednotný test runner pro všechny unit testy (pomocí Check)
// Spustí suite z test_system_helpers.c a test_cli_helpers.c.
// Struktura: Kompiluj s: gcc all_tests.c test_system_helpers.c test_cli_helpers.c ../src/utils/system_helpers.c ../src/utils/cli_helpers.c ../src/common.c -o all_tests -lcheck -lsolv -lm -pthread -lrt -lsubunit
// Pak: ./all_tests
// Created by [tvé jméno] on 06/11/2025.
//

#include <check.h>

// Deklarace suites z jednotlivých test souborů (přidej do test_*.c extern Suite *..._suite(void); pokud není)
extern Suite *system_helpers_suite(void);
extern Suite *cli_helpers_suite(void);

int main(void) {
    int failures = 0;

    // Spusť system_helpers tests
    Suite *s_system = system_helpers_suite();
    SRunner *sr_system = srunner_create(s_system);
    srunner_run_all(sr_system, CK_NORMAL);
    failures += srunner_ntests_failed(sr_system);
    srunner_free(sr_system);

    // Spusť cli_helpers tests
    Suite *s_cli = cli_helpers_suite();
    SRunner *sr_cli = srunner_create(s_cli);
    srunner_run_all(sr_cli, CK_NORMAL);
    failures += srunner_ntests_failed(sr_cli);
    srunner_free(sr_cli);

    // Přidej další suites zde (např. test_string_helpers_suite() atd.)

    return (failures == 0) ? 0 : 1;
}