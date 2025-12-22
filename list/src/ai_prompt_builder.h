#ifndef AI_PROMPT_BUILDER_H
#define AI_PROMPT_BUILDER_H

#include "metadata.h"
#include <string>
#include <vector>

struct PromptConfig {
    size_t max_tracks_in_prompt = 1500;  // Adjust based on backend capabilities
    bool include_artist = true;
    bool include_album = true;
    bool include_genre = true;
    bool include_year = true;
};

class AIPromptBuilder {
public:
    // Build prompt and return sampled indices
    // For large libraries, randomly samples up to max_tracks_in_prompt tracks
    // sampled_indices_out will contain the indices of tracks included in the prompt
    static std::string buildPrompt(
        const std::string& user_request,
        const std::vector<TrackMetadata>& library_metadata,
        std::vector<size_t>& sampled_indices_out,
        const PromptConfig& config = PromptConfig{}
    );

    // Parse response looking for JSON array (e.g., [1, 5, 12, ...])
    // Maps 1-based indices from AI response to original library indices via sampled_indices
    // Returns vector of string indices for consistency with existing API
    static std::vector<std::string> parseJsonResponse(
        const std::string& response_text,
        const std::vector<size_t>& sampled_indices
    );
};

#endif // AI_PROMPT_BUILDER_H
