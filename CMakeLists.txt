cmake_minimum_required(VERSION 3.12)
project(cli-player VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} ${SDL2_MIXER_LIBRARIES} ${NCURSES_LIBRARIES})
target_compile_options(${PROJECT_NAME} PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_MIXER_CFLAGS_OTHER})

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

