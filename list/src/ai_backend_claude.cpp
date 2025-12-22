#include "ai_backend_claude.h"
#include "ai_prompt_builder.h"
#include "library_search.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>

using json = nlohmann::json;

ClaudeBackend::ClaudeBackend(const std::string& api_key, ClaudeModel model)
    : api_key_(api_key), model_(getModelId(model)) {
}

ClaudeBackend::ClaudeBackend(const std::string& api_key, const std::string& model_id)
    : api_key_(api_key), model_(model_id) {
}

std::string ClaudeBackend::getModelId(ClaudeModel model) {
    switch (model) {
        case ClaudeModel::FAST:
            return "claude-3-5-haiku-20241022";
        case ClaudeModel::BALANCED:
            return "claude-3-5-sonnet-20240620";  // Using June 2024 version for wider availability
        case ClaudeModel::BEST:
            return "claude-sonnet-4-5-20250929";  // Latest Claude Sonnet 4.5
        default:
            return "claude-3-5-haiku-20241022";
    }
}

ClaudeModel ClaudeBackend::parseModelPreset(const std::string& preset) {
    std::string lower = preset;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "fast" || lower == "haiku") {
        return ClaudeModel::FAST;
    } else if (lower == "balanced" || lower == "sonnet") {
        return ClaudeModel::BALANCED;
    } else if (lower == "best" || lower == "opus") {
        return ClaudeModel::BEST;
    }

    // Default to fast
    return ClaudeModel::FAST;
}

bool ClaudeBackend::validate(std::string& error_message) const {
    if (api_key_.empty()) {
        error_message = "ANTHROPIC_API_KEY not set. Get a key from https://console.anthropic.com";
        return false;
    }
    return true;
}

json ClaudeBackend::buildToolDefinitions() const {
    return json::array({
        {
            {"name", "search_by_artist"},
            {"description", "Search the music library for tracks by a specific artist. Use this to find all songs by an artist or band."},
            {"input_schema", {
                {"type", "object"},
                {"properties", {
                    {"artist_name", {
                        {"type", "string"},
                        {"description", "The name of the artist or band to search for (partial matches supported)"}
                    }},
                    {"max_results", {
                        {"type", "number"},
                        {"description", "Maximum number of results to return (default: 100)"},
                        {"default", 100}
                    }}
                }},
                {"required", json::array({"artist_name"})}
            }}
        },
        {
            {"name", "search_by_genre"},
            {"description", "Search the music library for tracks in a specific genre. Use this to find songs by musical style."},
            {"input_schema", {
                {"type", "object"},
                {"properties", {
                    {"genre", {
                        {"type", "string"},
                        {"description", "The genre to search for (e.g., 'rock', 'jazz', 'classical')"}
                    }},
                    {"max_results", {
                        {"type", "number"},
                        {"description", "Maximum number of results to return (default: 100)"},
                        {"default", 100}
                    }}
                }},
                {"required", json::array({"genre"})}
            }}
        },
        {
            {"name", "search_by_album"},
            {"description", "Search the music library for tracks from a specific album."},
            {"input_schema", {
                {"type", "object"},
                {"properties", {
                    {"album_name", {
                        {"type", "string"},
                        {"description", "The name of the album to search for (partial matches supported)"}
                    }},
                    {"max_results", {
                        {"type", "number"},
                        {"description", "Maximum number of results to return (default: 100)"},
                        {"default", 100}
                    }}
                }},
                {"required", json::array({"album_name"})}
            }}
        },
        {
            {"name", "search_by_title"},
            {"description", "Search the music library for tracks by song title or keywords in the title."},
            {"input_schema", {
                {"type", "object"},
                {"properties", {
                    {"title", {
                        {"type", "string"},
                        {"description", "The song title or keywords to search for (partial matches supported)"}
                    }},
                    {"max_results", {
                        {"type", "number"},
                        {"description", "Maximum number of results to return (default: 100)"},
                        {"default", 100}
                    }}
                }},
                {"required", json::array({"title"})}
            }}
        },
        {
            {"name", "search_by_year_range"},
            {"description", "Search the music library for tracks released within a specific year range."},
            {"input_schema", {
                {"type", "object"},
                {"properties", {
                    {"start_year", {
                        {"type", "number"},
                        {"description", "The starting year (inclusive)"}
                    }},
                    {"end_year", {
                        {"type", "number"},
                        {"description", "The ending year (inclusive)"}
                    }},
                    {"max_results", {
                        {"type", "number"},
                        {"description", "Maximum number of results to return (default: 100)"},
                        {"default", 100}
                    }}
                }},
                {"required", json::array({"start_year", "end_year"})}
            }}
        },
        {
            {"name", "get_library_overview"},
            {"description", "Get an overview of the music library including total tracks, unique artists, genres, and albums. Use this first to understand what's available."},
            {"input_schema", {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }}
        }
    });
}

