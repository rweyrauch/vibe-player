#ifndef AI_BACKEND_CLAUDE_H
#define AI_BACKEND_CLAUDE_H

#include "ai_backend.h"

class ClaudeBackend : public AIBackend {
public:
    explicit ClaudeBackend(const std::string& api_key);

    std::optional<std::vector<std::string>> generate(
        const std::string& user_prompt,
        const std::vector<TrackMetadata>& library_metadata,
        StreamCallback stream_callback = nullptr
    ) override;

    std::string name() const override { return "Claude API"; }
    bool validate(std::string& error_message) const override;

private:
    std::string api_key_;
    static constexpr const char* API_ENDPOINT = "api.anthropic.com";
    static constexpr const char* DEFAULT_MODEL = "claude-3-haiku-20240307";
    static constexpr const char* API_VERSION = "2023-06-01";
};

#endif // AI_BACKEND_CLAUDE_H
