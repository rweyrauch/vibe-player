#include "temp_file_manager.h"
#include "path_handler.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

namespace fs = std::filesystem;

TempFileManager::TempFileManager(const std::string& temp_dir)
    : temp_dir_(temp_dir)
{
    ensureTempDirectory();
}

TempFileManager::~TempFileManager()
{
    clearAll();
}

void TempFileManager::ensureTempDirectory()
{
    try {
        if (!fs::exists(temp_dir_)) {
            fs::create_directories(temp_dir_);
            spdlog::debug("Created temp directory: {}", temp_dir_);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to create temp directory {}: {}", temp_dir_, e.what());
    }
}

std::string TempFileManager::getLocalPath(const std::string& dropbox_url,
                                         DropboxClient& client)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already cached
    auto it = path_cache_.find(dropbox_url);
    if (it != path_cache_.end()) {
        const std::string& local_path = it->second;

        // Verify file still exists
        if (fs::exists(local_path)) {
            // Update access time
            access_times_[local_path] = getCurrentTime();
            spdlog::debug("Using cached file for {}: {}", dropbox_url, local_path);
            return local_path;
        } else {
            // File was deleted, remove from cache
            path_cache_.erase(it);
            access_times_.erase(local_path);
        }
    }

    // Generate local path
    std::string local_path = generateLocalPath(dropbox_url);

    // Download file
    std::string dropbox_path = PathHandler::parseDropboxUrl(dropbox_url);

    spdlog::info("Downloading Dropbox file: {} -> {}", dropbox_path, local_path);

    if (!client.downloadFile(dropbox_path, local_path)) {
        spdlog::error("Failed to download {}: {}", dropbox_path, client.getLastError());
        return "";
    }

    // Verify file was created
    if (!fs::exists(local_path)) {
        spdlog::error("Downloaded file does not exist: {}", local_path);
        return "";
    }

    // Cache the path
    path_cache_[dropbox_url] = local_path;
    access_times_[local_path] = getCurrentTime();

    spdlog::debug("Cached Dropbox file: {} -> {}", dropbox_url, local_path);

    return local_path;
}

void TempFileManager::clearFile(const std::string& dropbox_url)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = path_cache_.find(dropbox_url);
    if (it != path_cache_.end()) {
        const std::string& local_path = it->second;

        // Don't delete if active
        if (active_files_.count(local_path) > 0) {
            spdlog::debug("Skipping deletion of active file: {}", local_path);
            return;
        }

        try {
            if (fs::exists(local_path)) {
                fs::remove(local_path);
                spdlog::debug("Deleted temp file: {}", local_path);
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to delete temp file {}: {}", local_path, e.what());
        }

        path_cache_.erase(it);
        access_times_.erase(local_path);
    }
}

void TempFileManager::clearAll()
{
    std::lock_guard<std::mutex> lock(mutex_);

    spdlog::debug("Clearing all temp files in {}", temp_dir_);

    for (const auto& [dropbox_url, local_path] : path_cache_) {
        // Don't delete if active
        if (active_files_.count(local_path) > 0) {
            spdlog::debug("Skipping deletion of active file: {}", local_path);
            continue;
        }

        try {
            if (fs::exists(local_path)) {
                fs::remove(local_path);
                spdlog::debug("Deleted temp file: {}", local_path);
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to delete temp file {}: {}", local_path, e.what());
        }
    }

    path_cache_.clear();
    access_times_.clear();
}

void TempFileManager::clearOldFiles(int max_age_seconds)
{
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t current_time = getCurrentTime();
    int64_t cutoff_time = current_time - max_age_seconds;

    std::vector<std::string> to_delete;

    for (const auto& [local_path, access_time] : access_times_) {
        if (access_time < cutoff_time) {
            // Don't delete if active
            if (active_files_.count(local_path) > 0) {
                continue;
            }

            to_delete.push_back(local_path);
        }
    }

    for (const auto& local_path : to_delete) {
        try {
            if (fs::exists(local_path)) {
                fs::remove(local_path);
                spdlog::debug("Deleted old temp file: {}", local_path);
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to delete old temp file {}: {}", local_path, e.what());
        }

        // Remove from cache
        access_times_.erase(local_path);

        // Find and remove from path_cache_
        for (auto it = path_cache_.begin(); it != path_cache_.end();) {
            if (it->second == local_path) {
                it = path_cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (!to_delete.empty()) {
        spdlog::info("Cleaned up {} old temp files", to_delete.size());
    }
}

void TempFileManager::markActive(const std::string& dropbox_url)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = path_cache_.find(dropbox_url);
    if (it != path_cache_.end()) {
        active_files_.insert(it->second);
        spdlog::debug("Marked file as active: {}", it->second);
    }
}

void TempFileManager::markInactive(const std::string& dropbox_url)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = path_cache_.find(dropbox_url);
    if (it != path_cache_.end()) {
        active_files_.erase(it->second);
        spdlog::debug("Marked file as inactive: {}", it->second);
    }
}

std::string TempFileManager::generateLocalPath(const std::string& dropbox_url)
{
    // Extract filename from Dropbox URL
    std::string dropbox_path = PathHandler::parseDropboxUrl(dropbox_url);
    fs::path path(dropbox_path);
    std::string filename = path.filename().string();

    // Generate SHA256 hash of full dropbox URL for uniqueness
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(dropbox_url.c_str()),
           dropbox_url.length(), hash);

    // Convert hash to hex string (first 16 bytes for brevity)
    std::ostringstream hash_str;
    for (int i = 0; i < 16; ++i) {
        hash_str << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(hash[i]);
    }

    // Combine hash and filename
    std::string local_filename = hash_str.str() + "_" + filename;

    return (fs::path(temp_dir_) / local_filename).string();
}

int64_t TempFileManager::getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
}
