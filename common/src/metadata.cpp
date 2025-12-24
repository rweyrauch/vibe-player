#include "metadata.h"
#include "path_handler.h"
#include "dropbox_client.h"
#include "temp_file_manager.h"
#include "dropbox_state.h"

#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>

using json = nlohmann::json;

// Sanitize string to ensure valid UTF-8
// Removes invalid UTF-8 sequences and overlong encodings
static std::string sanitizeUtf8(const std::string &input)
{
    std::string result;
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(input.data());
    size_t len = input.size();

    for (size_t i = 0; i < len;)
    {
        unsigned char c = bytes[i];

        // ASCII (single byte: 0x00-0x7F)
        if (c <= 0x7F)
        {
            result += static_cast<char>(c);
            i++;
        }
        // 2-byte sequence (0xC2-0xDF)
        // Note: 0xC0 and 0xC1 are invalid (overlong encodings)
        else if (c >= 0xC2 && c <= 0xDF)
        {
            if (i + 1 < len && (bytes[i + 1] & 0xC0) == 0x80)
            {
                result += static_cast<char>(bytes[i]);
                result += static_cast<char>(bytes[i + 1]);
                i += 2;
            }
            else
            {
                // Invalid sequence, skip
                i++;
            }
        }
        // 3-byte sequence (0xE0-0xEF)
        else if (c >= 0xE0 && c <= 0xEF)
        {
            if (i + 2 < len &&
                (bytes[i + 1] & 0xC0) == 0x80 &&
                (bytes[i + 2] & 0xC0) == 0x80)
            {

                // Check for overlong encodings
                if (c == 0xE0 && bytes[i + 1] < 0xA0)
                {
                    // Overlong encoding, skip
                    i++;
                }
                else
                {
                    result += static_cast<char>(bytes[i]);
                    result += static_cast<char>(bytes[i + 1]);
                    result += static_cast<char>(bytes[i + 2]);
                    i += 3;
                }
            }
            else
            {
                // Invalid sequence, skip
                i++;
            }
        }
        // 4-byte sequence (0xF0-0xF4)
        // Note: 0xF5-0xFF are invalid
        else if (c >= 0xF0 && c <= 0xF4)
        {
            if (i + 3 < len &&
                (bytes[i + 1] & 0xC0) == 0x80 &&
                (bytes[i + 2] & 0xC0) == 0x80 &&
                (bytes[i + 3] & 0xC0) == 0x80)
            {

                // Check for overlong encodings and out-of-range values
                if (c == 0xF0 && bytes[i + 1] < 0x90)
                {
                    // Overlong encoding, skip
                    i++;
                }
                else if (c == 0xF4 && bytes[i + 1] > 0x8F)
                {
                    // Out of valid Unicode range, skip
                    i++;
                }
                else
                {
                    result += static_cast<char>(bytes[i]);
                    result += static_cast<char>(bytes[i + 1]);
                    result += static_cast<char>(bytes[i + 2]);
                    result += static_cast<char>(bytes[i + 3]);
                    i += 4;
                }
            }
            else
            {
                // Invalid sequence, skip
                i++;
            }
        }
        // Invalid UTF-8 start byte (0x80-0xBF, 0xC0-0xC1, 0xF5-0xFF)
        else
        {
            // Skip invalid byte
            i++;
        }
    }

    return result;
}

nlohmann::json TrackMetadata::toJson() const
{
    json j;
    j["filepath"] = filepath;
    j["filename"] = filename;

    if (title)
    {
        j["title"] = *title;
    }
    else
    {
        j["title"] = nullptr;
    }

    if (artist)
    {
        j["artist"] = *artist;
    }
    else
    {
        j["artist"] = nullptr;
    }

    if (album)
    {
        j["album"] = *album;
    }
    else
    {
        j["album"] = nullptr;
    }

    if (genre)
    {
        j["genre"] = *genre;
    }
    else
    {
        j["genre"] = nullptr;
    }

    if (year)
    {
        j["year"] = *year;
    }
    else
    {
        j["year"] = nullptr;
    }

    j["duration_ms"] = duration_ms;
    j["file_mtime"] = file_mtime;

    if (dropbox_hash)
    {
        j["dropbox_hash"] = *dropbox_hash;
    }
    else
    {
        j["dropbox_hash"] = nullptr;
    }

    if (dropbox_rev)
    {
        j["dropbox_rev"] = *dropbox_rev;
    }
    else
    {
        j["dropbox_rev"] = nullptr;
    }

    return j;
}

