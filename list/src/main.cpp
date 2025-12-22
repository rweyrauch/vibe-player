/*
 * vibe-player
 * main.cpp
 */

#include "metadata.h"
#include "metadata_cache.h"
#include "playlist.h"
#include "ai_backend.h"
#include "ai_backend_claude.h"
#include "ai_backend_llamacpp.h"
#include "ai_backend_chatgpt.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

void InitializeLogger(bool verbose)
{
    namespace fs = std::filesystem;

    // Create log directory if it doesn't exist
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    fs::path log_dir = fs::path(home) / ".cache" / "vibe-playlist";

    try {
        fs::create_directories(log_dir);

        // Create file logger
        auto log_path = log_dir / "vibe-playlist.log";
        auto logger = spdlog::basic_logger_mt("vibe-playlist", log_path.string());
        spdlog::set_default_logger(logger);

        // Set log level based on verbose flag
        if (verbose) {
            spdlog::set_level(spdlog::level::debug);
            spdlog::info("Verbose logging enabled");
        } else {
            spdlog::set_level(spdlog::level::info);
        }

        spdlog::info("Vibe Playlist Generator started");
        spdlog::info("Log file: {}", log_path.string());

    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to initialize logger: " << e.what() << std::endl;
    }
}

std::vector<TrackMetadata> GetLibraryMetadata(
    const std::string& library_path,
    bool force_rescan = false,
    bool verbose = false)
{
    MetadataCache cache;

    if (!force_rescan) {
        auto cached = cache.load(library_path);
        if (cached && cache.isValid(library_path, *cached)) {
            std::cerr << "Using cached metadata (" << cached->size()
                     << " tracks)" << std::endl;
            return *cached;
        }
    }

    std::cerr << "Scanning library and extracting metadata..." << std::endl;
    auto metadata = MetadataExtractor::extractFromDirectory(library_path, true, verbose);

    std::cerr << "Extracted metadata for " << metadata.size() << " tracks" << std::endl;

    if (!cache.save(library_path, metadata)) {
        std::cerr << "Warning: Failed to save metadata cache" << std::endl;
    }

    return metadata;
}

