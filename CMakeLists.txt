cmake_minimum_required(VERSION 3.10)
project(impl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

# Použij pkg-config pro všechny knihovny
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(Jansson REQUIRED jansson)
pkg_check_modules(LibArchive REQUIRED libarchive)
pkg_check_modules(LibSolv REQUIRED libsolv)
pkg_check_modules(CHECK REQUIRED check)

enable_testing()


## ========================================
## MAIN EXECUTABLE FOR BALIKAR
## ========================================

add_executable(balikar
        src/balikar/main.c
        src/balikar/parser.c
        src/balikar/downloader.c
        src/common/common.c
        src/utils/repo_parser.c
        src/utils/cli_helpers.c
        src/utils/string_helpers.c
        src/utils/debug.c
        src/utils/print.c
        src/utils/system_helpers.c
)

target_include_directories(balikar PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${Jansson_INCLUDE_DIRS}
        ${LibArchive_INCLUDE_DIRS}
        ${LibSolv_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        src/balikar/includes
        src/utils/includes
        src/common/includes
)

target_link_libraries(balikar
        ${CURL_LIBRARIES}
        ${Jansson_LIBRARIES}
        ${LibArchive_LIBRARIES}
        ${LibSolv_LIBRARIES}
        OpenSSL::Crypto  # Přidáno pro linking OpenSSL (Crypto pro EVP/SHA256)
)


## ========================================
## UNIT TESTS
## ========================================

add_executable(tests
        src/utils/cli_helpers.c
        src/utils/system_helpers.c
        src/common/common.c
        tests/tests-utils/test_cli_helpers.c
        tests/tests-utils/test_system_helpers.c
        tests/tests-utils/main.c
)

target_include_directories(tests PRIVATE
        ${CHECK_INCLUDE_DIRS}
        ${LibSolv_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
        src/utils/includes
        src/common/includes
        tests/tests-utils
)

target_link_libraries(tests
        ${CHECK_LIBRARIES}
        ${LibSolv_LIBRARIES}
        OpenSSL::Crypto
        m
        pthread
        rt
        subunit
)

target_link_options(tests PRIVATE "LINKER:--wrap=uname")

target_compile_definitions(tests PRIVATE UNIT_TESTS)

add_test(NAME unit_tests COMMAND tests)