std::optional<TrackMetadata> TrackMetadata::fromJson(const nlohmann::json &j)
{
    try
    {
        TrackMetadata metadata;
        metadata.filepath = j.at("filepath").get<std::string>();
        metadata.filename = j.at("filename").get<std::string>();

        if (!j["title"].is_null())
        {
            metadata.title = j["title"].get<std::string>();
        }
        if (!j["artist"].is_null())
        {
            metadata.artist = j["artist"].get<std::string>();
        }
        if (!j["album"].is_null())
        {
            metadata.album = j["album"].get<std::string>();
        }
        if (!j["genre"].is_null())
        {
            metadata.genre = j["genre"].get<std::string>();
        }
        if (!j["year"].is_null())
        {
            metadata.year = j["year"].get<int>();
        }

        metadata.duration_ms = j.at("duration_ms").get<int64_t>();
        metadata.file_mtime = j.at("file_mtime").get<int64_t>();

        if (j.contains("dropbox_hash") && !j["dropbox_hash"].is_null())
        {
            metadata.dropbox_hash = j["dropbox_hash"].get<std::string>();
        }
        if (j.contains("dropbox_rev") && !j["dropbox_rev"].is_null())
        {
            metadata.dropbox_rev = j["dropbox_rev"].get<std::string>();
        }

        return metadata;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing metadata JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<TrackMetadata> MetadataExtractor::extract(const std::string &filepath, bool verbose)
{
    namespace fs = std::filesystem;

    // Check if file exists
    if (!fs::exists(filepath))
    {
        return std::nullopt;
    }

    // Sanitize filepath early to ensure all derived strings are clean
    std::string clean_filepath = sanitizeUtf8(filepath);

    TagLib::FileRef file(filepath.c_str());

    TrackMetadata metadata;
    metadata.filepath = clean_filepath;
    metadata.filename = sanitizeUtf8(fs::path(clean_filepath).filename().string());
    metadata.file_mtime = getFileModificationTime(filepath);

    if (file.isNull())
    {
        // File couldn't be opened, but use filename as fallback
        metadata.title = sanitizeUtf8(fs::path(clean_filepath).stem().string());
        metadata.duration_ms = 0;
        return std::nullopt;
    }

    // Extract tags
    if (file.tag())
    {
        TagLib::Tag *tag = file.tag();

        spdlog::info("Extracting metadata for file: {}", clean_filepath);

        auto titleStr = tag->title().to8Bit();
        if (!titleStr.empty())
        {
            spdlog::info("Title: {}", titleStr);
            metadata.title = sanitizeUtf8(titleStr);
        }

        auto artistStr = tag->artist().to8Bit();
        if (!artistStr.empty())
        {
            spdlog::info("Artist: {}", artistStr);
            metadata.artist = sanitizeUtf8(artistStr);
        }

        auto albumStr = tag->album().to8Bit();
        if (!albumStr.empty())
        {
            spdlog::info("Album: {}", albumStr);
            metadata.album = sanitizeUtf8(albumStr);
        }

        auto genreStr = tag->genre().to8Bit();
        if (!genreStr.empty())
        {
            metadata.genre = sanitizeUtf8(genreStr);
        }

        if (tag->year() > 0)
        {
            metadata.year = tag->year();
        }
    }

    // Fallback: if no title found, use filename
    if (!metadata.title)
    {
        metadata.title = sanitizeUtf8(fs::path(clean_filepath).stem().string());
    }

    // Get duration from audio properties
    if (file.audioProperties())
    {
        metadata.duration_ms = file.audioProperties()->lengthInMilliseconds();
    }
    else
    {
        metadata.duration_ms = 0;
    }

    return metadata;
}

std::vector<TrackMetadata> MetadataExtractor::extractFromDirectory(
    const std::string &directory_path,
    bool recursive,
    bool verbose)
{
    // Check if this is a Dropbox path and delegate if so
    if (PathHandler::isDropboxPath(directory_path))
    {
        return extractFromDropboxDirectory(directory_path, recursive, verbose);
    }

    std::vector<TrackMetadata> results;
    const std::vector<std::string> valid_extensions = {".wav", ".mp3", ".flac", ".ogg"};

    namespace fs = std::filesystem;

    try
    {
        if (!fs::exists(directory_path) || !fs::is_directory(directory_path))
        {
            std::cerr << "Error: Directory does not exist: " << directory_path << std::endl;
            return results;
        }

        if (recursive)
        {
            for (const auto &entry : fs::recursive_directory_iterator(directory_path))
            {
                if (entry.is_regular_file())
                {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (std::find(valid_extensions.begin(), valid_extensions.end(), ext) != valid_extensions.end())
                    {

                        auto metadata = extract(entry.path().string(), verbose);
                        if (metadata)
                        {
                            results.push_back(*metadata);
                        }
                    }
                }
            }
        }
        else
        {
            for (const auto &entry : fs::directory_iterator(directory_path))
            {
                if (entry.is_regular_file())
                {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (std::find(valid_extensions.begin(), valid_extensions.end(), ext) != valid_extensions.end())
                    {

                        auto metadata = extract(entry.path().string());
                        if (metadata)
                        {
                            results.push_back(*metadata);
                        }
                    }
                }
            }
        }

        // Sort by filepath
        std::sort(results.begin(), results.end(),
                  [](const TrackMetadata &a, const TrackMetadata &b)
                  {
                      return a.filepath < b.filepath;
                  });
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }

    return results;
}

std::vector<TrackMetadata> MetadataExtractor::extractFromDropboxDirectory(
    const std::string &directory_path,
    bool recursive,
    bool verbose)
{
    std::vector<TrackMetadata> results;
    const std::vector<std::string> valid_extensions = {".wav", ".mp3", ".flac", ".ogg"};

    namespace fs = std::filesystem;

    auto* client = getDropboxClient();
    auto* temp_mgr = getTempFileManager();

    if (!client) {
        std::cerr << "Error: Dropbox client not initialized" << std::endl;
        return results;
    }

    if (!temp_mgr) {
        std::cerr << "Error: Temp file manager not initialized" << std::endl;
        return results;
    }

    try {
        std::string dropbox_path = PathHandler::parseDropboxUrl(directory_path);
        spdlog::info("Scanning Dropbox directory: {}", dropbox_path);

        auto files = client->listDirectory(dropbox_path, recursive);

        spdlog::info("Found {} items in Dropbox directory", files.size());

        for (const auto& file : files) {
            // Skip directories
            if (file.is_directory) {
                continue;
            }

            std::string ext = fs::path(file.path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (std::find(valid_extensions.begin(), valid_extensions.end(), ext)
                != valid_extensions.end()) {

                // Download temporarily for metadata extraction
                std::string dropbox_url = PathHandler::toDropboxUrl(file.path);
                std::string local_path = temp_mgr->getLocalPath(dropbox_url, *client);

                if (!local_path.empty()) {
                    auto metadata = extract(local_path, verbose);
                    if (metadata) {
                        // Replace local path with Dropbox URL
                        metadata->filepath = dropbox_url;
                        metadata->filename = fs::path(file.path).filename().string();

                        // Store Dropbox-specific cache keys
                        metadata->dropbox_hash = file.content_hash;
                        metadata->dropbox_rev = file.rev;
                        metadata->file_mtime = file.modified_time;

                        results.push_back(*metadata);
                    }
                } else {
                    spdlog::warn("Failed to download file for metadata extraction: {}", file.path);
                }
            }
        }

        // Sort by filepath
        std::sort(results.begin(), results.end(),
                  [](const TrackMetadata &a, const TrackMetadata &b)
                  {
                      return a.filepath < b.filepath;
                  });

        spdlog::info("Extracted metadata for {} Dropbox files", results.size());

    } catch (const std::exception& e) {
        std::cerr << "Error scanning Dropbox directory: " << e.what() << std::endl;
    }

    return results;
}

int64_t MetadataExtractor::getFileModificationTime(const std::string &filepath)
{
    struct stat st;
    if (stat(filepath.c_str(), &st) == 0)
    {
        return static_cast<int64_t>(st.st_mtime);
    }
    return 0;
}
