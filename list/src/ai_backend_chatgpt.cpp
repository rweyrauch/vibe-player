/*
 * vibe-player
 * ai_backend_chatgpt.cpp
 */

#include "ai_backend_chatgpt.h"
#include "ai_prompt_builder.h"
#include "library_search.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <algorithm>
#include <sstream>

using json = nlohmann::json;

ChatGPTBackend::ChatGPTBackend(const std::string &api_key, ChatGPTModel model)
    : api_key_(api_key), model_(getModelId(model))
{
}

ChatGPTBackend::ChatGPTBackend(const std::string &api_key, const std::string &model_id)
    : api_key_(api_key), model_(model_id)
{
}

std::string ChatGPTBackend::getModelId(ChatGPTModel model)
{
    switch (model)
    {
    case ChatGPTModel::FAST:
        return "gpt-4o-mini";
    case ChatGPTModel::BALANCED:
        return "gpt-4o";
    case ChatGPTModel::BEST:
        return "gpt-4";
    default:
        return "gpt-4o-mini";
    }
}

ChatGPTModel ChatGPTBackend::parseModelPreset(const std::string &preset)
{
    std::string lower = preset;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "fast" || lower == "mini" || lower == "gpt-4o-mini")
    {
        return ChatGPTModel::FAST;
    }
    else if (lower == "balanced" || lower == "gpt-4o")
    {
        return ChatGPTModel::BALANCED;
    }
    else if (lower == "best" || lower == "gpt-4")
    {
        return ChatGPTModel::BEST;
    }

    // Default to fast
    return ChatGPTModel::FAST;
}

bool ChatGPTBackend::validate(std::string &error_message) const
{
    if (api_key_.empty())
    {
        error_message = "OPENAI_API_KEY not set. Get a key from https://platform.openai.com/api-keys";
        return false;
    }
    return true;
}

// clang-format off
json ChatGPTBackend::buildToolDefinitions() const 
{
    return json::array({
        {
            {"type", "function"},
            {"function", {
                {"name", "search_by_artist"},
                {"description", "Search the music library for tracks by a specific artist. Use this to find all songs by an artist or band."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"artist_name", {
                            {"type", "string"},
                            {"description", "The name of the artist or band to search for (partial matches supported)"}
                        }},
                        {"max_results", {
                            {"type", "integer"},
                            {"description", "Maximum number of results to return"},
                            {"default", 100}
                        }}
                    }},
                    {"required", json::array({"artist_name"})}
                }}
            }}
        },
        {
            {"type", "function"},
            {"function", {
                {"name", "search_by_genre"},
                {"description", "Search the music library for tracks in a specific genre. Use this to find songs by musical style."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"genre", {
                            {"type", "string"},
                            {"description", "The genre to search for (e.g., 'rock', 'jazz', 'classical')"}
                        }},
                        {"max_results", {
                            {"type", "integer"},
                            {"description", "Maximum number of results to return"},
                            {"default", 100}
                        }}
                    }},
                    {"required", json::array({"genre"})}
                }}
            }}
        },
        {
            {"type", "function"},
            {"function", {
                {"name", "search_by_album"},
                {"description", "Search the music library for tracks from a specific album."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"album_name", {
                            {"type", "string"},
                            {"description", "The name of the album to search for (partial matches supported)"}
                        }},
                        {"max_results", {
                            {"type", "integer"},
                            {"description", "Maximum number of results to return"},
                            {"default", 100}
                        }}
                    }},
                    {"required", json::array({"album_name"})}
                }}
            }}
        },
        {
            {"type", "function"},
            {"function", {
                {"name", "search_by_title"},
                {"description", "Search the music library for tracks by song title or keywords in the title."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"title", {
                            {"type", "string"},
                            {"description", "The song title or keywords to search for (partial matches supported)"}
                        }},
                        {"max_results", {
                            {"type", "integer"},
                            {"description", "Maximum number of results to return"},
                            {"default", 100}
                        }}
                    }},
                    {"required", json::array({"title"})}
                }}
            }}
        },
        {
            {"type", "function"},
            {"function", {
                {"name", "search_by_year_range"},
                {"description", "Search the music library for tracks released within a specific year range."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"start_year", {
                            {"type", "integer"},
                            {"description", "The starting year (inclusive)"}
                        }},
                        {"end_year", {
                            {"type", "integer"},
                            {"description", "The ending year (inclusive)"}
                        }},
                        {"max_results", {
                            {"type", "integer"},
                            {"description", "Maximum number of results to return"},
                            {"default", 100}
                        }}
                    }},
                    {"required", json::array({"start_year", "end_year"})}
                }}
            }}
        },
        {
            {"type", "function"},
            {"function", {
                {"name", "get_library_overview"},
                {"description", "Get an overview of the music library including total tracks, unique artists, genres, and albums. Use this first to understand what's available."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", json::object()},
                    {"required", json::array()},
                    {"additionalProperties", false}
                }}
            }}
        }
    });
}
// clang-format on