int main(int argc, char *argv[])
{
    // Parse command line arguments using cxxopts
    cxxopts::Options options("vibe-playlist",
                             "Vibe Playlist Generator - Generate music playlists");

    options.add_options()
        ("d,directory", "Generate playlist from directory", cxxopts::value<std::string>())
        ("f,file", "Generate playlist from single file", cxxopts::value<std::string>())
        ("l,library", "Music library path for AI playlist generation", cxxopts::value<std::string>())
        ("p,prompt", "Generate AI playlist from description", cxxopts::value<std::string>())
        ("ai-backend", "AI backend: 'claude', 'chatgpt', or 'llamacpp' (default: claude)",
            cxxopts::value<std::string>()->default_value("claude"))
        ("claude-model", "Claude model preset: 'fast' (Haiku), 'balanced' (Sonnet), 'best' (Opus) or full model ID (default: fast)",
            cxxopts::value<std::string>()->default_value("fast"))
        ("chatgpt-model", "ChatGPT model preset: 'fast' (GPT-4o Mini), 'balanced' (GPT-4o), 'best' (GPT-4) or full model ID (default: fast)",
            cxxopts::value<std::string>()->default_value("fast"))
        ("ai-model", "Path to GGUF model file (required for llamacpp backend)",
            cxxopts::value<std::string>())
        ("ai-context-size", "Context size for llama.cpp (default: 2048)",
            cxxopts::value<int>()->default_value("2048"))
        ("ai-threads", "Number of threads for llama.cpp (default: 4)",
            cxxopts::value<int>()->default_value("4"))
        ("force-scan", "Force rescan library metadata (ignore cache)")
        ("verbose", "Display AI prompts and debug information")
        ("s,shuffle", "Shuffle playlist")
        ("save", "Save playlist to file (default: output to stdout)", cxxopts::value<std::string>())
        ("h,help", "Print usage");

    options.parse_positional({"file"});
    options.positional_help("<audio_file>");

    cxxopts::ParseResult result;
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const cxxopts::exceptions::exception &e)
    {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    const bool shuffle = result.count("shuffle") > 0;
    const bool force_scan = result.count("force-scan") > 0;
    const bool verbose = result.count("verbose") > 0;
    const bool save_to_file = result.count("save") > 0;

    // Initialize logger
    InitializeLogger(verbose);

    std::vector<TrackMetadata> playlist_tracks;

    // AI Playlist mode
    if (result.count("prompt"))
    {
        if (!result.count("library"))
        {
            std::cerr << "Error: --library required with --prompt" << std::endl;
            std::cout << options.help() << std::endl;
            return 1;
        }

        std::string library_path = result["library"].as<std::string>();
        std::string prompt_text = result["prompt"].as<std::string>();
        std::string backend_type = result["ai-backend"].as<std::string>();

        // Get or generate metadata
        auto library_metadata = GetLibraryMetadata(library_path, force_scan, verbose);

        if (library_metadata.empty())
        {
            std::cerr << "Error: No audio files found in library" << std::endl;
            return 1;
        }

        // Create backend based on flag
        std::unique_ptr<AIBackend> backend;
        StreamCallback stream_cb = nullptr;

        if (backend_type == "claude") {
            // Check for API key
            const char* api_key = std::getenv("ANTHROPIC_API_KEY");
            if (!api_key || strlen(api_key) == 0) {
                std::cerr << "Error: ANTHROPIC_API_KEY environment variable not set" << std::endl;
                std::cerr << "Set it with: export ANTHROPIC_API_KEY=your_key_here" << std::endl;
                return 1;
            }

            // Get model selection
            std::string model_selection = result["claude-model"].as<std::string>();

            // Check if it's a preset or a full model ID
            if (model_selection == "fast" || model_selection == "balanced" ||
                model_selection == "best" || model_selection == "haiku" ||
                model_selection == "sonnet" || model_selection == "opus") {
                ClaudeModel model = ClaudeBackend::parseModelPreset(model_selection);
                backend = std::make_unique<ClaudeBackend>(api_key, model);
            } else {
                // Use as full model ID
                backend = std::make_unique<ClaudeBackend>(api_key, model_selection);
            }

        } else if (backend_type == "llamacpp") {
            // Check for model path
            if (!result.count("ai-model")) {
                std::cerr << "Error: --ai-model required for llamacpp backend" << std::endl;
                std::cerr << "Example: --ai-model=/path/to/model.gguf" << std::endl;
                return 1;
            }

            std::string model_path = result["ai-model"].as<std::string>();
            auto llamacpp_backend = std::make_unique<LlamaCppBackend>(model_path);

            // Configure llama.cpp
            LlamaConfig config;
            config.context_size = result["ai-context-size"].as<int>();
            config.threads = result["ai-threads"].as<int>();
            llamacpp_backend->setConfig(config);

            // Setup streaming callback for progress
            stream_cb = [](const std::string& chunk, bool is_final) {
                if (!is_final) {
                    std::cerr << chunk << std::flush;
                } else {
                    std::cerr << "\n";
                }
            };

            backend = std::move(llamacpp_backend);

        } else if (backend_type == "chatgpt") {
            // Check for API key
            const char* api_key = std::getenv("OPENAI_API_KEY");
            if (!api_key || strlen(api_key) == 0) {
                std::cerr << "Error: OPENAI_API_KEY environment variable not set" << std::endl;
                std::cerr << "Set it with: export OPENAI_API_KEY=your_key_here" << std::endl;
                return 1;
            }

            // Get model selection
            std::string model_selection = result["chatgpt-model"].as<std::string>();

            // Check if it's a preset or a full model ID
            if (model_selection == "fast" || model_selection == "balanced" ||
                model_selection == "best" || model_selection == "mini" ||
                model_selection == "gpt-4o" || model_selection == "gpt-4") {
                ChatGPTModel model = ChatGPTBackend::parseModelPreset(model_selection);
                backend = std::make_unique<ChatGPTBackend>(api_key, model);
            } else {
                // Use as full model ID
                backend = std::make_unique<ChatGPTBackend>(api_key, model_selection);
            }

        } else {
            std::cerr << "Error: Invalid AI backend '" << backend_type << "'" << std::endl;
            std::cerr << "Valid options: 'claude', 'chatgpt', or 'llamacpp'" << std::endl;
            return 1;
        }

        // Validate backend
        std::string error_msg;
        if (!backend->validate(error_msg)) {
            std::cerr << "Error: " << error_msg << std::endl;
            return 1;
        }

        // Generate playlist
        auto track_indices = backend->generate(prompt_text, library_metadata, stream_cb, verbose);

        if (!track_indices)
        {
            std::cerr << "Error: Failed to generate AI playlist" << std::endl;
            return 1;
        }

        // Convert indices to track metadata
        for (const auto& idx_str : *track_indices)
        {
            size_t idx = std::stoull(idx_str);
            if (idx < library_metadata.size())
            {
                playlist_tracks.push_back(library_metadata[idx]);
            }
        }

        if (playlist_tracks.empty())
        {
            std::cerr << "Error: AI generated empty playlist" << std::endl;
            return 1;
        }

        std::cerr << "Generated AI playlist with " << playlist_tracks.size()
                  << " tracks" << std::endl;
    }
    else if (result.count("directory"))
    {
        // Directory mode
        std::string dir_path = result["directory"].as<std::string>();

        std::cerr << "Scanning directory and extracting metadata..." << std::endl;
        playlist_tracks = MetadataExtractor::extractFromDirectory(dir_path, true, verbose);

        if (playlist_tracks.empty())
        {
            std::cerr << "No audio files found in directory: " << dir_path << std::endl;
            return 1;
        }

        std::cerr << "Found " << playlist_tracks.size() << " audio file(s) in directory" << std::endl;
    }
    else if (result.count("file"))
    {
        // Single file mode
        std::string file_path = result["file"].as<std::string>();

        // Extract metadata for the single file
        auto metadata = MetadataExtractor::extract(file_path, verbose);
        if (metadata) {
            playlist_tracks.push_back(*metadata);
        } else {
            std::cerr << "Error: Failed to extract metadata from file: " << file_path << std::endl;
            return 1;
        }
    }
    else
    {
        std::cerr << "Error: Please specify --directory, --file, or --prompt with --library" << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    // Apply shuffle if requested
    if (shuffle)
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(playlist_tracks.begin(), playlist_tracks.end(), g);
        std::cerr << "Playlist shuffled" << std::endl;
    }

    // Create playlist object
    Playlist playlist = Playlist::fromTracks(playlist_tracks);

    // Output playlist
    if (save_to_file) {
        std::string filename = result["save"].as<std::string>();

        // Determine format from extension or default to text
        PlaylistFormat format = PlaylistFormat::TEXT;
        if (filename.ends_with(".json")) {
            format = PlaylistFormat::JSON;
        }

        if (playlist.saveToFile(filename, format)) {
            std::cerr << "Playlist saved to: " << filename << std::endl;
        } else {
            std::cerr << "Error: Failed to save playlist to file" << std::endl;
            return 1;
        }
    } else {
        // Output to stdout as text (just paths)
        std::cout << playlist.toText() << std::endl;
    }

    return 0;
}
