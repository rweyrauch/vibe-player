# Player executable
add_executable(vibe-player
    src/main.cpp
)

target_link_libraries(vibe-player
    vibe-player-common
    cxxopts::cxxopts
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    tag
    Threads::Threads
    m
)
