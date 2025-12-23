#include "ai_backend_llamacpp.h"
#include "ai_prompt_builder.h"

#include <llama.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <filesystem>
#include <cstring>

LlamaCppBackend::LlamaCppBackend(const std::string &model_path)
    : model_path_(model_path)
{
}

LlamaCppBackend::~LlamaCppBackend()
{
    cleanup();
}

bool LlamaCppBackend::validate(std::string &error_message) const
{
    namespace fs = std::filesystem;

    if (!fs::exists(model_path_))
    {
        error_message = "Model file not found: " + model_path_;
        return false;
    }

    if (!fs::is_regular_file(model_path_))
    {
        error_message = "Model path is not a file: " + model_path_;
        return false;
    }

    return true;
}

bool LlamaCppBackend::initializeModel()
{
    if (initialized_)
    {
        spdlog::debug("Model already initialized");
        return true;
    }

    spdlog::info("Initializing llama.cpp backend");
    spdlog::debug("Loading model from: {}", model_path_);

    // Initialize llama backend
    llama_backend_init();

    // Load model
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0; // CPU only for now

    model_ = llama_model_load_from_file(model_path_.c_str(), model_params);
    if (!model_)
    {
        spdlog::error("Failed to load model from {}", model_path_);
        std::cerr << "Error: Failed to load model from " << model_path_ << std::endl;
        return false;
    }

    spdlog::info("Model loaded successfully");

    // Create context
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = config_.context_size;
    ctx_params.n_threads = config_.threads;
    ctx_params.n_threads_batch = config_.threads;

    spdlog::debug("Creating llama context with {} context size, {} threads",
                  config_.context_size, config_.threads);

    ctx_ = llama_init_from_model(model_, ctx_params);
    if (!ctx_)
    {
        spdlog::error("Failed to create llama context");
        std::cerr << "Error: Failed to create llama context" << std::endl;
        llama_model_free(model_);
        model_ = nullptr;
        return false;
    }

    spdlog::info("llama.cpp backend initialized successfully");
    initialized_ = true;
    return true;
}

void LlamaCppBackend::cleanup()
{
    if (ctx_)
    {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_)
    {
        llama_model_free(model_);
        model_ = nullptr;
    }
    if (initialized_)
    {
        llama_backend_free();
        initialized_ = false;
    }
}

std::string LlamaCppBackend::generateText(const std::string &prompt, StreamCallback stream_callback)
{
    spdlog::debug("Entering generateText()");

    if (!initialized_)
    {
        spdlog::debug("Model not initialized, initializing now...");
        if (!initializeModel())
        {
            spdlog::error("Failed to initialize model");
            return "";
        }
        spdlog::debug("Model initialized successfully");
    }

    spdlog::debug("Generating text with prompt length: {} chars", prompt.length());
    spdlog::debug("First 200 chars of prompt: {}", prompt.substr(0, std::min(size_t(200), prompt.length())));

    // Get vocab from model
    spdlog::debug("Getting vocab from model");
    const llama_vocab *vocab = llama_model_get_vocab(model_);
    if (!vocab)
    {
        spdlog::error("Failed to get vocab from model");
        return "";
    }

    // Tokenize prompt
    spdlog::debug("Tokenizing prompt...");

    std::vector<llama_token> tokens;
    const int n_prompt_tokens = -llama_tokenize(vocab, prompt.c_str(), prompt.length(),
                                                nullptr, 0, true, true);

    if (n_prompt_tokens <= 0)
    {
        spdlog::error("Failed to tokenize prompt, got {} tokens", n_prompt_tokens);
        return "";
    }

    tokens.resize(n_prompt_tokens);
    llama_tokenize(vocab, prompt.c_str(), prompt.length(),
                   tokens.data(), tokens.size(), true, true);

    spdlog::debug("Tokenized prompt into {} tokens", tokens.size());

    // Check if prompt fits in context
    if ((int)tokens.size() >= config_.context_size)
    {
        std::cerr << "Error: Prompt too long (" << tokens.size()
                  << " tokens, max " << config_.context_size << ")" << std::endl;
        return "";
    }

    // Create a batch for the prompt tokens
    // llama_batch_get_one automatically sets logits for the last token
    llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());

    // Evaluate the prompt
    if (llama_decode(ctx_, batch) != 0)
    {
        std::cerr << "Error: Failed to evaluate prompt" << std::endl;
        return "";
    }

    // Create sampler chain with better defaults
    llama_sampler *sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());

    // Add top-k sampling (k=40 is a good default)
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));

    // Add top-p (nucleus) sampling (p=0.95 is a good default)
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.95f, 1));

    // Add temperature scaling
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(config_.temperature));

    // Add distribution sampling (final step)
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // Generate tokens
    std::string generated_text;
    int n_generated = 0;
    const int n_ctx = llama_n_ctx(ctx_);
    int n_cur = tokens.size();

    spdlog::debug("Starting token generation. Prompt tokens: {}, Max tokens: {}, Context size: {}",
                  n_cur, config_.max_tokens, n_ctx);

    while (n_generated < config_.max_tokens)
    {
        // Sample next token from the last computed logits
        // We use -1 to indicate we want the last logits in the context
        llama_token new_token = llama_sampler_sample(sampler, ctx_, -1);

        spdlog::debug("Generated token {}: id={}", n_generated, new_token);

        // Check for EOS
        if (llama_vocab_is_eog(vocab, new_token))
        {
            spdlog::debug("End of generation token received after {} tokens", n_generated);
            break;
        }

        // Convert token to text
        char buf[256];
        int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, true);
        if (n < 0)
        {
            std::cerr << "Error: Failed to convert token to text" << std::endl;
            break;
        }

        std::string token_text(buf, n);
        generated_text += token_text;

        // Stream callback
        if (stream_callback)
        {
            stream_callback(token_text, false);
        }

        // Check if we've reached context limit
        if (n_cur >= n_ctx)
        {
            std::cerr << "Warning: Reached context limit" << std::endl;
            break;
        }

        // Create batch with just the new token
        // llama_batch_get_one handles position and logits internally
        llama_batch next_batch = llama_batch_get_one(&new_token, 1);

        // Evaluate
        if (llama_decode(ctx_, next_batch) != 0)
        {
            std::cerr << "Error: Failed to decode" << std::endl;
            break;
        }

        n_cur++;
        n_generated++;
    }

    // Final stream callback
    if (stream_callback)
    {
        stream_callback(generated_text, true);
    }

    // Cleanup
    llama_sampler_free(sampler);

    spdlog::debug("Token generation complete. Generated {} tokens, {} characters",
                  n_generated, generated_text.length());

    if (generated_text.empty())
    {
        spdlog::warn("Generated text is empty - model may have immediately produced EOS token");
    }

    return generated_text;
}

