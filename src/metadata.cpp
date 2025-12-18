#include "metadata.h"

#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>

using json = nlohmann::json;

// Sanitize string to ensure valid UTF-8
// Removes invalid UTF-8 sequences and overlong encodings
static std::string sanitizeUtf8(const std::string& input) {
    std::string result;
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(input.data());
    size_t len = input.size();

    for (size_t i = 0; i < len; ) {
        unsigned char c = bytes[i];

        // ASCII (single byte: 0x00-0x7F)
        if (c <= 0x7F) {
            result += static_cast<char>(c);
            i++;
        }
        // 2-byte sequence (0xC2-0xDF)
        // Note: 0xC0 and 0xC1 are invalid (overlong encodings)
        else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 < len && (bytes[i+1] & 0xC0) == 0x80) {
                result += static_cast<char>(bytes[i]);
                result += static_cast<char>(bytes[i+1]);
                i += 2;
            } else {
                // Invalid sequence, skip
                i++;
            }
        }
        // 3-byte sequence (0xE0-0xEF)
        else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 < len &&
                (bytes[i+1] & 0xC0) == 0x80 &&
                (bytes[i+2] & 0xC0) == 0x80) {

                // Check for overlong encodings
                if (c == 0xE0 && bytes[i+1] < 0xA0) {
                    // Overlong encoding, skip
                    i++;
                } else {
                    result += static_cast<char>(bytes[i]);
                    result += static_cast<char>(bytes[i+1]);
                    result += static_cast<char>(bytes[i+2]);
                    i += 3;
                }
            } else {
                // Invalid sequence, skip
                i++;
            }
        }
        // 4-byte sequence (0xF0-0xF4)
        // Note: 0xF5-0xFF are invalid
        else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 < len &&
                (bytes[i+1] & 0xC0) == 0x80 &&
                (bytes[i+2] & 0xC0) == 0x80 &&
                (bytes[i+3] & 0xC0) == 0x80) {

                // Check for overlong encodings and out-of-range values
                if (c == 0xF0 && bytes[i+1] < 0x90) {
                    // Overlong encoding, skip
                    i++;
                } else if (c == 0xF4 && bytes[i+1] > 0x8F) {
                    // Out of valid Unicode range, skip
                    i++;
                } else {
                    result += static_cast<char>(bytes[i]);
                    result += static_cast<char>(bytes[i+1]);
                    result += static_cast<char>(bytes[i+2]);
                    result += static_cast<char>(bytes[i+3]);
                    i += 4;
                }
            } else {
                // Invalid sequence, skip
                i++;
            }
        }
        // Invalid UTF-8 start byte (0x80-0xBF, 0xC0-0xC1, 0xF5-0xFF)
        else {
            // Skip invalid byte
            i++;
        }
    }

    return result;
}

nlohmann::json TrackMetadata::toJson() const {
    json j;
    j["filepath"] = filepath;
    j["filename"] = filename;

    if (title) {
        j["title"] = *title;
    } else {
        j["title"] = nullptr;
    }

    if (artist) {
        j["artist"] = *artist;
    } else {
        j["artist"] = nullptr;
    }

    if (album) {
        j["album"] = *album;
    } else {
        j["album"] = nullptr;
    }

    if (genre) {
        j["genre"] = *genre;
    } else {
        j["genre"] = nullptr;
    }

    if (year) {
        j["year"] = *year;
    } else {
        j["year"] = nullptr;
    }

    j["duration_ms"] = duration_ms;
    j["file_mtime"] = file_mtime;
    return j;
}

