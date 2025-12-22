#include "library_search.h"
#include <algorithm>
#include <cctype>
#include <set>

LibrarySearch::LibrarySearch(const std::vector<TrackMetadata>& library)
    : library_(library) {
}

std::string LibrarySearch::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool LibrarySearch::containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    std::string hay_lower = toLower(haystack);
    std::string needle_lower = toLower(needle);
    return hay_lower.find(needle_lower) != std::string::npos;
}

SearchResult LibrarySearch::searchByArtist(const std::string& artist_query, size_t max_results) const {
    SearchResult result;
    result.total_matches = 0;

    for (size_t i = 0; i < library_.size() && result.track_indices.size() < max_results; i++) {
        const auto& track = library_[i];
        if (track.artist && containsIgnoreCase(*track.artist, artist_query)) {
            result.track_indices.push_back(i);
            result.total_matches++;
        }
    }

    // Count remaining matches if we hit the limit
    if (result.track_indices.size() >= max_results) {
        for (size_t i = result.track_indices.size(); i < library_.size(); i++) {
            const auto& track = library_[i];
            if (track.artist && containsIgnoreCase(*track.artist, artist_query)) {
                result.total_matches++;
            }
        }
    }

    return result;
}

SearchResult LibrarySearch::searchByGenre(const std::string& genre_query, size_t max_results) const {
    SearchResult result;
    result.total_matches = 0;

    for (size_t i = 0; i < library_.size() && result.track_indices.size() < max_results; i++) {
        const auto& track = library_[i];
        if (track.genre && containsIgnoreCase(*track.genre, genre_query)) {
            result.track_indices.push_back(i);
            result.total_matches++;
        }
    }

    if (result.track_indices.size() >= max_results) {
        for (size_t i = result.track_indices.size(); i < library_.size(); i++) {
            const auto& track = library_[i];
            if (track.genre && containsIgnoreCase(*track.genre, genre_query)) {
                result.total_matches++;
            }
        }
    }

    return result;
}

SearchResult LibrarySearch::searchByAlbum(const std::string& album_query, size_t max_results) const {
    SearchResult result;
    result.total_matches = 0;

    for (size_t i = 0; i < library_.size() && result.track_indices.size() < max_results; i++) {
        const auto& track = library_[i];
        if (track.album && containsIgnoreCase(*track.album, album_query)) {
            result.track_indices.push_back(i);
            result.total_matches++;
        }
    }

    if (result.track_indices.size() >= max_results) {
        for (size_t i = result.track_indices.size(); i < library_.size(); i++) {
            const auto& track = library_[i];
            if (track.album && containsIgnoreCase(*track.album, album_query)) {
                result.total_matches++;
            }
        }
    }

    return result;
}

SearchResult LibrarySearch::searchByTitle(const std::string& title_query, size_t max_results) const {
    SearchResult result;
    result.total_matches = 0;

    for (size_t i = 0; i < library_.size() && result.track_indices.size() < max_results; i++) {
        const auto& track = library_[i];
        if (track.title && containsIgnoreCase(*track.title, title_query)) {
            result.track_indices.push_back(i);
            result.total_matches++;
        }
    }

    if (result.track_indices.size() >= max_results) {
        for (size_t i = result.track_indices.size(); i < library_.size(); i++) {
            const auto& track = library_[i];
            if (track.title && containsIgnoreCase(*track.title, title_query)) {
                result.total_matches++;
            }
        }
    }

    return result;
}

SearchResult LibrarySearch::searchByYearRange(int start_year, int end_year, size_t max_results) const {
    SearchResult result;
    result.total_matches = 0;

    for (size_t i = 0; i < library_.size() && result.track_indices.size() < max_results; i++) {
        const auto& track = library_[i];
        if (track.year && *track.year >= start_year && *track.year <= end_year) {
            result.track_indices.push_back(i);
            result.total_matches++;
        }
    }

    if (result.track_indices.size() >= max_results) {
        for (size_t i = result.track_indices.size(); i < library_.size(); i++) {
            const auto& track = library_[i];
            if (track.year && *track.year >= start_year && *track.year <= end_year) {
                result.total_matches++;
            }
        }
    }

    return result;
}

std::vector<std::string> LibrarySearch::getUniqueArtists() const {
    std::set<std::string> unique;
    for (const auto& track : library_) {
        if (track.artist) {
            unique.insert(*track.artist);
        }
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

std::vector<std::string> LibrarySearch::getUniqueGenres() const {
    std::set<std::string> unique;
    for (const auto& track : library_) {
        if (track.genre) {
            unique.insert(*track.genre);
        }
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

std::vector<std::string> LibrarySearch::getUniqueAlbums() const {
    std::set<std::string> unique;
    for (const auto& track : library_) {
        if (track.album) {
            unique.insert(*track.album);
        }
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

SearchResult LibrarySearch::intersectResults(const SearchResult& a, const SearchResult& b) {
    SearchResult result;
    std::set<size_t> b_set(b.track_indices.begin(), b.track_indices.end());

    for (size_t idx : a.track_indices) {
        if (b_set.count(idx)) {
            result.track_indices.push_back(idx);
        }
    }

    result.total_matches = result.track_indices.size();
    return result;
}

SearchResult LibrarySearch::unionResults(const SearchResult& a, const SearchResult& b) {
    SearchResult result;
    std::set<size_t> combined(a.track_indices.begin(), a.track_indices.end());
    combined.insert(b.track_indices.begin(), b.track_indices.end());

    result.track_indices.assign(combined.begin(), combined.end());
    result.total_matches = result.track_indices.size();
    return result;
}
