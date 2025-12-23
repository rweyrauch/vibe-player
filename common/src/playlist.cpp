/*
 * vibe-player
 * playlist.cpp
 */

#include "playlist.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;

Playlist::Playlist(const std::vector<TrackMetadata> &tracks)
    : tracks_(tracks), current_index_(0)
{
}

Playlist::Playlist(const std::vector<std::string> &paths, const std::string &base_path)
    : paths_(paths), base_path_(base_path), current_index_(0)
{
}

Playlist Playlist::fromTracks(const std::vector<TrackMetadata> &tracks)
{
    return Playlist(tracks);
}

std::string Playlist::resolvePath(const std::string &path) const
{
    namespace fs = std::filesystem;

    // Handle home directory expansion
    std::string resolved = path;
    if (path.starts_with("~/"))
    {
        const char *home = std::getenv("HOME");
        if (home)
        {
            resolved = std::string(home) + path.substr(1);
        }
    }

    // Check if absolute path
    fs::path p(resolved);
    if (p.is_absolute())
    {
        // Return canonical path if file exists, otherwise return as-is
        if (fs::exists(p))
        {
            return fs::canonical(p).string();
        }
        return resolved;
    }

    // Relative path - resolve against base_path_
    if (!base_path_.empty())
    {
        fs::path base(base_path_);
        fs::path full = base / p;
        if (fs::exists(full))
        {
            return fs::canonical(full).string();
        }
    }

    // Try current working directory as fallback
    if (fs::exists(p))
    {
        return fs::canonical(p).string();
    }

    // Return as-is if file doesn't exist (will fail later)
    return resolved;
}

std::optional<Playlist> Playlist::fromPaths(const std::vector<std::string> &paths, const std::string &base_path)
{
    if (paths.empty())
    {
        std::cerr << "Error: Path list is empty" << std::endl;
        return std::nullopt;
    }

    return Playlist(paths, base_path);
}

std::optional<Playlist> Playlist::fromTextFile(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open playlist file: " << filepath << std::endl;
        return std::nullopt;
    }

    std::vector<std::string> paths;
    std::string line;

    // Store base directory for relative path resolution
    namespace fs = std::filesystem;
    std::string base_dir = fs::path(filepath).parent_path().string();

    while (std::getline(file, line))
    {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        paths.push_back(line);
    }

    if (paths.empty())
    {
        std::cerr << "Error: Playlist contains no valid paths" << std::endl;
        return std::nullopt;
    }

    return fromPaths(paths, base_dir);
}

std::optional<Playlist> Playlist::fromJson(const std::string &json_content)
{
    try
    {
        json playlist_json = json::parse(json_content);

        // Validate version
        if (!playlist_json.contains("version"))
        {
            std::cerr << "Error: Playlist missing version field" << std::endl;
            return std::nullopt;
        }

        // Parse tracks array
        if (!playlist_json.contains("tracks") || !playlist_json["tracks"].is_array())
        {
            std::cerr << "Error: Playlist missing or invalid tracks array" << std::endl;
            return std::nullopt;
        }

        std::vector<TrackMetadata> tracks;
        for (const auto &track_json : playlist_json["tracks"])
        {
            auto track = TrackMetadata::fromJson(track_json);
            if (track)
            {
                tracks.push_back(*track);
            }
            else
            {
                std::cerr << "Warning: Skipping invalid track in playlist" << std::endl;
            }
        }

        if (tracks.empty())
        {
            std::cerr << "Error: Playlist contains no valid tracks" << std::endl;
            return std::nullopt;
        }

        return Playlist(tracks);
    }
    catch (const json::exception &e)
    {
        std::cerr << "Error parsing playlist JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<Playlist> Playlist::fromFile(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open playlist file: " << filepath << std::endl;
        return std::nullopt;
    }

    // Read first non-whitespace character to detect format
    char first_char = '\0';
    file >> std::ws;
    if (file.peek() != EOF)
    {
        first_char = file.peek();
    }
    file.close();

    // Auto-detect format
    if (first_char == '{' || first_char == '[')
    {
        // JSON format
        std::ifstream json_file(filepath);
        std::stringstream buffer;
        buffer << json_file.rdbuf();
        return fromJson(buffer.str());
    }
    else
    {
        // Text format
        return fromTextFile(filepath);
    }
}

std::string Playlist::toJson() const
{
    json playlist_json;
    playlist_json["version"] = version();

    json tracks_array = json::array();
    for (const auto &track : tracks_)
    {
        tracks_array.push_back(track.toJson());
    }
    playlist_json["tracks"] = tracks_array;

    return playlist_json.dump(2); // Pretty print with 2-space indent
}