std::optional<TrackMetadata> TrackMetadata::fromJson(const nlohmann::json& j) {
    try {
        TrackMetadata metadata;
        metadata.filepath = j.at("filepath").get<std::string>();
        metadata.filename = j.at("filename").get<std::string>();

        if (!j["title"].is_null()) {
            metadata.title = j["title"].get<std::string>();
        }
        if (!j["artist"].is_null()) {
            metadata.artist = j["artist"].get<std::string>();
        }
        if (!j["album"].is_null()) {
            metadata.album = j["album"].get<std::string>();
        }
        if (!j["genre"].is_null()) {
            metadata.genre = j["genre"].get<std::string>();
        }
        if (!j["year"].is_null()) {
            metadata.year = j["year"].get<int>();
        }

        metadata.duration_ms = j.at("duration_ms").get<int64_t>();
        metadata.file_mtime = j.at("file_mtime").get<int64_t>();

        return metadata;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing metadata JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<TrackMetadata> MetadataExtractor::extract(const std::string& filepath) {
    namespace fs = std::filesystem;

    // Check if file exists
    if (!fs::exists(filepath)) {
        return std::nullopt;
    }

    TagLib::FileRef file(filepath.c_str());

    TrackMetadata metadata;
    metadata.filepath = sanitizeUtf8(filepath);
    metadata.filename = sanitizeUtf8(fs::path(filepath).filename().string());
    metadata.file_mtime = getFileModificationTime(filepath);

    if (file.isNull()) {
        // File couldn't be opened, but use filename as fallback
        metadata.title = sanitizeUtf8(fs::path(filepath).stem().string());
        metadata.duration_ms = 0;
        return metadata;
    }

    // Extract tags
    if (file.tag()) {
        TagLib::Tag* tag = file.tag();

        auto titleStr = tag->title().toCString(true);
        if (titleStr && strlen(titleStr) > 0) {
            metadata.title = sanitizeUtf8(std::string(titleStr));
        }

        auto artistStr = tag->artist().toCString(true);
        if (artistStr && strlen(artistStr) > 0) {
            metadata.artist = sanitizeUtf8(std::string(artistStr));
        }

        auto albumStr = tag->album().toCString(true);
        if (albumStr && strlen(albumStr) > 0) {
            metadata.album = sanitizeUtf8(std::string(albumStr));
        }

        auto genreStr = tag->genre().toCString(true);
        if (genreStr && strlen(genreStr) > 0) {
            metadata.genre = sanitizeUtf8(std::string(genreStr));
        }

        if (tag->year() > 0) {
            metadata.year = tag->year();
        }
    }

    // Fallback: if no title found, use filename
    if (!metadata.title) {
        metadata.title = sanitizeUtf8(fs::path(filepath).stem().string());
    }

    // Get duration from audio properties
    if (file.audioProperties()) {
        metadata.duration_ms = file.audioProperties()->lengthInMilliseconds();
    } else {
        metadata.duration_ms = 0;
    }

    return metadata;
}

std::vector<TrackMetadata> MetadataExtractor::extractFromDirectory(
    const std::string& directory_path,
    bool recursive) {

    std::vector<TrackMetadata> results;
    const std::vector<std::string> valid_extensions = {".wav", ".mp3", ".flac", ".ogg"};

    namespace fs = std::filesystem;

    try {
        if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
            std::cerr << "Error: Directory does not exist: " << directory_path << std::endl;
            return results;
        }

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(directory_path)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (std::find(valid_extensions.begin(), valid_extensions.end(), ext)
                        != valid_extensions.end()) {

                        auto metadata = extract(entry.path().string());
                        if (metadata) {
                            results.push_back(*metadata);
                        }
                    }
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(directory_path)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (std::find(valid_extensions.begin(), valid_extensions.end(), ext)
                        != valid_extensions.end()) {

                        auto metadata = extract(entry.path().string());
                        if (metadata) {
                            results.push_back(*metadata);
                        }
                    }
                }
            }
        }

        // Sort by filepath
        std::sort(results.begin(), results.end(),
                  [](const TrackMetadata& a, const TrackMetadata& b) {
                      return a.filepath < b.filepath;
                  });

    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }

    return results;
}

int64_t MetadataExtractor::getFileModificationTime(const std::string& filepath) {
    struct stat st;
    if (stat(filepath.c_str(), &st) == 0) {
        return static_cast<int64_t>(st.st_mtime);
    }
    return 0;
}
