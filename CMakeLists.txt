cmake_minimum_required(VERSION 3.12)
project(vibe-player VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch libraries
include(FetchContent)

# Fetch cxxopts library
FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.2.1
)

# Fetch miniaudio library
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG master
)

# Fetch colors library
FetchContent_Declare(
    colors
    GIT_REPOSITORY https://github.com/ShakaUVM/colors.git
    GIT_TAG master
)

# Fetch nlohmann/json library
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# Fetch cpp-httplib library
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)

# Fetch spdlog library
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)

# Fetch TagLib library
FetchContent_Declare(
    taglib
    GIT_REPOSITORY https://github.com/taglib/taglib.git
    GIT_TAG v2.0.2
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)

# Fetch llama.cpp library
FetchContent_Declare(
    llama
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
    GIT_TAG master
)

# Configure llama.cpp build options
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
# Enable CPU optimizations but keep it portable
set(LLAMA_NATIVE OFF CACHE BOOL "" FORCE)
set(LLAMA_AVX2 ON CACHE BOOL "" FORCE)
set(LLAMA_FMA ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cxxopts miniaudio colors json httplib spdlog taglib llama)

# Include directories
include_directories(${miniaudio_SOURCE_DIR})
include_directories(${miniaudio_SOURCE_DIR}/extras)
include_directories(${colors_SOURCE_DIR})
include_directories(${taglib_SOURCE_DIR}/taglib)
include_directories(${taglib_SOURCE_DIR}/taglib/toolkit)
include_directories(${taglib_SOURCE_DIR}/taglib/mpeg/id3v2)
include_directories(${taglib_BINARY_DIR})

# Common library (shared code)
add_library(vibe-player-common STATIC
    src/player.cpp
    src/metadata.cpp
    src/metadata_cache.cpp
    src/playlist.cpp
)

target_link_libraries(vibe-player-common
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    tag
)

# Playlist generator executable
add_executable(vibe-playlist
    src/main_playlist_gen.cpp
    src/ai_prompt_builder.cpp
    src/ai_backend_claude.cpp
    src/ai_backend_llamacpp.cpp
    src/library_search.cpp
)

target_link_libraries(vibe-playlist
    vibe-player-common
    cxxopts::cxxopts
    nlohmann_json::nlohmann_json
    httplib::httplib
    spdlog::spdlog
    tag
    llama
    pthread
    m
    dl
    ssl
    crypto
)

# Player executable
add_executable(vibe-player
    src/main_player.cpp
)

target_link_libraries(vibe-player
    vibe-player-common
    cxxopts::cxxopts
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    tag
    pthread
    m
)

# Installation
install(TARGETS vibe-player vibe-playlist DESTINATION bin)

# CPack configuration for creating .deb packages
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "vibe-player")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vibe Player - AI-powered music player and playlist generator")
set(CPACK_PACKAGE_DESCRIPTION "An AI-powered command-line music player and playlist generator. Features include AI playlist generation, playback controls, volume adjustment, seeking, and full library search.")
set(CPACK_PACKAGE_CONTACT "Rick Weyrauch <rpweyrauch@gmail.com>")
set(CPACK_PACKAGE_VENDOR "Rick Weyrauch")

# Debian-specific settings
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Rick Weyrauch <rpweyrauch@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl3 (>= 3.0)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/rpweyrauch/vibe-player")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Package metadata
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

include(CPack)
