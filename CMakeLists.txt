cmake_minimum_required(VERSION 3.10)
project(impl C)

set(CMAKE_C_STANDARD 11)

# Option pro statickou kompilaci
option(BUILD_STATIC "Build fully static binary" OFF)

if(BUILD_STATIC)
    message(STATUS "Building STATIC binary")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_EXE_LINKER_FLAGS "-static")
else()
    message(STATUS "Building DYNAMIC binary")
endif()

find_package(PkgConfig REQUIRED)

if(BUILD_STATIC)
    # Pro statický build: hledej .a soubory přímo
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(Jansson REQUIRED jansson)
    pkg_check_modules(LibArchive REQUIRED libarchive)
    pkg_check_modules(CHECK REQUIRED check)
    
    # Přidej statické závislosti libcurl
    find_library(SSL_LIBRARY NAMES libssl.a ssl REQUIRED)
    find_library(CRYPTO_LIBRARY NAMES libcrypto.a crypto REQUIRED)
    find_library(ZLIB_LIBRARY NAMES libz.a z REQUIRED)
    
    # Přidej statické závislosti libarchive
    find_library(LZMA_LIBRARY NAMES liblzma.a lzma)
    find_library(BZ2_LIBRARY NAMES libbz2.a bz2)
    find_library(ZSTD_LIBRARY NAMES libzstd.a zstd)
    
    # Další potřebné knihovny
    find_library(NGHTTP2_LIBRARY NAMES libnghttp2.a nghttp2)
    find_library(PSL_LIBRARY NAMES libpsl.a psl)
    find_library(IDN2_LIBRARY NAMES libidn2.a idn2)
    
    set(STATIC_EXTRA_LIBS 
        ${SSL_LIBRARY}
        ${CRYPTO_LIBRARY}
        ${ZLIB_LIBRARY}
    )
    
    # Přidej volitelné knihovny pokud byly nalezeny
    if(LZMA_LIBRARY)
        list(APPEND STATIC_EXTRA_LIBS ${LZMA_LIBRARY})
    endif()
    if(BZ2_LIBRARY)
        list(APPEND STATIC_EXTRA_LIBS ${BZ2_LIBRARY})
    endif()
    if(ZSTD_LIBRARY)
        list(APPEND STATIC_EXTRA_LIBS ${ZSTD_LIBRARY})
    endif()
    if(NGHTTP2_LIBRARY)
        list(APPEND STATIC_EXTRA_LIBS ${NGHTTP2_LIBRARY})
    endif()
    if(PSL_LIBRARY)
        list(APPEND STATIC_EXTRA_LIBS ${PSL_LIBRARY})
    endif()
    if(IDN2_LIBRARY)
        list(APPEND STATIC_EXTRA_LIBS ${IDN2_LIBRARY})
    endif()
    
    message(STATUS "Static extra libraries: ${STATIC_EXTRA_LIBS}")
else()
    # Pro dynamický build: použij pkg-config
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(Jansson REQUIRED jansson)
    pkg_check_modules(LibArchive REQUIRED libarchive)
    pkg_check_modules(CHECK REQUIRED check)
endif()

# Nastav Conda prefix (z ENV)
if(NOT CONDA_PREFIX)
    set(CONDA_PREFIX $ENV{CONDA_PREFIX})
endif()
if(NOT CONDA_PREFIX)
    message(FATAL_ERROR "CONDA_PREFIX not set - set it in CLion CMake Environment!")
endif()
message(STATUS "Using CONDA_PREFIX: ${CONDA_PREFIX}")  # Debug: Ověř v outputu

# LibSolv z Conda nebo system
set(LibSolv_INCLUDE_DIRS "${CONDA_PREFIX}/include" "${CONDA_PREFIX}/include/solv")

