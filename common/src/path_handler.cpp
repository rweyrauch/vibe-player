#include "path_handler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

PathType PathHandler::getPathType(const std::string& path)
{
    if (isDropboxPath(path)) {
        return PathType::DROPBOX;
    }
    return PathType::LOCAL;
}

bool PathHandler::isDropboxPath(const std::string& path)
{
    return path.size() >= 10 && path.substr(0, 10) == "dropbox://";
}

std::string PathHandler::parseDropboxUrl(const std::string& url)
{
    if (!isDropboxPath(url)) {
        return url;
    }

    // Remove "dropbox://" prefix (10 characters)
    std::string path = url.substr(10);

    // Ensure path starts with /
    if (path.empty() || path[0] != '/') {
        path = "/" + path;
    }

    return path;
}

std::string PathHandler::toDropboxUrl(const std::string& path)
{
    if (isDropboxPath(path)) {
        return path;  // Already a dropbox URL
    }

    std::string normalized_path = path;

    // Remove leading / if present (we'll add it back in the URL)
    if (!normalized_path.empty() && normalized_path[0] == '/') {
        normalized_path = normalized_path.substr(1);
    }

    return "dropbox://" + normalized_path;
}

std::string PathHandler::urlEncode(const std::string& str)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        // Keep alphanumeric and other safe characters
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            escaped << c;
            continue;
        }

        // Encode any other characters
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        escaped << std::nouppercase;
    }

    return escaped.str();
}

std::string PathHandler::urlDecode(const std::string& str)
{
    std::ostringstream decoded;

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            // Decode %XX sequences
            std::string hex = str.substr(i + 1, 2);
            int value = std::stoi(hex, nullptr, 16);
            decoded << static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            decoded << ' ';
        } else {
            decoded << str[i];
        }
    }

    return decoded.str();
}