std::optional<std::vector<std::string>> LlamaCppBackend::generate(
    const std::string &user_prompt,
    const std::vector<TrackMetadata> &library_metadata,
    StreamCallback stream_callback,
    bool verbose)
{

    if (library_metadata.empty())
    {
        std::cerr << "Error: No tracks in library" << std::endl;
        return std::nullopt;
    }

    // Build prompt and get sampled indices
    std::vector<size_t> sampled_indices;
    PromptConfig config;
    config.max_tracks_in_prompt = 50; // More conservative for local models
    std::string prompt = AIPromptBuilder::buildPrompt(
        user_prompt, library_metadata, sampled_indices, config);

    // Log debug information
    spdlog::info("llama.cpp Backend: Generating playlist for prompt: '{}'", user_prompt);
    spdlog::debug("Sampled {} tracks from {} total tracks", sampled_indices.size(), library_metadata.size());
    spdlog::debug("AI Prompt:\n{}", prompt);
    spdlog::debug("Using model: {}", model_path_);
    spdlog::debug("Context size: {}, Threads: {}", config_.context_size, config_.threads);

    // Flush logs before potentially crashing operation
    if (auto logger = spdlog::get("vibe-player"))
    {
        logger->flush();
    }

    // Show progress message
    if (stream_callback)
    {
        std::cerr << "Generating playlist";
    }
    else
    {
        std::cout << "Generating AI playlist..." << std::endl;
    }

    // Generate text with streaming
    spdlog::debug("About to call generateText()");
    if (auto logger = spdlog::get("vibe-player"))
    {
        logger->flush();
    }

    std::string response_text;
    try
    {
        response_text = generateText(prompt, stream_callback);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Exception in generateText: {}", e.what());
        std::cerr << "Error: Exception during generation: " << e.what() << std::endl;
        return std::nullopt;
    }
    catch (...)
    {
        spdlog::error("Unknown exception in generateText");
        std::cerr << "Error: Unknown exception during generation" << std::endl;
        return std::nullopt;
    }

    if (response_text.empty())
    {
        spdlog::error("llama.cpp failed to generate response");
        std::cerr << "Error: Failed to generate response" << std::endl;
        return std::nullopt;
    }

    spdlog::debug("llama.cpp response:\n{}", response_text);

    // Parse response
    auto playlist = AIPromptBuilder::parseJsonResponse(response_text, sampled_indices);

    if (playlist.empty())
    {
        spdlog::error("Generated empty playlist");
        std::cerr << "Error: Generated empty playlist" << std::endl;
        return std::nullopt;
    }

    spdlog::info("Successfully generated playlist with {} tracks", playlist.size());
    return playlist;
}
