#include "metadata_cache.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

MetadataCache::MetadataCache(const std::string &cache_dir)
    : cache_dir_(cache_dir.empty() ? std::string(getenv("HOME")) + "/.cache/vibe-player" : cache_dir)
{
    ensureCacheDirectoryExists();
}

std::string MetadataCache::hashLibraryPath(const std::string &library_path) const
{
    // Simple hash function for library path (for cache filename)
    std::hash<std::string> hasher;
    size_t hash = hasher(std::filesystem::absolute(library_path).string());
    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

std::string MetadataCache::getCachePath(const std::string &library_path) const
{
    return cache_dir_ + "/metadata_" + hashLibraryPath(library_path) + ".json";
}

void MetadataCache::ensureCacheDirectoryExists()
{
    namespace fs = std::filesystem;
    try
    {
        if (!fs::exists(cache_dir_))
        {
            fs::create_directories(cache_dir_);
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Warning: Could not create cache directory: " << e.what() << std::endl;
    }
}

std::optional<std::vector<TrackMetadata>> MetadataCache::load(const std::string &library_path)
{
    namespace fs = std::filesystem;

    std::string cache_path = getCachePath(library_path);

    if (!fs::exists(cache_path))
    {
        return std::nullopt;
    }

    try
    {
        std::ifstream file(cache_path);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        json cache_json;
        file >> cache_json;

        // Validate version
        if (!cache_json.contains("version") || cache_json["version"] != 1)
        {
            std::cerr << "Warning: Cache version mismatch, ignoring cache" << std::endl;
            return std::nullopt;
        }

        // Validate library path matches
        if (!cache_json.contains("library_path") ||
            cache_json["library_path"] != fs::absolute(library_path).string())
        {
            return std::nullopt;
        }

        // Extract tracks
        std::vector<TrackMetadata> tracks;
        if (cache_json.contains("tracks"))
        {
            for (const auto &track_json : cache_json["tracks"])
            {
                auto track = TrackMetadata::fromJson(track_json);
                if (track)
                {
                    tracks.push_back(*track);
                }
            }
        }

        return tracks;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error reading cache: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool MetadataCache::save(const std::string &library_path, const std::vector<TrackMetadata> &tracks)
{
    namespace fs = std::filesystem;

    std::string cache_path = getCachePath(library_path);

    try
    {
        json cache_json;
        cache_json["version"] = 1;
        cache_json["library_path"] = fs::absolute(library_path).string();
        cache_json["last_scan"] = std::time(nullptr);

        json tracks_json = json::array();
        for (const auto &track : tracks)
        {
            tracks_json.push_back(track.toJson());
        }
        cache_json["tracks"] = tracks_json;

        std::ofstream file(cache_path);
        if (!file.is_open())
        {
            std::cerr << "Error: Could not write cache file: " << cache_path << std::endl;
            return false;
        }

        file << cache_json.dump(2);
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error writing cache: " << e.what() << std::endl;
        return false;
    }
}

bool MetadataCache::isValid(const std::string &library_path,
                            const std::vector<TrackMetadata> &cached_tracks)
{
    namespace fs = std::filesystem;

    // Quick check: verify a sample of files still exist with same mtime
    size_t sample_size = std::min(cached_tracks.size(), size_t(10));

    for (size_t i = 0; i < sample_size; i++)
    {
        size_t idx = (i * cached_tracks.size()) / sample_size;
        if (idx >= cached_tracks.size())
            continue;

        const auto &track = cached_tracks[idx];

        if (!fs::exists(track.filepath))
        {
            return false;
        }

        int64_t current_mtime = MetadataExtractor::getFileModificationTime(track.filepath);
        if (current_mtime != track.file_mtime)
        {
            return false;
        }
    }

    return true;
}

void MetadataCache::clear(const std::string &library_path)
{
    namespace fs = std::filesystem;

    std::string cache_path = getCachePath(library_path);

    try
    {
        if (fs::exists(cache_path))
        {
            fs::remove(cache_path);
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error clearing cache: " << e.what() << std::endl;
    }
}