json ChatGPTBackend::executeToolCall(
    const std::string &function_name,
    const json &arguments,
    const LibrarySearch &search_engine) const
{

    spdlog::debug("Executing function: {} with arguments: {}", function_name, arguments.dump());

    if (function_name == "search_by_artist")
    {
        std::string artist = arguments["artist_name"];
        int max_results = arguments.value("max_results", 100);

        auto result = search_engine.searchByArtist(artist, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}};
    }
    else if (function_name == "search_by_genre")
    {
        std::string genre = arguments["genre"];
        int max_results = arguments.value("max_results", 100);

        auto result = search_engine.searchByGenre(genre, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}};
    }
    else if (function_name == "search_by_album")
    {
        std::string album = arguments["album_name"];
        int max_results = arguments.value("max_results", 100);

        auto result = search_engine.searchByAlbum(album, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}};
    }
    else if (function_name == "search_by_title")
    {
        std::string title = arguments["title"];
        int max_results = arguments.value("max_results", 100);

        auto result = search_engine.searchByTitle(title, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}};
    }
    else if (function_name == "search_by_year_range")
    {
        int start_year = arguments["start_year"];
        int end_year = arguments["end_year"];
        int max_results = arguments.value("max_results", 100);

        auto result = search_engine.searchByYearRange(start_year, end_year, max_results);

        return {
            {"found", result.track_indices.size()},
            {"total_matches", result.total_matches},
            {"indices", result.track_indices}};
    }
    else if (function_name == "get_library_overview")
    {
        auto artists = search_engine.getUniqueArtists();
        auto genres = search_engine.getUniqueGenres();
        auto albums = search_engine.getUniqueAlbums();

        json sample_artists = json::array();
        for (size_t i = 0; i < std::min(size_t(20), artists.size()); i++)
        {
            sample_artists.push_back(artists[i]);
        }

        json sample_genres = json::array();
        for (size_t i = 0; i < std::min(size_t(20), genres.size()); i++)
        {
            sample_genres.push_back(genres[i]);
        }

        json result;
        result["total_tracks"] = search_engine.searchByYearRange(1900, 2100, 999999).total_matches;
        result["unique_artists"] = artists.size();
        result["unique_genres"] = genres.size();
        result["unique_albums"] = albums.size();
        result["sample_artists"] = sample_artists;
        result["sample_genres"] = sample_genres;

        return result;
    }

    return {{"error", "Unknown function: " + function_name}};
}