json ClaudeBackend::executeToolCall(
    const std::string& tool_name,
    const json& tool_input,
    const LibrarySearch& search_engine) const {

    spdlog::debug("Executing tool: {} with input: {}", tool_name, tool_input.dump());

    if (tool_name == "search_by_artist") {
        std::string artist = tool_input["artist_name"];
        int max_results = tool_input.value("max_results", 100);

        auto result = search_engine.searchByArtist(artist, max_results);

        json tracks = json::array();
        for (size_t idx : result.track_indices) {
            tracks.push_back({
                {"index", idx},
                {"title", result.track_indices.size() > 0 ? "..." : ""}  // Placeholder
            });
        }

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}
        };

    } else if (tool_name == "search_by_genre") {
        std::string genre = tool_input["genre"];
        int max_results = tool_input.value("max_results", 100);

        auto result = search_engine.searchByGenre(genre, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}
        };

    } else if (tool_name == "search_by_album") {
        std::string album = tool_input["album_name"];
        int max_results = tool_input.value("max_results", 100);

        auto result = search_engine.searchByAlbum(album, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}
        };

    } else if (tool_name == "search_by_title") {
        std::string title = tool_input["title"];
        int max_results = tool_input.value("max_results", 100);

        auto result = search_engine.searchByTitle(title, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}
        };

    } else if (tool_name == "search_by_year_range") {
        int start_year = tool_input["start_year"];
        int end_year = tool_input["end_year"];
        int max_results = tool_input.value("max_results", 100);

        auto result = search_engine.searchByYearRange(start_year, end_year, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}
        };

    } else if (tool_name == "get_library_overview") {
        auto artists = search_engine.getUniqueArtists();
        auto genres = search_engine.getUniqueGenres();
        auto albums = search_engine.getUniqueAlbums();

        // Sample some artists and genres to show
        json sample_artists = json::array();
        for (size_t i = 0; i < std::min(size_t(20), artists.size()); i++) {
            sample_artists.push_back(artists[i]);
        }

        json sample_genres = json::array();
        for (size_t i = 0; i < std::min(size_t(20), genres.size()); i++) {
            sample_genres.push_back(genres[i]);
        }

        return {
            {"total_tracks", search_engine.getUniqueArtists().size()}, // Placeholder
            {"unique_artists", artists.size()},
            {"unique_genres", genres.size()},
            {"unique_albums", albums.size()},
            {"sample_artists", sample_artists},
            {"sample_genres", sample_genres}
        };
    }

    return {{"error", "Unknown tool: " + tool_name}};
}

