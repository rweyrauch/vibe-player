#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "metadata.h"
#include <string>
#include <vector>
#include <optional>

class Playlist {
public:
    // Construction
    static std::optional<Playlist> fromJson(const std::string& json_content);
    static std::optional<Playlist> fromFile(const std::string& filepath);
    static Playlist fromTracks(const std::vector<TrackMetadata>& tracks);

    // Serialization
    std::string toJson() const;
    bool saveToFile(const std::string& filepath) const;

    // Navigation
    const TrackMetadata& current() const;
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

    // Metadata
    std::string version() const { return "1.0"; }
    bool empty() const { return tracks_.empty(); }

private:
    Playlist(const std::vector<TrackMetadata>& tracks);

    std::vector<TrackMetadata> tracks_;
    size_t current_index_;
};

#endif // PLAYLIST_H
