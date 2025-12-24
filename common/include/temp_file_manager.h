#ifndef TEMP_FILE_MANAGER_H
#define TEMP_FILE_MANAGER_H

#include "dropbox_client.h"
#include <string>
#include <map>
#include <set>
#include <mutex>

class TempFileManager {
public:
    explicit TempFileManager(const std::string& temp_dir = "/tmp/vibe-player");
    ~TempFileManager();

    // Get local path for dropbox file (download if needed)
    // Returns empty string on failure
    std::string getLocalPath(const std::string& dropbox_url,
                            DropboxClient& client);

    // Clear specific file
    void clearFile(const std::string& dropbox_url);

    // Clear all cached files
    void clearAll();

    // Clear files not accessed recently (for session management)
    void clearOldFiles(int max_age_seconds = 3600);

    // Mark file as active (prevents cleanup during playback)
    void markActive(const std::string& dropbox_url);

    // Mark file as inactive (allows cleanup)
    void markInactive(const std::string& dropbox_url);

private:
    std::string temp_dir_;
    std::map<std::string, std::string> path_cache_;  // dropbox_url -> local_path
    std::map<std::string, int64_t> access_times_;    // local_path -> last_access
    std::set<std::string> active_files_;             // local_paths currently in use
    std::mutex mutex_;  // Thread safety

    // Generate unique local path for dropbox file
    std::string generateLocalPath(const std::string& dropbox_url);

    // Get current Unix timestamp
    int64_t getCurrentTime();

    // Ensure temp directory exists
    void ensureTempDirectory();
};

#endif // TEMP_FILE_MANAGER_H
