#include "dropbox_client.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>

using json = nlohmann::json;

DropboxClient::DropboxClient(const std::string& access_token)
    : access_token_(access_token)
{
}

bool DropboxClient::testConnection()
{
    try {
        std::string response = makeApiRequest("/2/users/get_current_account", "null");
        if (!response.empty()) {
            json j = json::parse(response);
            if (j.contains("account_id")) {
                spdlog::debug("Dropbox connection successful");
                return true;
            }
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("Connection test failed: ") + e.what();
        spdlog::error("{}", last_error_);
    }
    return false;
}

std::vector<DropboxClient::FileMetadata> DropboxClient::listDirectory(
    const std::string& path,
    bool recursive)
{
    std::vector<FileMetadata> results;

    try {
        json request_body = {
            {"path", path.empty() || path == "/" ? "" : path},
            {"recursive", recursive},
            {"include_media_info", false},
            {"include_deleted", false},
            {"include_has_explicit_shared_members", false}
        };

        std::string response = makeRequestWithRetry("/2/files/list_folder", request_body.dump());
        if (response.empty()) {
            return results;
        }

        json response_json = json::parse(response);
        auto entries = parseFileMetadataList(response);
        results.insert(results.end(), entries.begin(), entries.end());

        // Handle pagination
        while (response_json.contains("has_more") && response_json["has_more"].get<bool>()) {
            std::string cursor = response_json["cursor"].get<std::string>();

            json continue_body = {{"cursor", cursor}};
            response = makeRequestWithRetry("/2/files/list_folder/continue", continue_body.dump());

            if (response.empty()) {
                break;
            }

            response_json = json::parse(response);
            entries = parseFileMetadataList(response);
            results.insert(results.end(), entries.begin(), entries.end());
        }

        spdlog::debug("Listed {} items from Dropbox path: {}", results.size(), path);
    } catch (const std::exception& e) {
        last_error_ = std::string("Failed to list directory: ") + e.what();
        spdlog::error("{}", last_error_);
    }

    return results;
}

bool DropboxClient::downloadFile(const std::string& dropbox_path,
                                const std::string& local_path)
{
    try {
        bool success = makeContentRequest("/2/files/download", dropbox_path, local_path);

        if (success) {
            spdlog::debug("Downloaded {} to {}", dropbox_path, local_path);
        } else {
            spdlog::error("Failed to download {} to {}", dropbox_path, local_path);
        }

        return success;
    } catch (const std::exception& e) {
        last_error_ = std::string("Failed to download file: ") + e.what();
        spdlog::error("{}", last_error_);
        return false;
    }
}

std::optional<DropboxClient::FileMetadata> DropboxClient::getFileMetadata(
    const std::string& dropbox_path)
{
    try {
        json request_body = {
            {"path", dropbox_path},
            {"include_media_info", false}
        };

        std::string response = makeRequestWithRetry("/2/files/get_metadata", request_body.dump());
        if (response.empty()) {
            return std::nullopt;
        }

        return parseFileMetadata(response);
    } catch (const std::exception& e) {
        last_error_ = std::string("Failed to get metadata: ") + e.what();
        spdlog::error("{}", last_error_);
        return std::nullopt;
    }
}

std::vector<DropboxClient::FileMetadata> DropboxClient::getFileMetadataBatch(
    const std::vector<std::string>& dropbox_paths)
{
    std::vector<FileMetadata> results;

    if (dropbox_paths.empty()) {
        return results;
    }

    try {
        json::array_t entries;
        for (const auto& path : dropbox_paths) {
            entries.push_back({{"path", path}});
        }

        json request_body = {{"entries", entries}};
        std::string response = makeRequestWithRetry("/2/files/get_metadata_batch", request_body.dump());

        if (response.empty()) {
            return results;
        }

        json response_json = json::parse(response);
        if (response_json.contains("entries")) {
            for (const auto& entry : response_json["entries"]) {
                if (entry.contains("metadata") && entry["metadata"].contains(".tag")) {
                    auto metadata = parseFileMetadata(entry["metadata"].dump());
                    if (metadata) {
                        results.push_back(*metadata);
                    }
                }
            }
        }

        spdlog::debug("Got metadata for {} files in batch", results.size());
    } catch (const std::exception& e) {
        last_error_ = std::string("Failed to get metadata batch: ") + e.what();
        spdlog::error("{}", last_error_);
    }

    return results;
}

std::string DropboxClient::makeApiRequest(const std::string& endpoint,
                                         const std::string& body)
{
    httplib::Client client("https://api.dropboxapi.com");
    client.set_connection_timeout(30, 0);  // 30 seconds
    client.set_read_timeout(30, 0);
    client.set_write_timeout(30, 0);

    httplib::Headers headers = {
        {"Authorization", "Bearer " + access_token_}
    };

    auto res = client.Post(endpoint, headers, body, "application/json");

    if (!res) {
        last_error_ = "HTTP request failed: " + httplib::to_string(res.error());
        throw std::runtime_error(last_error_);
    }

    if (res->status != 200) {
        last_error_ = "HTTP error " + std::to_string(res->status) + ": " + res->body;

        if (res->status == 401) {
            last_error_ += " (Invalid access token)";
        } else if (res->status == 409) {
            last_error_ += " (Path not found or conflict)";
        } else if (res->status == 429) {
            last_error_ += " (Rate limited)";
        }

        throw std::runtime_error(last_error_);
    }

    return res->body;
}

bool DropboxClient::makeContentRequest(const std::string& endpoint,
                                      const std::string& dropbox_path,
                                      const std::string& local_path)
{
    httplib::Client client("https://content.dropboxapi.com");
    client.set_connection_timeout(30, 0);  // 30 seconds
    client.set_read_timeout(60, 0);  // 60 seconds for large files
    client.set_write_timeout(30, 0);

    json dropbox_arg = {{"path", dropbox_path}};

    httplib::Headers headers = {
        {"Authorization", "Bearer " + access_token_},
        {"Dropbox-API-Arg", dropbox_arg.dump()}
    };

    auto res = client.Post(endpoint, headers, "", "text/plain");

    if (!res) {
        last_error_ = "HTTP request failed: " + httplib::to_string(res.error());
        return false;
    }

    if (res->status != 200) {
        last_error_ = "HTTP error " + std::to_string(res->status);
        return false;
    }

    // Write response body to file
    std::ofstream outfile(local_path, std::ios::binary);
    if (!outfile) {
        last_error_ = "Failed to open local file for writing: " + local_path;
        return false;
    }

    outfile.write(res->body.c_str(), res->body.size());
    outfile.close();

    if (!outfile) {
        last_error_ = "Failed to write to local file: " + local_path;
        return false;
    }

    return true;
}

std::string DropboxClient::makeRequestWithRetry(const std::string& endpoint,
                                               const std::string& body,
                                               int max_retries)
{
    int retries = 0;
    int delay_ms = 1000;  // Start with 1 second delay

    while (retries < max_retries) {
        try {
            return makeApiRequest(endpoint, body);
        } catch (const std::exception& e) {
            retries++;

            if (retries >= max_retries) {
                throw;  // Re-throw the exception after max retries
            }

            spdlog::warn("Request failed (attempt {}/{}): {}. Retrying in {}ms...",
                        retries, max_retries, e.what(), delay_ms);

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;  // Exponential backoff
        }
    }

    return "";
}

std::optional<DropboxClient::FileMetadata> DropboxClient::parseFileMetadata(
    const std::string& json_str)
{
    try {
        json j = json::parse(json_str);

        if (!j.contains(".tag")) {
            return std::nullopt;
        }

        std::string tag = j[".tag"].get<std::string>();
        bool is_directory = (tag == "folder");

        FileMetadata metadata;
        metadata.path = j.value("path_display", j.value("path_lower", ""));
        metadata.id = j.value("id", "");
        metadata.is_directory = is_directory;

        if (!is_directory) {
            metadata.content_hash = j.value("content_hash", "");
            metadata.rev = j.value("rev", "");
            metadata.size = j.value("size", 0);

            if (j.contains("server_modified")) {
                metadata.modified_time = parseIso8601(j["server_modified"].get<std::string>());
            } else {
                metadata.modified_time = 0;
            }
        } else {
            metadata.content_hash = "";
            metadata.rev = "";
            metadata.size = 0;
            metadata.modified_time = 0;
        }

        return metadata;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse file metadata: {}", e.what());
        return std::nullopt;
    }
}

std::vector<DropboxClient::FileMetadata> DropboxClient::parseFileMetadataList(
    const std::string& json_str)
{
    std::vector<FileMetadata> results;

    try {
        json j = json::parse(json_str);

        if (!j.contains("entries")) {
            return results;
        }

        for (const auto& entry : j["entries"]) {
            auto metadata = parseFileMetadata(entry.dump());
            if (metadata) {
                results.push_back(*metadata);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse file metadata list: {}", e.what());
    }

    return results;
}

int64_t DropboxClient::parseIso8601(const std::string& datetime)
{
    // Parse ISO 8601 format: "2023-12-25T12:34:56Z"
    std::tm tm = {};
    std::istringstream ss(datetime);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        return 0;
    }

    // Convert to Unix timestamp (UTC)
    return std::mktime(&tm);
}
