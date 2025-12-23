#ifndef LIBRARY_SEARCH_H
#define LIBRARY_SEARCH_H

#include "metadata.h"
#include <string>
#include <vector>
#include <optional>
#include <set>

// Result of a search operation
struct SearchResult
{
    std::vector<size_t> track_indices; // Indices into the original library
    size_t total_matches;              // Total number of matches
};

class LibrarySearch
{
public:
    explicit LibrarySearch(const std::vector<TrackMetadata> &library);

    // Search by artist name (case-insensitive partial match)
    SearchResult searchByArtist(const std::string &artist_query, size_t max_results = 100) const;

    // Search by genre (case-insensitive partial match)
    SearchResult searchByGenre(const std::string &genre_query, size_t max_results = 100) const;

    // Search by album (case-insensitive partial match)
    SearchResult searchByAlbum(const std::string &album_query, size_t max_results = 100) const;

    // Search by title (case-insensitive partial match)
    SearchResult searchByTitle(const std::string &title_query, size_t max_results = 100) const;

    // Search by year range (inclusive)
    SearchResult searchByYearRange(int start_year, int end_year, size_t max_results = 100) const;

    // Get all unique artists in the library
    std::vector<std::string> getUniqueArtists() const;

    // Get all unique genres in the library
    std::vector<std::string> getUniqueGenres() const;

    // Get all unique albums in the library
    std::vector<std::string> getUniqueAlbums() const;

    // Combine multiple search results (intersection)
    static SearchResult intersectResults(const SearchResult &a, const SearchResult &b);

    // Combine multiple search results (union)
    static SearchResult unionResults(const SearchResult &a, const SearchResult &b);

private:
    const std::vector<TrackMetadata> &library_;

    // Helper to convert string to lowercase
    static std::string toLower(const std::string &str);

    // Helper to check if haystack contains needle (case-insensitive)
    static bool containsIgnoreCase(const std::string &haystack, const std::string &needle);
};

#endif // LIBRARY_SEARCH_H
