/*
 * vibe-player
 * ai_backend_keyword.h
 */

#ifndef AI_BACKEND_KEYWORD_H
#define AI_BACKEND_KEYWORD_H

#include "ai_backend.h"
#include <string>
#include <vector>
#include <set>

class KeywordBackend : public AIBackend
{
public:
    KeywordBackend();

    std::optional<std::vector<std::string>> generate(
        const std::string &user_prompt,
        const std::vector<TrackMetadata> &library_metadata,
        StreamCallback stream_callback = nullptr,
        bool verbose = false) override;

    std::string name() const override { return "Keyword Matching"; }
    bool validate(std::string &error_message) const override;

    // Configuration
    void setMaxResults(size_t max_results) { max_results_ = max_results; }
    void setMinScore(double min_score) { min_score_ = min_score; }

private:
    struct TrackScore
    {
        size_t index;
        double score;
        std::string reason;
    };

    // Extract keywords from prompt
    std::set<std::string> extractKeywords(const std::string &text) const;

    // Calculate match score for a track
    double scoreTrack(
        const TrackMetadata &track,
        const std::set<std::string> &keywords,
        std::string &reason) const;

    // Normalize text (lowercase, remove punctuation)
    std::string normalizeText(const std::string &text) const;

    // Check if a year keyword matches
    bool matchesYear(const std::string &keyword, const std::string &year) const;

    size_t max_results_ = 50; // Maximum number of tracks to return
    double min_score_ = 0.0;  // Minimum score threshold
};

#endif // AI_BACKEND_KEYWORD_H