if(BUILD_STATIC)
    # Pro statický build: hledej .a soubory
    find_library(LIBSOLVEXT_STATIC 
        NAMES libsolvext.a solvext
        PATHS ${CONDA_PREFIX}/lib ${CONDA_PREFIX}/lib/x86_64-linux-gnu /usr/lib /usr/local/lib
        REQUIRED
    )
    find_library(LIBSOLV_STATIC 
        NAMES libsolv.a solv
        PATHS ${CONDA_PREFIX}/lib ${CONDA_PREFIX}/lib/x86_64-linux-gnu /usr/lib /usr/local/lib
        REQUIRED
    )
    set(LibSolv_LIBRARIES ${LIBSOLVEXT_STATIC} ${LIBSOLV_STATIC})
    message(STATUS "Using STATIC libsolv: ${LibSolv_LIBRARIES}")
else()
    # Pro dynamický build: použij .so soubory
    if(EXISTS "${CONDA_PREFIX}/lib/x86_64-linux-gnu/libsolv.so")
        set(LibSolv_LIBRARIES "${CONDA_PREFIX}/lib/x86_64-linux-gnu/libsolv.so" "${CONDA_PREFIX}/lib/x86_64-linux-gnu/libsolvext.so")
    else()
        set(LibSolv_LIBRARIES "${CONDA_PREFIX}/lib/libsolv.so" "${CONDA_PREFIX}/lib/libsolvext.so")
    endif()
    message(STATUS "Using DYNAMIC libsolv: ${LibSolv_LIBRARIES}")
endif()

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
        src/common/config.c
        src/balikar/parser.c
        src/utils/cli_helpers.c
        src/utils/string_helpers.c
        src/utils/print.c
        src/utils/system_helpers.c
        src/balikar/solver.c
        src/balikar/includes/solver.h
        src/balikar/builder.c
        src/balikar/includes/builder.h
        src/balikar/installer.c
        src/balikar/includes/installer.h
)

# Build binary to project root instead of build/
set_target_properties(balikar PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_include_directories(balikar PRIVATE
        ${LibSolv_INCLUDE_DIRS}  # Conda první
        ${CURL_INCLUDE_DIRS}
        ${Jansson_INCLUDE_DIRS}
        ${LibArchive_INCLUDE_DIRS}
        src/balikar/includes
        src/utils/includes
        src/common/includes
)

if(BUILD_STATIC)
    # Statický build: linkuj všechny závislosti
    target_link_libraries(balikar
        ${LibSolv_LIBRARIES}
        ${CURL_LIBRARIES}
        ${Jansson_LIBRARIES}
        ${LibArchive_LIBRARIES}
        ${STATIC_EXTRA_LIBS}
        pthread
        m
        dl
        rt
    )
    
    # Pro statický build: explicitní static flags
    target_link_options(balikar PRIVATE
        -static
        -static-libgcc
    )
else()
    # Dynamický build: standardní linkování
    target_link_libraries(balikar
        ${LibSolv_LIBRARIES}
        ${CURL_LIBRARIES}
        ${Jansson_LIBRARIES}
        ${LibArchive_LIBRARIES}
    )
    
    # RPATH configuration for distribution
    # Use $ORIGIN for portable binary with bundled libraries
    set_target_properties(balikar PROPERTIES
        INSTALL_RPATH "$ORIGIN:$ORIGIN/lib:$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH_USE_LINK_PATH FALSE
    )
endif()

## ========================================
## UNIT TESTS
## ========================================

add_executable(tests
        src/utils/cli_helpers.c
        src/utils/system_helpers.c
        src/utils/print.c
        src/utils/string_helpers.c
        src/common/common.c
        src/balikar/parser.c
        src/balikar/solver.c
        src/balikar/downloader.c
        src/balikar/builder.c
        src/balikar/installer.c
        tests/tests-utils/test_cli_helpers.c
        tests/tests-utils/test_system_helpers.c
        tests/main.c
        tests/tests-src/test_parser.c
        tests/tests-src/test_solver.c
        tests/tests-src/test_downloader.c
        tests/tests-src/test_builder.c
        tests/tests-src/test_installer.c
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
        m
        pthread
        rt
        subunit
)

target_link_options(tests PRIVATE "LINKER:--wrap=uname")

target_compile_definitions(tests PRIVATE UNIT_TESTS)

add_test(NAME unit_tests COMMAND tests)