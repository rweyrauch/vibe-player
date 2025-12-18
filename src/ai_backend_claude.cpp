#include "ai_backend_claude.h"
#include "ai_prompt_builder.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>
#include <chrono>

using json = nlohmann::json;

ClaudeBackend::ClaudeBackend(const std::string& api_key)
    : api_key_(api_key) {
}

bool ClaudeBackend::validate(std::string& error_message) const {
    if (api_key_.empty()) {
        error_message = "ANTHROPIC_API_KEY not set. Get a key from https://console.anthropic.com";
        return false;
    }
    return true;
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

    // Build prompt and get sampled indices
    std::vector<size_t> sampled_indices;
    PromptConfig config;
    config.max_tracks_in_prompt = 1500;  // Claude can handle larger context
    std::string prompt = AIPromptBuilder::buildPrompt(
        user_prompt, library_metadata, sampled_indices, config);

    // Log debug information
    spdlog::info("Claude Backend: Generating playlist for prompt: '{}'", user_prompt);
    spdlog::debug("Sampled {} tracks from {} total tracks", sampled_indices.size(), library_metadata.size());
    spdlog::debug("AI Prompt:\n{}", prompt);

    // Create HTTPS client
    httplib::SSLClient client(API_ENDPOINT);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(60, 0);

    // Build request body
    json request_body = {
        {"model", DEFAULT_MODEL},
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
        {"anthropic-version", API_VERSION},
        {"content-type", "application/json"}
    };

    // Make request with retry
    std::cout << "Generating AI playlist..." << std::endl;
    spdlog::debug("Sending request to Claude API endpoint: {}", API_ENDPOINT);

    int retries = 1;
    for (int attempt = 0; attempt <= retries; attempt++) {
        if (attempt > 0) {
            spdlog::info("Retrying Claude API request (attempt {}/{})", attempt + 1, retries + 1);
        }

        auto response = client.Post("/v1/messages", headers,
                                   request_body.dump(), "application/json");

        if (response && response->status == 200) {
            spdlog::info("Received successful response from Claude API");
            spdlog::debug("Response body length: {} bytes", response->body.size());

            // Parse the Claude API response
            try {
                json response_json = json::parse(response->body);

                // Extract content text from Claude's response
                if (!response_json.contains("content") || !response_json["content"].is_array() ||
                    response_json["content"].empty()) {
                    spdlog::error("Invalid response format from Claude API");
                    std::cerr << "Error: Invalid response format from Claude API" << std::endl;
                    return std::nullopt;
                }

                std::string content_text = response_json["content"][0]["text"];
                spdlog::debug("Claude response text:\n{}", content_text);

                // Use AIPromptBuilder to parse the response
                auto playlist = AIPromptBuilder::parseJsonResponse(content_text, sampled_indices);

                if (!playlist.empty()) {
                    spdlog::info("Successfully generated playlist with {} tracks", playlist.size());
                    return playlist;
                } else {
                    spdlog::error("Claude API returned empty playlist");
                    std::cerr << "Error: Claude API returned empty playlist" << std::endl;
                    return std::nullopt;
                }

            } catch (const std::exception& e) {
                spdlog::error("Exception parsing Claude API response: {}", e.what());
                std::cerr << "Error parsing Claude API response: " << e.what() << std::endl;
                return std::nullopt;
            }
        }

        if (response) {
            spdlog::error("Claude API returned status {}", response->status);
            std::cerr << "Error: Claude API returned status " << response->status << std::endl;
            if (response->status >= 400) {
                spdlog::debug("Error response body: {}", response->body);
                std::cerr << "Response: " << response->body << std::endl;
            }
        } else {
            spdlog::error("Failed to connect to Claude API");
            std::cerr << "Error: Failed to connect to Claude API" << std::endl;
        }

        if (attempt < retries) {
            std::cout << "Retrying API request..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    spdlog::error("Failed to generate playlist after {} attempts", retries + 1);
    std::cerr << "Error: Failed to generate playlist after " << (retries + 1)
              << " attempts" << std::endl;
    return std::nullopt;
}
