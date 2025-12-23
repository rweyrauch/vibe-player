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

# Fetch notcurses library
FetchContent_Declare(
    notcurses
    GIT_REPOSITORY https://github.com/dankamongmen/notcurses.git
    GIT_TAG v3.0.11
)
set(USE_QRCODEGEN OFF CACHE BOOL "" FORCE)
set(USE_PANDOC OFF CACHE BOOL "" FORCE)
set(BUILD_FFI_LIBRARY OFF CACHE BOOL "" FORCE)
set(BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(USE_MULTIMEDIA "ffmpeg" CACHE STRING "" FORCE)
set(USE_DEFLATE OFF CACHE BOOL "" FORCE)

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

FetchContent_MakeAvailable(cxxopts miniaudio colors json httplib spdlog notcurses taglib llama)

find_package(Threads REQUIRED)

# Include directories
include_directories(${miniaudio_SOURCE_DIR})
include_directories(${miniaudio_SOURCE_DIR}/extras)
include_directories(${colors_SOURCE_DIR})
include_directories(${taglib_SOURCE_DIR}/taglib)
include_directories(${taglib_SOURCE_DIR}/taglib/toolkit)
include_directories(${taglib_SOURCE_DIR}/taglib/mpeg/id3v2)
include_directories(${taglib_BINARY_DIR})
include_directories(${notcurses_SOURCE_DIR}/include)

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

add_subdirectory(common)
add_subdirectory(player)
add_subdirectory(list)
add_subdirectory(tui_player)