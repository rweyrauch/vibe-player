cmake_minimum_required(VERSION 3.12)
project(cli-player VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch cxxopts library
include(FetchContent)
FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.2.1
)
FetchContent_MakeAvailable(cxxopts)

# Find SDL2 and SDL2_mixer
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
pkg_check_modules(NCURSES REQUIRED ncurses)

# Include directories
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${SDL2_MIXER_INCLUDE_DIRS})
include_directories(${NCURSES_INCLUDE_DIRS})

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
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${NCURSES_LIBRARIES}
    cxxopts::cxxopts
)
target_compile_options(${PROJECT_NAME} PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_MIXER_CFLAGS_OTHER})

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# CPack configuration for creating .deb packages
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "cli-player")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CLI Audio Player - Terminal-based audio player with ncurses interface")
set(CPACK_PACKAGE_DESCRIPTION "A command-line audio player with ncurses interface supporting WAV, MP3, FLAC, and OGG formats. Features include playback controls, volume adjustment, seeking, directory playlist support, and real-time frequency visualization with colored ncurses display.")
set(CPACK_PACKAGE_CONTACT "Rick Weyrauch <rpweyrauch@gmail.com>")
set(CPACK_PACKAGE_VENDOR "Rick Weyrauch")

# Debian-specific settings
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Rick Weyrauch <rpweyrauch@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libsdl2-mixer-2.0-0, libncurses6")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/rpweyrauch/cli-player")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Package metadata
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

include(CPack)
