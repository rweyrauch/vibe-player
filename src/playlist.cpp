#include "playlist.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

Playlist::Playlist(const std::vector<TrackMetadata>& tracks)
    : tracks_(tracks), current_index_(0) {
}

Playlist Playlist::fromTracks(const std::vector<TrackMetadata>& tracks) {
    return Playlist(tracks);
}

std::optional<Playlist> Playlist::fromJson(const std::string& json_content) {
    try {
        json playlist_json = json::parse(json_content);

        // Validate version
        if (!playlist_json.contains("version")) {
            std::cerr << "Error: Playlist missing version field" << std::endl;
            return std::nullopt;
        }

        // Parse tracks array
        if (!playlist_json.contains("tracks") || !playlist_json["tracks"].is_array()) {
            std::cerr << "Error: Playlist missing or invalid tracks array" << std::endl;
            return std::nullopt;
        }

        std::vector<TrackMetadata> tracks;
        for (const auto& track_json : playlist_json["tracks"]) {
            auto track = TrackMetadata::fromJson(track_json);
            if (track) {
                tracks.push_back(*track);
            } else {
                std::cerr << "Warning: Skipping invalid track in playlist" << std::endl;
            }
        }

        if (tracks.empty()) {
            std::cerr << "Error: Playlist contains no valid tracks" << std::endl;
            return std::nullopt;
        }

        return Playlist(tracks);

    } catch (const json::exception& e) {
        std::cerr << "Error parsing playlist JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<Playlist> Playlist::fromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open playlist file: " << filepath << std::endl;
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromJson(buffer.str());
}

std::string Playlist::toJson() const {
    json playlist_json;
    playlist_json["version"] = version();

    json tracks_array = json::array();
    for (const auto& track : tracks_) {
        tracks_array.push_back(track.toJson());
    }
    playlist_json["tracks"] = tracks_array;

    return playlist_json.dump(2);  // Pretty print with 2-space indent
}

bool Playlist::saveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filepath << std::endl;
        return false;
    }

    file << toJson();
    return true;
}

const TrackMetadata& Playlist::current() const {
    return tracks_[current_index_];
}

bool Playlist::advance() {
    if (current_index_ < tracks_.size() - 1) {
        current_index_++;
        return true;
    }
    return false;
}

bool Playlist::previous() {
    if (current_index_ > 0) {
        current_index_--;
        return true;
    }
    return false;
}

bool Playlist::hasPrevious() const {
    return current_index_ > 0;
}

bool Playlist::hasNext() const {
    return current_index_ < tracks_.size() - 1;
}

size_t Playlist::size() const {
    return tracks_.size();
}

size_t Playlist::currentIndex() const {
    return current_index_;
}

void Playlist::setIndex(size_t index) {
    if (index < tracks_.size()) {
        current_index_ = index;
    }
}

void Playlist::reset() {
    current_index_ = 0;
}
