#ifndef METADATA_CACHE_H
#define METADATA_CACHE_H

#include "metadata.h"
#include <string>
#include <vector>
#include <optional>

class MetadataCache {
public:
    MetadataCache(const std::string& cache_dir = "");

    // Load cached metadata for a library path
    std::optional<std::vector<TrackMetadata>> load(const std::string& library_path);

    // Save metadata to cache
    bool save(const std::string& library_path, const std::vector<TrackMetadata>& tracks);

    // Check if cache is valid (files haven't changed)
    bool isValid(const std::string& library_path,
                 const std::vector<TrackMetadata>& cached_tracks);

    // Clear cache for a library path
    void clear(const std::string& library_path);

private:
    std::string cache_dir_;
    std::string getCachePath(const std::string& library_path) const;
    void ensureCacheDirectoryExists();
    std::string hashLibraryPath(const std::string& library_path) const;
};

#endif // METADATA_CACHE_H
