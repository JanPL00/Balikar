// Main pro UNIT testy

#include <check.h>

// Deklarace suites z jednotlivých test souborů (přidej do test_*.c extern Suite *..._suite(void); pokud není)
extern Suite *system_helpers_suite(void);
extern Suite *cli_helpers_suite(void);
extern Suite *parser_suite(void);
extern Suite *solver_suite(void);

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
    Suite *s_parser = parser_suite();
    SRunner *sr_parser = srunner_create(s_parser);
    srunner_run_all(sr_parser, CK_NORMAL);
    failures += srunner_ntests_failed(sr_parser);
    srunner_free(sr_parser);

    // Spusť solver tests
    Suite *s_solver = solver_suite();
    SRunner *sr_solver = srunner_create(s_solver);
    srunner_run_all(sr_solver, CK_NORMAL);
    failures += srunner_ntests_failed(sr_solver);
    srunner_free(sr_solver);

    return (failures == 0) ? 0 : 1;
}