std::optional<std::vector<std::string>> ClaudeBackend::generate(
    const std::string& user_prompt,
    const std::vector<TrackMetadata>& library_metadata,
    StreamCallback stream_callback,
    bool verbose) {

    if (library_metadata.empty()) {
        std::cerr << "Error: No tracks in library" << std::endl;
        return std::nullopt;
    }

    spdlog::info("Claude Backend: Generating playlist for prompt: '{}'", user_prompt);
    spdlog::info("Using tool-enabled search across {} tracks", library_metadata.size());

    // Create library search engine for the full library
    LibrarySearch search_engine(library_metadata);

    // Create HTTPS client
    httplib::SSLClient client(API_ENDPOINT);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(90, 0);  // Longer timeout for tool use

    // Set headers
    httplib::Headers headers = {
        {"x-api-key", api_key_},
        {"anthropic-version", API_VERSION},
        {"content-type", "application/json"}
    };

    // Build initial prompt
    std::ostringstream initial_prompt;
    initial_prompt << "You are a music playlist curator with access to search tools for a music library of "
                   << library_metadata.size() << " tracks.\n\n"
                   << "User's request: \"" << user_prompt << "\"\n\n"
                   << "Use the provided search tools to find tracks that match the user's request. "
                   << "You can search by artist, genre, album, title, or year range. "
                   << "Start by using get_library_overview to understand what's available, "
                   << "then use specific searches to find matching tracks.\n\n"
                   << "Once you've found suitable tracks, respond with a JSON array of track indices (0-based) "
                   << "that best match the request. Select 10-50 tracks that fit the description.\n"
                   << "Example final response: [42, 156, 892, 1043, ...]";

    // Initialize conversation
    json::array_t messages;
    messages.push_back({
        {"role", "user"},
        {"content", initial_prompt.str()}
    });

    spdlog::debug("Generating AI playlist using tool search...");

    // Tool use loop
    const int MAX_TURNS = 10;
    for (int turn = 0; turn < MAX_TURNS; turn++) {
        spdlog::debug("Tool use turn {}/{}", turn + 1, MAX_TURNS);

        // Build request with tools
        json request_body = {
            {"model", model_},
            {"max_tokens", 4096},
            {"messages", messages},
            {"tools", buildToolDefinitions()}
        };

        spdlog::debug("Sending request to Claude API");
        auto response = client.Post("/v1/messages", headers,
                                   request_body.dump(), "application/json");

        if (!response || response->status != 200) {
            if (response) {
                spdlog::error("Claude API returned status {}", response->status);
                std::cerr << "Error: Claude API returned status " << response->status << std::endl;
                if (response->status >= 400) {
                    spdlog::debug("Error response: {}", response->body);
                    std::cerr << "Response: " << response->body << std::endl;
                }
            } else {
                spdlog::error("Failed to connect to Claude API");
                std::cerr << "Error: Failed to connect to Claude API" << std::endl;
            }
            return std::nullopt;
        }

        // Parse response
        json response_json;
        try {
            response_json = json::parse(response->body);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse API response: {}", e.what());
            std::cerr << "Error parsing API response: " << e.what() << std::endl;
            return std::nullopt;
        }

        spdlog::debug("Response: {}", response_json.dump(2));

        // Check stop reason
        std::string stop_reason = response_json.value("stop_reason", "");

        // Add assistant's response to conversation
        if (response_json.contains("content")) {
            messages.push_back({
                {"role", "assistant"},
                {"content", response_json["content"]}
            });
        }

        // Handle tool use
        if (stop_reason == "tool_use") {
            spdlog::info("Claude is using tools to search the library...");

            // Process all tool use requests
            json::array_t tool_results;
            for (const auto& content_block : response_json["content"]) {
                if (content_block.value("type", "") == "tool_use") {
                    std::string tool_name = content_block["name"];
                    std::string tool_use_id = content_block["id"];
                    json tool_input = content_block["input"];

                    spdlog::info("Executing tool: {}", tool_name);

                    // Execute the tool
                    json result = executeToolCall(tool_name, tool_input, search_engine);

                    tool_results.push_back({
                        {"type", "tool_result"},
                        {"tool_use_id", tool_use_id},
                        {"content", result.dump()}
                    });
                }
            }

            // Add tool results to conversation
            messages.push_back({
                {"role", "user"},
                {"content", tool_results}
            });

            continue;  // Continue the loop

        } else if (stop_reason == "end_turn") {
            // Extract final answer
            for (const auto& content_block : response_json["content"]) {
                if (content_block.value("type", "") == "text") {
                    std::string text_content = content_block["text"];
                    spdlog::debug("Final response text: {}", text_content);

                    // Parse JSON array of indices
                    try {
                        size_t start = text_content.find('[');
                        size_t end = text_content.rfind(']');

                        if (start != std::string::npos && end != std::string::npos && start < end) {
                            std::string json_array_str = text_content.substr(start, end - start + 1);
                            json indices_array = json::parse(json_array_str);

                            if (indices_array.is_array() && !indices_array.empty()) {
                                std::vector<std::string> playlist;
                                for (const auto& idx : indices_array) {
                                    if (idx.is_number_integer()) {
                                        size_t track_idx = idx.get<size_t>();
                                        if (track_idx < library_metadata.size()) {
                                            playlist.push_back(std::to_string(track_idx));
                                        }
                                    }
                                }

                                if (!playlist.empty()) {
                                    spdlog::info("Successfully generated playlist with {} tracks", playlist.size());
                                    return playlist;
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to parse final response: {}", e.what());
                    }
                }
            }

            std::cerr << "Error: Could not parse playlist from response" << std::endl;
            return std::nullopt;

        } else {
            spdlog::error("Unexpected stop_reason: {}", stop_reason);
            std::cerr << "Error: Unexpected API response" << std::endl;
            return std::nullopt;
        }
    }

    spdlog::error("Exceeded maximum tool use turns");
    std::cerr << "Error: Tool search took too many turns" << std::endl;
    return std::nullopt;
}
