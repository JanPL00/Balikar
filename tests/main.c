// Main pro UNIT testy

#include <check.h>

// Deklarace suites z jednotlivých test souborů (přidej do test_*.c extern Suite *..._suite(void); pokud není)
extern Suite *system_helpers_suite(void);
extern Suite *cli_helpers_suite(void);
extern Suite *parser_suite(void);
extern Suite *solver_suite(void);
extern Suite *downloader_suite(void);
extern Suite *builder_suite(void);
extern Suite *installer_suite(void);

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

    // Spusť downloader tests
    Suite *s_downloader = downloader_suite();
    SRunner *sr_downloader = srunner_create(s_downloader);
    srunner_run_all(sr_downloader, CK_NORMAL);
    failures += srunner_ntests_failed(sr_downloader);
    srunner_free(sr_downloader);

    // Spusť builder tests
    Suite *s_builder = builder_suite();
    SRunner *sr_builder = srunner_create(s_builder);
    srunner_run_all(sr_builder, CK_NORMAL);
    failures += srunner_ntests_failed(sr_builder);
    srunner_free(sr_builder);

    // Spusť installer tests
    Suite *s_installer = installer_suite();
    SRunner *sr_installer = srunner_create(s_installer);
    srunner_run_all(sr_installer, CK_NORMAL);
    failures += srunner_ntests_failed(sr_installer);
    srunner_free(sr_installer);

    return (failures == 0) ? 0 : 1;
}