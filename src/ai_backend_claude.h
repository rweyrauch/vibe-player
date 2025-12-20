#ifndef AI_BACKEND_CLAUDE_H
#define AI_BACKEND_CLAUDE_H

#include "ai_backend.h"
#include <string>

// Model presets for easy selection
enum class ClaudeModel {
    FAST,        // Claude 3.5 Haiku - Fastest and cheapest
    BALANCED,    // Claude 3.5 Sonnet - Good balance of speed and quality
    BEST         // Claude Opus 4.5 - Highest quality
};

class ClaudeBackend : public AIBackend {
public:
    explicit ClaudeBackend(const std::string& api_key, ClaudeModel model = ClaudeModel::FAST);
    explicit ClaudeBackend(const std::string& api_key, const std::string& model_id);

    std::optional<std::vector<std::string>> generate(
        const std::string& user_prompt,
        const std::vector<TrackMetadata>& library_metadata,
        StreamCallback stream_callback = nullptr,
        bool verbose = false
    ) override;

    std::string name() const override { return "Claude API (" + model_ + ")"; }
    bool validate(std::string& error_message) const override;

    // Helper to convert model enum to model ID string
    static std::string getModelId(ClaudeModel model);

    // Helper to parse model string to enum
    static ClaudeModel parseModelPreset(const std::string& preset);

private:
    std::string api_key_;
    std::string model_;
    static constexpr const char* API_ENDPOINT = "api.anthropic.com";
    static constexpr const char* API_VERSION = "2023-06-01";
};

#endif // AI_BACKEND_CLAUDE_H
