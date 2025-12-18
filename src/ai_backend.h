#ifndef AI_BACKEND_H
#define AI_BACKEND_H

#include "metadata.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>

// Callback for streaming progress: (text_chunk, is_final)
// When is_final is false, text_chunk contains incremental tokens
// When is_final is true, text_chunk contains the complete response
using StreamCallback = std::function<void(const std::string&, bool)>;

// Abstract base class for AI backends
class AIBackend {
public:
    virtual ~AIBackend() = default;

    // Generate playlist from prompt
    // Returns vector of track indices (as strings for consistency with current API)
    virtual std::optional<std::vector<std::string>> generate(
        const std::string& user_prompt,
        const std::vector<TrackMetadata>& library_metadata,
        StreamCallback stream_callback = nullptr
    ) = 0;

    // Backend name for display/logging
    virtual std::string name() const = 0;

    // Validate backend is ready (model loaded, API key present, etc.)
    // Returns true if valid, false otherwise with error_message populated
    virtual bool validate(std::string& error_message) const = 0;
};

#endif // AI_BACKEND_H
