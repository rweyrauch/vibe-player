# Playlist generator executable
add_executable(vibe-playlist
    src/main.cpp
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

# Installation
install(TARGETS vibe-playlist DESTINATION bin)