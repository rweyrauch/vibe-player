/*
 * vibe-player
 * playlist.h
 */

#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "metadata.h"
#include <string>
#include <vector>
#include <optional>

enum class PlaylistFormat {
    TEXT,
    JSON,
    AUTO_DETECT
};

class Playlist {
public:
    // Construction
    static std::optional<Playlist> fromJson(const std::string& json_content);
    static std::optional<Playlist> fromFile(const std::string& filepath);
    static std::optional<Playlist> fromTextFile(const std::string& filepath);
    static std::optional<Playlist> fromPaths(const std::vector<std::string>& paths, const std::string& base_path = "");
    static Playlist fromTracks(const std::vector<TrackMetadata>& tracks);

    // Serialization
    std::string toJson() const;
    std::string toText() const;
    bool saveToFile(const std::string& filepath, PlaylistFormat format = PlaylistFormat::TEXT) const;

    // Navigation
    const TrackMetadata& current() const;
    const std::string& currentPath() const;
    bool advance();
    bool previous();
    bool hasPrevious() const;
    bool hasNext() const;
    size_t size() const;
    size_t currentIndex() const;
    void setIndex(size_t index);
    void reset();  // Reset to beginning

    // Access
    const std::vector<TrackMetadata>& tracks() const { return tracks_; }
    const std::vector<std::string>& paths() const { return paths_; }

    // Metadata extraction
    void extractAllMetadata();

    // Metadata
    std::string version() const { return "1.0"; }
    bool empty() const { return tracks_.empty() && paths_.empty(); }

private:
    Playlist(const std::vector<TrackMetadata>& tracks);
    Playlist(const std::vector<std::string>& paths, const std::string& base_path);

    std::string resolvePath(const std::string& path) const;

    std::vector<TrackMetadata> tracks_;
    std::vector<std::string> paths_;
    std::string base_path_;
    size_t current_index_;
};

#endif // PLAYLIST_H
