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
    # Při statickém buildu upřednostni CONDA_PREFIX před systémovými cestami
    if(CONDA_PREFIX)
        set(ENV{PKG_CONFIG_PATH} "${CONDA_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        set(ENV{PKG_CONFIG_LIBDIR} "")
    endif()

    # pkg_check_modules s STATIC variantou: vrátí v CURL_STATIC_LIBRARIES /
    # CURL_STATIC_LDFLAGS všechny tranzitivní závislosti tak, jak je
    # poskytuje pkg-config --static (tj. včetně gssapi, brotli, idn2,
    # psl, zstd, nghttp2 atd.). Toto je klíčové pro Variantu B (bez
    # Condy), kde systémová libcurl.a je sestavena se všemi rozšířeními.
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(Jansson REQUIRED jansson)
    pkg_check_modules(LibArchive REQUIRED libarchive)
    pkg_check_modules(CHECK REQUIRED check)

    # Nahraď -lcurl plnou cestou aby linker nenašel systémovou verzi
    find_library(CURL_LIBRARY_STATIC NAMES libcurl.a curl REQUIRED
        HINTS ${CONDA_PREFIX}/lib /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib
        NO_DEFAULT_PATH)
    if(NOT CURL_LIBRARY_STATIC)
        find_library(CURL_LIBRARY_STATIC NAMES libcurl.a curl REQUIRED)
    endif()

    # Přidej statické závislosti libcurl
    find_library(SSL_LIBRARY NAMES libssl.a ssl REQUIRED
        HINTS ${CONDA_PREFIX}/lib /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)
    find_library(CRYPTO_LIBRARY NAMES libcrypto.a crypto REQUIRED
        HINTS ${CONDA_PREFIX}/lib /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)
    find_library(ZLIB_LIBRARY NAMES libz.a z REQUIRED
        HINTS ${CONDA_PREFIX}/lib /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)

    # Přidej statické závislosti libarchive
    find_library(LZMA_LIBRARY NAMES liblzma.a lzma)
    find_library(BZ2_LIBRARY NAMES libbz2.a bz2)
    find_library(ZSTD_LIBRARY NAMES libzstd.a zstd)

    # Další potřebné knihovny
    find_library(NGHTTP2_LIBRARY NAMES libnghttp2.a nghttp2)
    find_library(PSL_LIBRARY NAMES libpsl.a psl)
    find_library(IDN2_LIBRARY NAMES libidn2.a idn2)
    # c-ares a brotli: potřeba na Alpine kde curl-static je sestaven s jejich podporou
    find_library(CARES_LIBRARY NAMES libcares.a cares)
    find_library(BROTLI_DEC_LIBRARY NAMES libbrotlidec.a brotlidec)
    find_library(BROTLI_COMMON_LIBRARY NAMES libbrotlicommon.a brotlicommon)
    # libunistring: potřeba pro libidn2-static na Alpine
    find_library(UNISTRING_LIBRARY NAMES libunistring.a unistring)

    # GSS-API / Kerberos: Debian/Ubuntu libcurl.a je linkována proti libgssapi_krb5
    # (Variant B). Conda libcurl.a je sestavena bez GSS, takže tyto symboly chybí.
    find_library(GSSAPI_KRB5_LIBRARY NAMES libgssapi_krb5.a gssapi_krb5)
    find_library(KRB5_LIBRARY NAMES libkrb5.a krb5)
    find_library(K5CRYPTO_LIBRARY NAMES libk5crypto.a k5crypto)
    find_library(KRB5SUPPORT_LIBRARY NAMES libkrb5support.a krb5support)
    find_library(COMERR_LIBRARY NAMES libcom_err.a com_err)
    # LDAP / SASL: další volitelné rozšíření systémové libcurl.a
    find_library(LDAP_LIBRARY NAMES libldap.a ldap)
    find_library(LBER_LIBRARY NAMES liblber.a lber)
    find_library(SASL2_LIBRARY NAMES libsasl2.a sasl2)
    # RTMP: ještě další možné rozšíření
    find_library(RTMP_LIBRARY NAMES librtmp.a rtmp)

    set(STATIC_EXTRA_LIBS
        ${SSL_LIBRARY}
        ${CRYPTO_LIBRARY}
        ${ZLIB_LIBRARY}
    )

    # Přidej volitelné knihovny pokud byly nalezeny
    foreach(_lib
            LZMA_LIBRARY BZ2_LIBRARY ZSTD_LIBRARY
            NGHTTP2_LIBRARY PSL_LIBRARY IDN2_LIBRARY
            CARES_LIBRARY BROTLI_DEC_LIBRARY BROTLI_COMMON_LIBRARY
            UNISTRING_LIBRARY
            LDAP_LIBRARY LBER_LIBRARY SASL2_LIBRARY RTMP_LIBRARY
            GSSAPI_KRB5_LIBRARY KRB5_LIBRARY K5CRYPTO_LIBRARY
            KRB5SUPPORT_LIBRARY COMERR_LIBRARY)
        if(${_lib})
            list(APPEND STATIC_EXTRA_LIBS ${${_lib}})
        endif()
    endforeach()

    # Pojistka: přidej --as-needed --start-group/--end-group pro tolerování
    # cyklických závislostí mezi krb5 / gssapi / com_err / openssl
    if(GSSAPI_KRB5_LIBRARY)
        set(STATIC_EXTRA_LIBS
            "-Wl,--start-group" ${STATIC_EXTRA_LIBS} "-Wl,--end-group")
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
        src/balikar/command_context.c
        src/balikar/command_registry.c
        src/balikar/commands/cmd_repos.c
        src/balikar/commands/cmd_autoremove.c
        src/balikar/commands/cmd_remove.c
        src/balikar/commands/cmd_resolve.c
        src/balikar/commands/cmd_clean_cache.c
        src/balikar/commands/cmd_repo_update.c
        src/balikar/commands/cmd_repo.c
        src/balikar/commands/cmd_download.c
        src/balikar/commands/cmd_install.c
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
        src/balikar/includes/command.h
        src/balikar/includes/command_context.h
        src/balikar/includes/command_registry.h
        src/balikar/includes/commands.h
)

# Build binary into build/ directory (not project root directly)
# POST_BUILD then writes content in-place to project root to preserve inode
# so Docker bind mounts see the updated binary without container restart.
set_target_properties(balikar PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_custom_command(TARGET balikar POST_BUILD
    COMMAND dd "if=${CMAKE_BINARY_DIR}/balikar" "of=${CMAKE_SOURCE_DIR}/balikar"
    COMMAND chmod +x "${CMAKE_SOURCE_DIR}/balikar"
    COMMENT "Overwriting balikar in-place (preserving inode for Docker bind mount)"
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
    # --start-group/--end-group: linker opakuje průchody pro rozlišení
    # kruhových závislostí (libssl.a ↔ libcrypto.a v OpenSSL 3)
    target_link_libraries(balikar
        -Wl,--start-group
        ${LibSolv_LIBRARIES}
        ${CURL_LIBRARY_STATIC}
        ${Jansson_LIBRARIES}
        ${LibArchive_LIBRARIES}
        ${STATIC_EXTRA_LIBS}
        -Wl,--end-group
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
        $<IF:$<BOOL:${BUILD_STATIC}>,${CURL_LIBRARY_STATIC},${CURL_LIBRARIES}>
        m
        pthread
        rt
        subunit
)

target_link_options(tests PRIVATE "LINKER:--wrap=uname")

target_compile_definitions(tests PRIVATE UNIT_TESTS)

add_test(NAME unit_tests COMMAND tests)