std::string Playlist::toText() const
{
    std::ostringstream output;

    // If we have paths, use those (new format)
    if (!paths_.empty())
    {
        for (const auto &path : paths_)
        {
            // Resolve to absolute path
            output << resolvePath(path) << "\n";
        }
    }
    else if (!tracks_.empty())
    {
        // If we have tracks, extract paths (for backward compatibility)
        for (const auto &track : tracks_)
        {
            output << track.filepath << "\n";
        }
    }

    return output.str();
}

std::string Playlist::toM3u() const
{
    std::ostringstream output;

    // M3U header
    output << "#EXTM3U\n";

    // If we have tracks with metadata, use extended M3U format
    if (!tracks_.empty())
    {
        for (const auto &track : tracks_)
        {
            // Calculate duration in seconds
            int duration_seconds = static_cast<int>(track.duration_ms / 1000);

            // Build display name: "Artist - Title" or just title or filename
            std::string display_name;
            if (track.artist.has_value() && track.title.has_value())
            {
                display_name = track.artist.value() + " - " + track.title.value();
            }
            else if (track.title.has_value())
            {
                display_name = track.title.value();
            }
            else
            {
                display_name = track.filename;
            }

            // Write EXTINF line: #EXTINF:duration,display name
            output << "#EXTINF:" << duration_seconds << "," << display_name << "\n";

            // Write file path
            output << track.filepath << "\n";
        }
    }
    else if (!paths_.empty())
    {
        // If we only have paths (no metadata), write simple M3U format
        for (const auto &path : paths_)
        {
            // Write EXTINF with -1 (unknown duration)
            output << "#EXTINF:-1,\n";
            // Write resolved path
            output << resolvePath(path) << "\n";
        }
    }

    return output.str();
}

bool Playlist::saveToFile(const std::string &filepath, PlaylistFormat format) const
{
    std::ofstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file for writing: " << filepath << std::endl;
        return false;
    }

    if (format == PlaylistFormat::JSON)
    {
        file << toJson();
    }
    else if (format == PlaylistFormat::M3U)
    {
        file << toM3u();
    }
    else
    {
        file << toText();
    }
    return true;
}

const TrackMetadata &Playlist::current() const
{
    return tracks_[current_index_];
}

const std::string &Playlist::currentPath() const
{
    if (!paths_.empty())
    {
        return paths_[current_index_];
    }
    // Fallback to filepath from metadata
    static std::string empty_path;
    if (!tracks_.empty() && current_index_ < tracks_.size())
    {
        return tracks_[current_index_].filepath;
    }
    return empty_path;
}

bool Playlist::advance()
{
    size_t total_size = !paths_.empty() ? paths_.size() : tracks_.size();
    if (current_index_ < total_size - 1)
    {
        current_index_++;
        return true;
    }
    return false;
}

bool Playlist::previous()
{
    if (current_index_ > 0)
    {
        current_index_--;
        return true;
    }
    return false;
}

bool Playlist::hasPrevious() const
{
    return current_index_ > 0;
}

bool Playlist::hasNext() const
{
    size_t total_size = !paths_.empty() ? paths_.size() : tracks_.size();
    return current_index_ < total_size - 1;
}

size_t Playlist::size() const
{
    return !paths_.empty() ? paths_.size() : tracks_.size();
}

size_t Playlist::currentIndex() const
{
    return current_index_;
}

void Playlist::setIndex(size_t index)
{
    size_t total_size = !paths_.empty() ? paths_.size() : tracks_.size();
    if (index < total_size)
    {
        current_index_ = index;
    }
}

void Playlist::reset()
{
    current_index_ = 0;
}

void Playlist::extractAllMetadata()
{
    // If we already have tracks, nothing to do
    if (!tracks_.empty())
    {
        return;
    }

    // Extract metadata from all paths
    if (!paths_.empty())
    {
        tracks_.clear();
        tracks_.reserve(paths_.size());

        for (const auto &path : paths_)
        {
            std::string resolved_path = resolvePath(path);
            auto metadata = MetadataExtractor::extract(resolved_path, false);

            if (metadata)
            {
                tracks_.push_back(*metadata);
            }
            else
            {
                // Create minimal metadata with just filepath
                TrackMetadata minimal;
                minimal.filepath = resolved_path;

                namespace fs = std::filesystem;
                fs::path p(resolved_path);
                minimal.filename = p.filename().string();
                minimal.title = p.stem().string();
                minimal.duration_ms = 0;
                minimal.file_mtime = 0;

                tracks_.push_back(minimal);
            }
        }
    }
}
