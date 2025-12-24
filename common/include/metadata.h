#ifndef METADATA_H
#define METADATA_H

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

struct TrackMetadata {
    std::string filepath;          // Full absolute path (or dropbox:// URL)
    std::string filename;          // Filename only (for display)
    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> album;
    std::optional<std::string> genre;
    std::optional<int> year;
    int64_t duration_ms;           // Duration in milliseconds
    int64_t file_mtime;            // Last modification time (for cache invalidation)
    std::optional<std::string> dropbox_hash;  // Dropbox content_hash for validation
    std::optional<std::string> dropbox_rev;   // Dropbox revision

    // Convert to JSON
    nlohmann::json toJson() const;

    // Parse from JSON
    static std::optional<TrackMetadata> fromJson(const nlohmann::json& json);
};

class MetadataExtractor {
public:
    // Extract metadata from a single audio file
    static std::optional<TrackMetadata> extract(const std::string& filepath, bool verbose = false);

    // Extract metadata from all files in a directory
    static std::vector<TrackMetadata> extractFromDirectory(
        const std::string& directory_path,
        bool recursive = true,
        bool verbose = false
    );

    // Extract metadata from Dropbox directory
    static std::vector<TrackMetadata> extractFromDropboxDirectory(
        const std::string& directory_path,
        bool recursive = true,
        bool verbose = false
    );

    // Get file modification time
    static int64_t getFileModificationTime(const std::string& filepath);
};

#endif // METADATA_H
