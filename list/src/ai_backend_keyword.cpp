/*
 * vibe-player
 * ai_backend_keyword.cpp
 */

#include "ai_backend_keyword.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
#include <map>

KeywordBackend::KeywordBackend()
{
}

bool KeywordBackend::validate(std::string &error_message) const
{
    // Keyword backend doesn't require any external dependencies
    return true;
}

std::string KeywordBackend::normalizeText(const std::string &text) const
{
    std::string result;
    result.reserve(text.length());

    for (char c : text)
    {
        if (std::isalnum(c) || std::isspace(c))
        {
            result += std::tolower(c);
        }
        else
        {
            result += ' ';
        }
    }

    return result;
}

std::set<std::string> KeywordBackend::extractKeywords(const std::string &text) const
{
    std::set<std::string> keywords;
    std::string normalized = normalizeText(text);
    std::istringstream stream(normalized);
    std::string word;

    // Common stop words to ignore
    static const std::set<std::string> stop_words = {
        "a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
        "has", "he", "in", "is", "it", "its", "of", "on", "that", "the",
        "to", "was", "will", "with", "songs", "music", "tracks", "playlist"};

    while (stream >> word)
    {
        if (word.length() >= 2 && stop_words.find(word) == stop_words.end())
        {
            keywords.insert(word);
        }
    }

    return keywords;
}

bool KeywordBackend::matchesYear(const std::string &keyword, const std::string &year) const
{
    if (year.empty())
        return false;

    // Direct year match
    if (keyword == year)
        return true;

    // Decade match (e.g., "80s" matches 1980-1989)
    if (keyword.length() == 3 && keyword[1] == '0' && keyword[2] == 's')
    {
        char decade = keyword[0];
        if (year.length() >= 3 && year[2] == decade)
        {
            return true;
        }
    }

    // Era keywords
    if (keyword == "recent" || keyword == "new" || keyword == "modern")
    {
        int y = std::stoi(year);
        return y >= 2015;
    }
    if (keyword == "classic" || keyword == "old" || keyword == "vintage")
    {
        int y = std::stoi(year);
        return y <= 1990;
    }

    return false;
}

double KeywordBackend::scoreTrack(
    const TrackMetadata &track,
    const std::set<std::string> &keywords,
    std::string &reason) const
{
    double score = 0.0;
    std::vector<std::string> matches;

    // Normalize metadata fields
    std::string artist = normalizeText(track.artist.value_or(""));
    std::string title = normalizeText(track.title.value_or(""));
    std::string album = normalizeText(track.album.value_or(""));
    std::string genre = normalizeText(track.genre.value_or(""));
    std::string year = track.year.has_value() ? std::to_string(track.year.value()) : "";

    for (const auto &keyword : keywords)
    {
        // Artist match (highest weight)
        if (artist.find(keyword) != std::string::npos)
        {
            score += 5.0;
            matches.push_back("artist:" + keyword);
        }

        // Genre match (high weight)
        if (genre.find(keyword) != std::string::npos)
        {
            score += 4.0;
            matches.push_back("genre:" + keyword);
        }

        // Album match (medium weight)
        if (album.find(keyword) != std::string::npos)
        {
            score += 2.0;
            matches.push_back("album:" + keyword);
        }

        // Title match (medium weight)
        if (title.find(keyword) != std::string::npos)
        {
            score += 2.0;
            matches.push_back("title:" + keyword);
        }

        // Year match (medium weight)
        if (matchesYear(keyword, year))
        {
            score += 3.0;
            matches.push_back("year:" + keyword);
        }
    }

    // Build reason string
    if (!matches.empty())
    {
        reason = "Matched: ";
        for (size_t i = 0; i < matches.size() && i < 3; ++i)
        {
            if (i > 0)
                reason += ", ";
            reason += matches[i];
        }
        if (matches.size() > 3)
        {
            reason += "...";
        }
    }

    return score;
}

std::optional<std::vector<std::string>> KeywordBackend::generate(
    const std::string &user_prompt,
    const std::vector<TrackMetadata> &library_metadata,
    StreamCallback stream_callback,
    bool verbose)
{
    if (library_metadata.empty())
    {
        std::cerr << "Error: No tracks in library" << std::endl;
        return std::nullopt;
    }

    spdlog::info("Keyword Backend: Generating playlist for prompt: '{}'", user_prompt);
    spdlog::info("Library size: {} tracks", library_metadata.size());

    // Extract keywords from prompt
    auto keywords = extractKeywords(user_prompt);

    if (keywords.empty())
    {
        std::cerr << "Error: No keywords found in prompt" << std::endl;
        return std::nullopt;
    }

    if (verbose)
    {
        std::cerr << "Extracted keywords: ";
        for (const auto &kw : keywords)
        {
            std::cerr << kw << " ";
        }
        std::cerr << std::endl;
    }

    spdlog::debug("Keywords: {}", [&keywords]()
                  {
        std::string s;
        for (const auto& kw : keywords) {
            if (!s.empty()) s += ", ";
            s += kw;
        }
        return s; }());

    // Score all tracks
    std::vector<TrackScore> scored_tracks;
    scored_tracks.reserve(library_metadata.size());

    for (size_t i = 0; i < library_metadata.size(); ++i)
    {
        std::string reason;
        double score = scoreTrack(library_metadata[i], keywords, reason);

        if (score > min_score_)
        {
            scored_tracks.push_back({i, score, reason});
        }
    }

    if (scored_tracks.empty())
    {
        std::cerr << "Error: No tracks matched the keywords" << std::endl;
        return std::nullopt;
    }

    // Sort by score (descending)
    std::sort(scored_tracks.begin(), scored_tracks.end(),
              [](const TrackScore &a, const TrackScore &b)
              {
                  return a.score > b.score;
              });

    // Limit to max_results
    if (scored_tracks.size() > max_results_)
    {
        scored_tracks.resize(max_results_);
    }

    if (verbose)
    {
        std::cerr << "\nTop matches:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(10), scored_tracks.size()); ++i)
        {
            const auto &ts = scored_tracks[i];
            const auto &track = library_metadata[ts.index];
            std::cerr << "  " << (i + 1) << ". "
                      << track.artist.value_or("Unknown") << " - "
                      << track.title.value_or("Unknown")
                      << " (score: " << ts.score << ") [" << ts.reason << "]" << std::endl;
        }
        std::cerr << std::endl;
    }

    // Convert to result format
    std::vector<std::string> result;
    result.reserve(scored_tracks.size());

    for (const auto &ts : scored_tracks)
    {
        result.push_back(std::to_string(ts.index));
    }

    spdlog::info("Successfully generated playlist with {} tracks", result.size());

    return result;
}
