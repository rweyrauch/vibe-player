#include "ai_backend_llamacpp.h"
#include "ai_prompt_builder.h"

#include <llama.h>
#include <iostream>
#include <filesystem>
#include <cstring>

LlamaCppBackend::LlamaCppBackend(const std::string& model_path)
    : model_path_(model_path) {
}

LlamaCppBackend::~LlamaCppBackend() {
    cleanup();
}

bool LlamaCppBackend::validate(std::string& error_message) const {
    namespace fs = std::filesystem;

    if (!fs::exists(model_path_)) {
        error_message = "Model file not found: " + model_path_;
        return false;
    }

    if (!fs::is_regular_file(model_path_)) {
        error_message = "Model path is not a file: " + model_path_;
        return false;
    }

    return true;
}

bool LlamaCppBackend::initializeModel() {
    if (initialized_) {
        return true;
    }

    // Initialize llama backend
    llama_backend_init();

    // Load model
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0;  // CPU only for now

    model_ = llama_model_load_from_file(model_path_.c_str(), model_params);
    if (!model_) {
        std::cerr << "Error: Failed to load model from " << model_path_ << std::endl;
        return false;
    }

    // Create context
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = config_.context_size;
    ctx_params.n_threads = config_.threads;
    ctx_params.n_threads_batch = config_.threads;

    ctx_ = llama_init_from_model(model_, ctx_params);
    if (!ctx_) {
        std::cerr << "Error: Failed to create llama context" << std::endl;
        llama_model_free(model_);
        model_ = nullptr;
        return false;
    }

    initialized_ = true;
    return true;
}

void LlamaCppBackend::cleanup() {
    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    if (initialized_) {
        llama_backend_free();
        initialized_ = false;
    }
}

std::string LlamaCppBackend::generateText(const std::string& prompt, StreamCallback stream_callback) {
    if (!initialized_ && !initializeModel()) {
        return "";
    }

    // Get vocab from model
    const llama_vocab* vocab = llama_model_get_vocab(model_);

    // Tokenize prompt
    std::vector<llama_token> tokens;
    const int n_prompt_tokens = -llama_tokenize(vocab, prompt.c_str(), prompt.length(),
                                                  nullptr, 0, true, true);
    tokens.resize(n_prompt_tokens);
    llama_tokenize(vocab, prompt.c_str(), prompt.length(),
                   tokens.data(), tokens.size(), true, true);

    // Check if prompt fits in context
    if ((int)tokens.size() >= config_.context_size) {
        std::cerr << "Error: Prompt too long (" << tokens.size()
                  << " tokens, max " << config_.context_size << ")" << std::endl;
        return "";
    }

    // Create a batch using llama_batch_get_one for simple use case
    llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());

    // Evaluate the prompt
    if (llama_decode(ctx_, batch) != 0) {
        std::cerr << "Error: Failed to evaluate prompt" << std::endl;
        return "";
    }

    // Create sampler
    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(config_.temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // Generate tokens
    std::string generated_text;
    int n_generated = 0;
    const int n_ctx = llama_n_ctx(ctx_);
    int n_cur = tokens.size();

    while (n_generated < config_.max_tokens) {
        // Sample next token
        llama_token new_token = llama_sampler_sample(sampler, ctx_, n_cur - 1);

        // Check for EOS
        if (llama_vocab_is_eog(vocab, new_token)) {
            break;
        }

        // Convert token to text
        char buf[256];
        int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, true);
        if (n < 0) {
            std::cerr << "Error: Failed to convert token to text" << std::endl;
            break;
        }

        std::string token_text(buf, n);
        generated_text += token_text;

        // Stream callback
        if (stream_callback) {
            stream_callback(token_text, false);
        }

        // Check if we've reached context limit
        if (n_cur >= n_ctx) {
            std::cerr << "Warning: Reached context limit" << std::endl;
            break;
        }

        // Create batch with just the new token
        llama_batch next_batch = llama_batch_get_one(&new_token, 1);

        // Evaluate
        if (llama_decode(ctx_, next_batch) != 0) {
            std::cerr << "Error: Failed to decode" << std::endl;
            break;
        }

        n_cur++;
        n_generated++;
    }

    // Final stream callback
    if (stream_callback) {
        stream_callback(generated_text, true);
    }

    // Cleanup
    llama_sampler_free(sampler);

    return generated_text;
}

std::optional<std::vector<std::string>> LlamaCppBackend::generate(
    const std::string& user_prompt,
    const std::vector<TrackMetadata>& library_metadata,
    StreamCallback stream_callback) {

    if (library_metadata.empty()) {
        std::cerr << "Error: No tracks in library" << std::endl;
        return std::nullopt;
    }

    // Build prompt and get sampled indices
    std::vector<size_t> sampled_indices;
    PromptConfig config;
    config.max_tracks_in_prompt = 1000;  // More conservative for local models
    std::string prompt = AIPromptBuilder::buildPrompt(
        user_prompt, library_metadata, sampled_indices, config);

    // Show progress message
    if (stream_callback) {
        std::cerr << "Generating playlist";
    } else {
        std::cout << "Generating AI playlist..." << std::endl;
    }

    // Generate text with streaming
    std::string response_text = generateText(prompt, stream_callback);

    if (response_text.empty()) {
        std::cerr << "Error: Failed to generate response" << std::endl;
        return std::nullopt;
    }

    // Parse response
    auto playlist = AIPromptBuilder::parseJsonResponse(response_text, sampled_indices);

    if (playlist.empty()) {
        std::cerr << "Error: Generated empty playlist" << std::endl;
        return std::nullopt;
    }

    return playlist;
}
