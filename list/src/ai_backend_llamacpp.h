#ifndef AI_BACKEND_LLAMACPP_H
#define AI_BACKEND_LLAMACPP_H

#include "ai_backend.h"
#include <memory>

// Forward declarations to avoid exposing llama.h in header
struct llama_model;
struct llama_context;
struct llama_sampler;

struct LlamaConfig {
    int context_size = 2048;
    int threads = 4;
    float temperature = 0.7f;
    int max_tokens = 1024;
};

class LlamaCppBackend : public AIBackend {
public:
    explicit LlamaCppBackend(const std::string& model_path);
    ~LlamaCppBackend();

    std::optional<std::vector<std::string>> generate(
        const std::string& user_prompt,
        const std::vector<TrackMetadata>& library_metadata,
        StreamCallback stream_callback = nullptr,
        bool verbose = false
    ) override;

    std::string name() const override { return "llama.cpp"; }
    bool validate(std::string& error_message) const override;

    // Set configuration before calling generate
    void setConfig(const LlamaConfig& config) { config_ = config; }

private:
    std::string model_path_;
    LlamaConfig config_;
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    bool initialized_ = false;

    bool initializeModel();
    void cleanup();

    std::string generateText(const std::string& prompt, StreamCallback stream_callback);
};

#endif // AI_BACKEND_LLAMACPP_H
