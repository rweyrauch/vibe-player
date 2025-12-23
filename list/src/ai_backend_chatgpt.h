/*
 * vibe-player
 * ai_backend_chatgpt.h
 */

#ifndef AI_BACKEND_CHATGPT_H
#define AI_BACKEND_CHATGPT_H

#include "ai_backend.h"
#include "library_search.h"
#include <string>
#include <nlohmann/json.hpp>

// Model presets for easy selection
enum class ChatGPTModel
{
    FAST,     // GPT-4o Mini - Fastest and cheapest
    BALANCED, // GPT-4o - Good balance of speed and quality
    BEST      // GPT-4 - Highest quality
};

class ChatGPTBackend : public AIBackend
{
public:
    explicit ChatGPTBackend(const std::string &api_key, ChatGPTModel model = ChatGPTModel::FAST);
    explicit ChatGPTBackend(const std::string &api_key, const std::string &model_id);

    std::optional<std::vector<std::string>> generate(
        const std::string &user_prompt,
        const std::vector<TrackMetadata> &library_metadata,
        StreamCallback stream_callback = nullptr,
        bool verbose = false) override;

    std::string name() const override { return "ChatGPT API (" + model_ + ")"; }
    bool validate(std::string &error_message) const override;

    // Helper to convert model enum to model ID string
    static std::string getModelId(ChatGPTModel model);

    // Helper to parse model string to enum
    static ChatGPTModel parseModelPreset(const std::string &preset);

private:
    // Tool definitions for function calling
    nlohmann::json buildToolDefinitions() const;

    // Execute a tool call
    nlohmann::json executeToolCall(
        const std::string &function_name,
        const nlohmann::json &arguments,
        const LibrarySearch &search_engine) const;

    std::string api_key_;
    std::string model_;
    static constexpr const char *API_ENDPOINT = "api.openai.com";
};

#endif // AI_BACKEND_CHATGPT_H
