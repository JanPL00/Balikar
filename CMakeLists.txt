cmake_minimum_required(VERSION 3.10)
project(impl C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

# Použij pkg-config pro všechny knihovny KROMĚ LibSolv
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(Jansson REQUIRED jansson)
pkg_check_modules(LibArchive REQUIRED libarchive)
pkg_check_modules(CHECK REQUIRED check)

# Nastav Conda prefix (z ENV)
if(NOT CONDA_PREFIX)
    set(CONDA_PREFIX $ENV{CONDA_PREFIX})
endif()
if(NOT CONDA_PREFIX)
    message(FATAL_ERROR "CONDA_PREFIX not set - set it in CLion CMake Environment!")
endif()
message(STATUS "Using CONDA_PREFIX: ${CONDA_PREFIX}")  # Debug: Ověř v outputu

# LibSolv z Conda
set(LibSolv_INCLUDE_DIRS "\( {CONDA_PREFIX}/include" " \){CONDA_PREFIX}/include/solv")
set(LibSolv_LIBRARIES "${CONDA_PREFIX}/lib/libsolv.so" "${CONDA_PREFIX}/lib/libsolvext.so")

# NOVÉ: Explicitní -I flags pro Conda (nahrazuje CMAKE_C_FLAGS pro lepší expanzi)
add_compile_options(
        -Wall -Wextra -g
        -I${CONDA_PREFIX}/include
        -I${CONDA_PREFIX}/include/solv
)

# Debug pro include dirs
message(STATUS "LibSolv_INCLUDE_DIRS: ${LibSolv_INCLUDE_DIRS}")

enable_testing()

## ========================================
## MAIN EXECUTABLE FOR BALIKAR
## ========================================

add_executable(balikar
        src/balikar/main.c
        src/balikar/downloader.c
        src/common/common.c
        src/balikar/parser.c
        src/utils/cli_helpers.c
        src/utils/string_helpers.c
        src/utils/debug.c
        src/utils/print.c
        src/utils/system_helpers.c
        src/balikar/solver.c
        src/balikar/includes/solver.h
        src/balikar/builder.c
        src/balikar/includes/builder.h
)

target_include_directories(balikar PRIVATE
        ${LibSolv_INCLUDE_DIRS}  # Conda první
        ${CURL_INCLUDE_DIRS}
        ${Jansson_INCLUDE_DIRS}
        ${LibArchive_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        src/balikar/includes
        src/utils/includes
        src/common/includes
)

target_link_libraries(balikar
        ${LibSolv_LIBRARIES}
        ${CURL_LIBRARIES}
        ${Jansson_LIBRARIES}
        ${LibArchive_LIBRARIES}
        OpenSSL::Crypto
)

# RPATH pro runtime
set_target_properties(balikar PROPERTIES
        INSTALL_RPATH "${CONDA_PREFIX}/lib"
        BUILD_WITH_INSTALL_RPATH TRUE
)

## ========================================
## UNIT TESTS
## ========================================

add_executable(tests
        src/utils/cli_helpers.c
        src/utils/system_helpers.c
        src/utils/debug.c
        src/utils/print.c
        src/utils/string_helpers.c
        src/common/common.c
        src/balikar/parser.c
        src/balikar/solver.c
        src/balikar/downloader.c
        src/balikar/builder.c
        tests/tests-utils/test_cli_helpers.c
        tests/tests-utils/test_system_helpers.c
        tests/main.c
        tests/tests-src/test_parser.c
        tests/tests-src/test_solver.c
        tests/tests-src/test_downloader.c
        tests/tests-src/test_builder.c
)

target_include_directories(tests PRIVATE
        ${CHECK_INCLUDE_DIRS}
        ${LibSolv_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
        ${Jansson_INCLUDE_DIRS}
        src/utils/includes
        src/common/includes
        src/balikar/includes
        tests/tests-utils
)

target_link_libraries(tests
        ${CHECK_LIBRARIES}
        ${LibSolv_LIBRARIES}
        ${Jansson_LIBRARIES}
        ${CURL_LIBRARIES}
        OpenSSL::Crypto
        m
        pthread
        rt
        subunit
)

target_link_options(tests PRIVATE "LINKER:--wrap=uname")

target_compile_definitions(tests PRIVATE UNIT_TESTS)

add_test(NAME unit_tests COMMAND tests)