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


add_executable(balikar
        src/balikar/main.c
        src/balikar/parser.c
        src/balikar/downloader.c
        src/utils/repo_parser.c
        src/utils/include/repo_parser.h)

target_include_directories(balikar PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${Jansson_INCLUDE_DIRS}
        ${LibArchive_INCLUDE_DIRS}
        ${LibSolv_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        ${CHECK_INCLUDE_DIRS}
)

target_link_libraries(balikar
        ${CURL_LIBRARIES}
        ${Jansson_LIBRARIES}
        ${LibArchive_LIBRARIES}
        ${LibSolv_LIBRARIES}
        ${CHECK_LIBRARIES}
        OpenSSL::Crypto  # Přidáno pro linking OpenSSL (Crypto pro EVP/SHA256)
)
