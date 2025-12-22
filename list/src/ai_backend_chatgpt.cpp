/*
 * vibe-player
 * ai_backend_chatgpt.cpp
 */

#include "ai_backend_chatgpt.h"
#include "ai_prompt_builder.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <algorithm>
#include <sstream>

using json = nlohmann::json;

ChatGPTBackend::ChatGPTBackend(const std::string& api_key, ChatGPTModel model)
    : api_key_(api_key), model_(getModelId(model)) {
}

ChatGPTBackend::ChatGPTBackend(const std::string& api_key, const std::string& model_id)
    : api_key_(api_key), model_(model_id) {
}

std::string ChatGPTBackend::getModelId(ChatGPTModel model) {
    switch (model) {
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

ChatGPTModel ChatGPTBackend::parseModelPreset(const std::string& preset) {
    std::string lower = preset;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "fast" || lower == "mini" || lower == "gpt-4o-mini") {
        return ChatGPTModel::FAST;
    } else if (lower == "balanced" || lower == "gpt-4o") {
        return ChatGPTModel::BALANCED;
    } else if (lower == "best" || lower == "gpt-4") {
        return ChatGPTModel::BEST;
    }

    // Default to fast
    return ChatGPTModel::FAST;
}

bool ChatGPTBackend::validate(std::string& error_message) const {
    if (api_key_.empty()) {
        error_message = "OPENAI_API_KEY not set. Get a key from https://platform.openai.com/api-keys";
        return false;
    }
    return true;
}

std::optional<std::vector<std::string>> ChatGPTBackend::generate(
    const std::string& user_prompt,
    const std::vector<TrackMetadata>& library_metadata,
    StreamCallback stream_callback,
    bool verbose) {

    if (library_metadata.empty()) {
        std::cerr << "Error: No tracks in library" << std::endl;
        return std::nullopt;
    }

    spdlog::info("ChatGPT Backend: Generating playlist for prompt: '{}'", user_prompt);
    spdlog::info("Library size: {} tracks", library_metadata.size());

    // Build prompt with sampled tracks
    std::vector<size_t> sampled_indices;
    PromptConfig config;
    config.max_tracks_in_prompt = 2000;  // ChatGPT can handle larger context

    std::string prompt = AIPromptBuilder::buildPrompt(
        user_prompt,
        library_metadata,
        sampled_indices,
        config
    );

    spdlog::debug("Prompt built with {} tracks", sampled_indices.size());

    // Create HTTPS client
    httplib::SSLClient client(API_ENDPOINT);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(90, 0);

    // Set headers
    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key_},
        {"Content-Type", "application/json"}
    };

    // Build request body
    json request_body = {
        {"model", model_},
        {"messages", json::array({
            {
                {"role", "user"},
                {"content", prompt}
            }
        })},
        {"temperature", 0.7},
        {"max_tokens", 1024}
    };

    spdlog::debug("Sending request to OpenAI API");
    auto response = client.Post("/v1/chat/completions", headers,
                               request_body.dump(), "application/json");

    if (!response || response->status != 200) {
        if (response) {
            spdlog::error("OpenAI API returned status {}", response->status);
            std::cerr << "Error: OpenAI API returned status " << response->status << std::endl;
            if (response->status >= 400) {
                spdlog::debug("Error response: {}", response->body);
                std::cerr << "Response: " << response->body << std::endl;
            }
        } else {
            spdlog::error("Failed to connect to OpenAI API");
            std::cerr << "Error: Failed to connect to OpenAI API" << std::endl;
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

    // Extract content from response
    if (!response_json.contains("choices") || response_json["choices"].empty()) {
        spdlog::error("No choices in API response");
        std::cerr << "Error: Invalid API response format" << std::endl;
        return std::nullopt;
    }

    std::string content = response_json["choices"][0]["message"]["content"];
    spdlog::debug("ChatGPT response: {}", content);

    // Stream the response if callback provided
    if (stream_callback) {
        stream_callback(content, false);
    }

    // Parse JSON array from response
    auto playlist = AIPromptBuilder::parseJsonResponse(content, sampled_indices);

    if (playlist.empty()) {
        std::cerr << "Error: Could not parse playlist from response" << std::endl;
        return std::nullopt;
    }

    spdlog::info("Successfully generated playlist with {} tracks", playlist.size());
    return playlist;
}
