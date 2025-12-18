#include "ai_playlist.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <numeric>

using json = nlohmann::json;

AIPlaylistGenerator::AIPlaylistGenerator(const std::string& api_key)
    : api_key_(api_key) {
}

std::string AIPlaylistGenerator::buildPrompt(
    const std::string& user_prompt,
    const std::vector<TrackMetadata>& library_metadata,
    std::vector<size_t>& sampled_indices) {

    std::ostringstream prompt;

    prompt << "You are a music playlist curator. Based on the user's request, "
           << "select songs from the provided library that best match their description.\n\n";

    prompt << "User's request: \"" << user_prompt << "\"\n\n";

    // Limit tracks to avoid rate limits (max ~1500 tracks for ~40k tokens)
    constexpr size_t MAX_TRACKS_FOR_PROMPT = 1500;
    sampled_indices.clear();

    if (library_metadata.size() <= MAX_TRACKS_FOR_PROMPT) {
        // Use all tracks if library is small enough
        for (size_t i = 0; i < library_metadata.size(); i++) {
            sampled_indices.push_back(i);
        }
    } else {
        // Randomly sample tracks if library is too large
        std::vector<size_t> all_indices(library_metadata.size());
        std::iota(all_indices.begin(), all_indices.end(), 0);
        std::random_shuffle(all_indices.begin(), all_indices.end());
        sampled_indices.assign(all_indices.begin(), all_indices.begin() + MAX_TRACKS_FOR_PROMPT);
        std::sort(sampled_indices.begin(), sampled_indices.end());

        prompt << "Note: Your library has " << library_metadata.size()
               << " tracks. Showing a random sample of " << MAX_TRACKS_FOR_PROMPT << ".\n\n";
    }

    prompt << "Available songs in library:\n";

    for (size_t i = 0; i < sampled_indices.size(); i++) {
        const auto& track = library_metadata[sampled_indices[i]];

        prompt << (i + 1) << ". ";

        if (track.title) {
            prompt << *track.title;
        } else {
            prompt << track.filename;
        }

        if (track.artist) {
            prompt << " - " << *track.artist;
        }

        if (track.album) {
            prompt << " (" << *track.album << ")";
        }

        if (track.genre) {
            prompt << " [" << *track.genre << "]";
        }

        if (track.year) {
            prompt << " {" << *track.year << "}";
        }

        prompt << "\n";
    }

    prompt << "\nRespond with ONLY a JSON array of song numbers that match "
           << "the user's request. Select 10-30 songs that best fit the description. "
           << "Example response: [1, 5, 12, 23, 45]\n";

    return prompt.str();
}

std::vector<std::string> AIPlaylistGenerator::parseResponse(
    const std::string& response_json,
    const std::vector<size_t>& sampled_indices) {
    try {
        json response = json::parse(response_json);

        // Extract content text from Claude's response
        if (!response.contains("content") || !response["content"].is_array() ||
            response["content"].empty()) {
            std::cerr << "Error: Invalid response format from Claude API" << std::endl;
            return {};
        }

        std::string content_text = response["content"][0]["text"];

        // Find JSON array in the response (it might have other text around it)
        size_t start = content_text.find('[');
        size_t end = content_text.rfind(']');

        if (start == std::string::npos || end == std::string::npos || start >= end) {
            std::cerr << "Error: Could not find JSON array in response" << std::endl;
            std::cerr << "Response: " << content_text << std::endl;
            return {};
        }

        std::string json_array_str = content_text.substr(start, end - start + 1);

        // Parse the JSON array of song indices
        json song_indices = json::parse(json_array_str);

        if (!song_indices.is_array()) {
            std::cerr << "Error: Response is not a JSON array" << std::endl;
            return {};
        }

        std::vector<std::string> playlist;
        for (const auto& idx : song_indices) {
            if (idx.is_number_integer()) {
                // Convert 1-based index to 0-based index into sampled list
                int sampled_idx = idx.get<int>() - 1;
                if (sampled_idx >= 0 && sampled_idx < static_cast<int>(sampled_indices.size())) {
                    // Map back to original library index
                    size_t original_idx = sampled_indices[sampled_idx];
                    playlist.push_back(std::to_string(original_idx));
                }
            }
        }

        return playlist;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing Claude API response: " << e.what() << std::endl;
        return {};
    }
}

std::optional<std::vector<std::string>> AIPlaylistGenerator::generate(
    const std::string& user_prompt,
    const std::vector<TrackMetadata>& library_metadata,
    int max_tracks) {

    if (library_metadata.empty()) {
        std::cerr << "Error: No tracks in library" << std::endl;
        return std::nullopt;
    }

    // Build prompt and get sampled indices
    std::vector<size_t> sampled_indices;
    std::string prompt = buildPrompt(user_prompt, library_metadata, sampled_indices);

    // Create HTTPS client
    httplib::SSLClient client(API_ENDPOINT);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(60, 0);

    // Build request body
    json request_body = {
        {"model", MODEL},
        {"max_tokens", 1024},
        {"messages", json::array({
            {
                {"role", "user"},
                {"content", prompt}
            }
        })}
    };

    // Set headers
    httplib::Headers headers = {
        {"x-api-key", api_key_},
        {"anthropic-version", "2023-06-01"},
        {"content-type", "application/json"}
    };

    // Make request with retry
    std::cout << "Generating AI playlist..." << std::endl;

    int retries = 1;
    for (int attempt = 0; attempt <= retries; attempt++) {
        auto response = client.Post("/v1/messages", headers,
                                   request_body.dump(), "application/json");

        if (response && response->status == 200) {
            auto playlist = parseResponse(response->body, sampled_indices);
            if (!playlist.empty()) {
                return playlist;
            } else {
                std::cerr << "Error: Claude API returned empty playlist" << std::endl;
                return std::nullopt;
            }
        }

        if (response) {
            std::cerr << "Error: Claude API returned status " << response->status << std::endl;
            if (response->status >= 400) {
                std::cerr << "Response: " << response->body << std::endl;
            }
        } else {
            std::cerr << "Error: Failed to connect to Claude API" << std::endl;
        }

        if (attempt < retries) {
            std::cout << "Retrying API request..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    std::cerr << "Error: Failed to generate playlist after " << (retries + 1)
              << " attempts" << std::endl;
    return std::nullopt;
}
