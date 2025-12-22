# Common library (shared code)
add_library(vibe-player-common STATIC
    src/player.cpp
    src/metadata.cpp
    src/metadata_cache.cpp
    src/playlist.cpp
)

target_include_directories(vibe-player-common PUBLIC include)

target_link_libraries(vibe-player-common
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    tag
)
