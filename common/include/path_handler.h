#ifndef PATH_HANDLER_H
#define PATH_HANDLER_H

#include <string>

enum class PathType {
    LOCAL,
    DROPBOX
};

class PathHandler {
public:
    // Detect path type
    static PathType getPathType(const std::string& path);

    // Check if path is dropbox URL
    static bool isDropboxPath(const std::string& path);

    // Parse dropbox:// URL to Dropbox path (removes dropbox:// prefix)
    // Example: "dropbox://Music/file.mp3" -> "/Music/file.mp3"
    static std::string parseDropboxUrl(const std::string& url);

    // Convert Dropbox path to dropbox:// URL (adds dropbox:// prefix)
    // Example: "/Music/file.mp3" -> "dropbox://Music/file.mp3"
    static std::string toDropboxUrl(const std::string& path);

    // URL encode a string for use in Dropbox API calls
    static std::string urlEncode(const std::string& str);

    // URL decode a string from Dropbox API responses
    static std::string urlDecode(const std::string& str);
};

#endif // PATH_HANDLER_H
