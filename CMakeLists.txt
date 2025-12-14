cmake_minimum_required(VERSION 3.12)
project(cli-player VERSION 1.0.0 LANGUAGES CXX)

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

FetchContent_MakeAvailable(cxxopts miniaudio)

# Include directories
include_directories(${miniaudio_SOURCE_DIR})
include_directories(${miniaudio_SOURCE_DIR}/extras)

# Source files
set(SOURCES
    src/main.cpp
    src/player.cpp
)

set(HEADERS
    src/player.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    cxxopts::cxxopts
    pthread
    m
    dl
)

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# CPack configuration for creating .deb packages
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "cli-player")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CLI Audio Player - Terminal-based audio player")
set(CPACK_PACKAGE_DESCRIPTION "A command-line audio player supporting WAV, MP3, FLAC, and OGG formats. Features include playback controls, volume adjustment, seeking, and directory playlist support.")
set(CPACK_PACKAGE_CONTACT "Rick Weyrauch <rpweyrauch@gmail.com>")
set(CPACK_PACKAGE_VENDOR "Rick Weyrauch")

# Debian-specific settings
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Rick Weyrauch <rpweyrauch@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/rpweyrauch/cli-player")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Package metadata
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

include(CPack)