std::optional<std::vector<std::string>> ChatGPTBackend::generate(
    const std::string &user_prompt,
    const std::vector<TrackMetadata> &library_metadata,
    StreamCallback stream_callback,
    bool verbose)
{

    if (library_metadata.empty())
    {
        std::cerr << "Error: No tracks in library" << std::endl;
        spdlog::error("Error: No tracks in library");
        return std::nullopt;
    }

    spdlog::info("ChatGPT Backend: Generating playlist for prompt: '{}'", user_prompt);
    spdlog::info("Using tool-enabled search across {} tracks", library_metadata.size());

    // Create library search engine for the full library
    LibrarySearch search_engine(library_metadata);

    // Create HTTPS client
    httplib::SSLClient client(API_ENDPOINT);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(90, 0); // Longer timeout for function calling

    // Set headers
    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key_}};

    // Build initial prompt
    std::ostringstream initial_prompt;
    initial_prompt << "You are a music playlist curator with access to search functions for a music library of "
                   << library_metadata.size() << " tracks.\n\n"
                   << "User's request: \"" << user_prompt << "\"\n\n"
                   << "Use the provided search functions to find tracks that match the user's request. "
                   << "You can search by artist, genre, album, title, or year range. "
                   << "Start by using get_library_overview to understand what's available, "
                   << "then use specific searches to find matching tracks.\n\n"
                   << "Once you've found suitable tracks, respond with a JSON array of track indices (0-based) "
                   << "that best match the request. Select 10-50 tracks that fit the description.\n"
                   << "Example final response: [42, 156, 892, 1043, ...]";

    // Initialize conversation
    json::array_t messages;
    messages.push_back({{"role", "user"},
                        {"content", initial_prompt.str()}});

    spdlog::debug("Generating AI playlist using function calling...");

    // Function calling loop
    const int MAX_TURNS = 10;
    for (int turn = 0; turn < MAX_TURNS; turn++)
    {
        spdlog::debug("Function calling turn {}/{}", turn + 1, MAX_TURNS);

        // Build request with tools
        json request_body = {
            {"model", model_},
            {"messages", messages},
            {"tools", buildToolDefinitions()},
            {"tool_choice", "auto"}};

        spdlog::debug("Sending request to OpenAI API");
        auto response = client.Post("/v1/chat/completions", headers,
                                    request_body.dump(), "application/json");

        if (!response || response->status != 200)
        {
            if (response)
            {
                spdlog::error("OpenAI API returned status {}", response->status);
                std::cerr << "Error: OpenAI API returned status " << response->status << std::endl;
                if (response->status >= 400)
                {
                    spdlog::debug("Error response: {}", response->body);
                    std::cerr << "Response: " << response->body << std::endl;
                }
            }
            else
            {
                spdlog::error("Failed to connect to OpenAI API");
                std::cerr << "Error: Failed to connect to OpenAI API" << std::endl;
            }
            return std::nullopt;
        }

        // Parse response
        json response_json;
        try
        {
            response_json = json::parse(response->body);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to parse API response: {}", e.what());
            std::cerr << "Error parsing API response: " << e.what() << std::endl;
            return std::nullopt;
        }

        spdlog::debug("Response: {}", response_json.dump(2));

        // Get the message from the response
        if (!response_json.contains("choices") || response_json["choices"].empty())
        {
            spdlog::error("No choices in API response");
            std::cerr << "Error: Invalid API response format" << std::endl;
            return std::nullopt;
        }

        json message = response_json["choices"][0]["message"];

        // Add assistant's message to conversation
        messages.push_back(message);

        // Check if there are tool calls
        if (message.contains("tool_calls") && !message["tool_calls"].is_null())
        {
            spdlog::info("ChatGPT is using functions to search the library...");

            // Process all function calls
            for (const auto &tool_call : message["tool_calls"])
            {
                std::string function_name = tool_call["function"]["name"];
                std::string tool_call_id = tool_call["id"];

                json arguments;
                try
                {
                    arguments = json::parse(tool_call["function"]["arguments"].get<std::string>());
                }
                catch (const std::exception &e)
                {
                    spdlog::error("Failed to parse function arguments: {}", e.what());
                    continue;
                }

                spdlog::info("Executing function: {}", function_name);

                // Execute the function
                json result = executeToolCall(function_name, arguments, search_engine);

                // Add function result to conversation
                messages.push_back({{"role", "tool"},
                                    {"tool_call_id", tool_call_id},
                                    {"content", result.dump()}});
            }

            continue; // Continue the loop
        }
        else
        {
            // No tool calls - this should be the final answer
            if (message.contains("content") && !message["content"].is_null())
            {
                std::string content = message["content"];
                spdlog::debug("Final response text: {}", content);

                // Parse JSON array of indices
                try
                {
                    size_t start = content.find('[');
                    size_t end = content.rfind(']');

                    if (start != std::string::npos && end != std::string::npos && start < end)
                    {
                        std::string json_array_str = content.substr(start, end - start + 1);
                        json indices_array = json::parse(json_array_str);

                        if (indices_array.is_array() && !indices_array.empty())
                        {
                            std::vector<std::string> playlist;
                            for (const auto &idx : indices_array)
                            {
                                if (idx.is_number_integer())
                                {
                                    size_t track_idx = idx.get<size_t>();
                                    if (track_idx < library_metadata.size())
                                    {
                                        playlist.push_back(std::to_string(track_idx));
                                    }
                                }
                            }

                            if (!playlist.empty())
                            {
                                spdlog::info("Successfully generated playlist with {} tracks", playlist.size());
                                return playlist;
                            }
                        }
                    }
                }
                catch (const std::exception &e)
                {
                    spdlog::error("Failed to parse playlist indices: {}", e.what());
                }
            }

            spdlog::error("Could not extract playlist from final response");
            std::cerr << "Error: Could not parse playlist from response" << std::endl;
            return std::nullopt;
        }
    }

    spdlog::error("Exceeded maximum turns in function calling loop");
    std::cerr << "Error: ChatGPT exceeded maximum function calling turns" << std::endl;
    return std::nullopt;
}
