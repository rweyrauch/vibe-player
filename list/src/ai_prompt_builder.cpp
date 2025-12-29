#include "ai_prompt_builder.h"

#include <algorithm>
#include <numeric>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>

using json = nlohmann::json;

std::string AIPromptBuilder::buildPrompt(
    const std::string &user_request,
    const std::vector<TrackMetadata> &library_metadata,
    std::vector<size_t> &sampled_indices_out,
    const PromptConfig &config)
{

    std::ostringstream prompt;

    prompt << "You are an expert music playlist curator. Your goal is to create a cohesive, "
           << "well-curated playlist based on the user's request.\n\n"
           << "User's request: \"" << user_request << "\"\n\n"
           << "CURATION PRINCIPLES:\n"
           << "- Create a cohesive listening experience, not just a list of matches\n"
           << "- Balance literal matches with thematic/vibe matches (e.g., 'upbeat' means energy, not just genre)\n"
           << "- Ensure diversity: avoid more than 3-4 consecutive tracks from the same artist or album\n"
           << "- Consider flow and pacing: vary energy levels, mix eras and styles thoughtfully\n"
           << "- For broad requests (e.g., 'rock'), sample across subgenres and decades\n"
           << "- For specific requests (e.g., 'Beatles'), include variety from their catalog\n"
           << "- Aim for 15-30 tracks depending on request specificity (narrow=fewer, broad=more)\n\n";

    // Sample tracks if library is too large
    sampled_indices_out.clear();

    if (library_metadata.size() <= config.max_tracks_in_prompt)
    {
        // Use all tracks if library is small enough
        for (size_t i = 0; i < library_metadata.size(); i++)
        {
            sampled_indices_out.push_back(i);
        }
    }
    else
    {
        // Randomly sample tracks if library is too large
        std::vector<size_t> all_indices(library_metadata.size());
        std::iota(all_indices.begin(), all_indices.end(), 0);
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(all_indices.begin(), all_indices.end(), g);
        sampled_indices_out.assign(all_indices.begin(),
                                   all_indices.begin() + config.max_tracks_in_prompt);
        std::sort(sampled_indices_out.begin(), sampled_indices_out.end());

        prompt << "Note: Your library has " << library_metadata.size()
               << " tracks. Showing a random sample of " << config.max_tracks_in_prompt << ".\n\n";
    }

    prompt << "Available songs in library:\n";

    for (size_t i = 0; i < sampled_indices_out.size(); i++)
    {
        const auto &track = library_metadata[sampled_indices_out[i]];

        prompt << (i + 1) << ". ";

        if (track.title)
        {
            prompt << *track.title;
        }
        else
        {
            prompt << track.filename;
        }

        if (config.include_artist && track.artist)
        {
            prompt << " - " << *track.artist;
        }

        if (config.include_album && track.album)
        {
            prompt << " (" << *track.album << ")";
        }

        if (config.include_genre && track.genre)
        {
            prompt << " [" << *track.genre << "]";
        }

        if (config.include_year && track.year)
        {
            prompt << " {" << *track.year << "}";
        }

        prompt << "\n";
    }

    prompt << "\nCarefully curate your selections following the principles above. "
           << "Respond with ONLY a JSON array of song numbers (from the numbered list) "
           << "that create the best playlist experience for this request.\n"
           << "Example response: [1, 5, 12, 23, 45]\n";

    return prompt.str();
}

std::vector<std::string> AIPromptBuilder::parseJsonResponse(
    const std::string &response_text,
    const std::vector<size_t> &sampled_indices)
{

    try
    {
        // Find JSON array in the response (it might have other text around it)
        size_t start = response_text.find('[');
        size_t end = response_text.rfind(']');

        if (start == std::string::npos || end == std::string::npos || start >= end)
        {
            std::cerr << "Error: Could not find JSON array in response" << std::endl;
            std::cerr << "Response: " << response_text << std::endl;
            return {};
        }

        std::string json_array_str = response_text.substr(start, end - start + 1);

        // Parse the JSON array of song indices
        json song_indices = json::parse(json_array_str);

        if (!song_indices.is_array())
        {
            std::cerr << "Error: Response is not a JSON array" << std::endl;
            return {};
        }

        std::vector<std::string> playlist;
        for (const auto &idx : song_indices)
        {
            if (idx.is_number_integer())
            {
                // Convert 1-based index to 0-based index into sampled list
                int sampled_idx = idx.get<int>() - 1;
                if (sampled_idx >= 0 && sampled_idx < static_cast<int>(sampled_indices.size()))
                {
                    // Map back to original library index
                    size_t original_idx = sampled_indices[sampled_idx];
                    playlist.push_back(std::to_string(original_idx));
                }
            }
        }

        return playlist;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing AI response: " << e.what() << std::endl;
        return {};
    }
}
