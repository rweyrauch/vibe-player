# Player executable
add_executable(tui-player
    src/main.cpp
)

target_link_libraries(tui-player
    vibe-player-common
    cxxopts::cxxopts
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    notcurses
    tag
    Threads::Threads
    m
)
