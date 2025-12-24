# Common library (shared code)
add_library(vibe-player-common STATIC
    src/player.cpp
    src/metadata.cpp
    src/metadata_cache.cpp
    src/miniaudio_impl.cpp
    src/playlist.cpp
    src/dropbox_client.cpp
    src/path_handler.cpp
    src/temp_file_manager.cpp
    src/dropbox_state.cpp
)

target_include_directories(vibe-player-common PUBLIC include)

# Find OpenSSL for HTTPS support
find_package(OpenSSL REQUIRED)

target_link_libraries(vibe-player-common
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    tag
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
)
