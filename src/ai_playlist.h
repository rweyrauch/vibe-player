#ifndef AI_PLAYLIST_H
#define AI_PLAYLIST_H

#include "metadata.h"
#include <string>
#include <vector>
#include <optional>

class AIPlaylistGenerator {
public:
    explicit AIPlaylistGenerator(const std::string& api_key);

    // Generate playlist from natural language prompt
    std::optional<std::vector<std::string>> generate(
        const std::string& prompt,
        const std::vector<TrackMetadata>& library_metadata,
        int max_tracks = 50
    );

private:
    std::string api_key_;
    static constexpr const char* API_ENDPOINT = "api.anthropic.com";
    static constexpr const char* MODEL = "claude-3-haiku-20240307";

    std::string buildPrompt(const std::string& user_prompt,
                           const std::vector<TrackMetadata>& library_metadata,
                           std::vector<size_t>& sampled_indices);

    std::vector<std::string> parseResponse(const std::string& response_json,
                                           const std::vector<size_t>& sampled_indices);
};

#endif // AI_PLAYLIST_H
