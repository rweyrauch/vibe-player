#ifndef DROPBOX_CLIENT_H
#define DROPBOX_CLIENT_H

#include <string>
#include <vector>
#include <optional>
#include <map>

class DropboxClient {
public:
    struct FileMetadata {
        std::string path;           // Dropbox path (e.g., "/Music/song.mp3")
        std::string id;             // File ID
        std::string content_hash;   // Dropbox content hash for caching
        std::string rev;            // Revision identifier
        int64_t size;               // File size in bytes
        int64_t modified_time;      // Server modified timestamp (Unix epoch)
        bool is_directory;          // True if this is a folder
    };

    explicit DropboxClient(const std::string& access_token);

    // List files in directory recursively
    std::vector<FileMetadata> listDirectory(const std::string& path,
                                           bool recursive = true);

    // Download file to local path
    bool downloadFile(const std::string& dropbox_path,
                     const std::string& local_path);

    // Get file metadata (for cache validation)
    std::optional<FileMetadata> getFileMetadata(const std::string& dropbox_path);

    // Get multiple file metadata in a batch (efficient for cache validation)
    std::vector<FileMetadata> getFileMetadataBatch(const std::vector<std::string>& dropbox_paths);

    // Test authentication
    bool testConnection();

    // Get last error message
    std::string getLastError() const { return last_error_; }

private:
    std::string access_token_;
    std::string last_error_;

    // HTTP request helpers
    std::string makeApiRequest(const std::string& endpoint,
                              const std::string& body);

    bool makeContentRequest(const std::string& endpoint,
                           const std::string& dropbox_path,
                           const std::string& local_path);

    // Retry logic
    std::string makeRequestWithRetry(const std::string& endpoint,
                                    const std::string& body,
                                    int max_retries = 3);

    // Parse metadata from JSON response
    std::optional<FileMetadata> parseFileMetadata(const std::string& json_str);
    std::vector<FileMetadata> parseFileMetadataList(const std::string& json_str);

    // Helper to parse ISO 8601 datetime to Unix timestamp
    int64_t parseIso8601(const std::string& datetime);
};

#endif // DROPBOX_CLIENT_H
