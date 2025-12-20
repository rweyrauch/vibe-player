#include "player.h"
#include "metadata.h"
#include "metadata_cache.h"
#include "ai_backend.h"
#include "ai_backend_claude.h"
#include "ai_backend_llamacpp.h"

#include <csignal>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <cxxopts.hpp>
#include <colors.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

volatile sig_atomic_t signal_received = 0;

void SignalHandler(int signum)
{
    signal_received = signum;
}

void InitializeLogger(bool verbose)
{
    namespace fs = std::filesystem;

    // Create log directory if it doesn't exist
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    fs::path log_dir = fs::path(home) / ".cache" / "cli-player";

    try {
        fs::create_directories(log_dir);

        // Create file logger
        auto log_path = log_dir / "cli-player.log";
        auto logger = spdlog::basic_logger_mt("cli-player", log_path.string());
        spdlog::set_default_logger(logger);

        // Set log level based on verbose flag
        if (verbose) {
            spdlog::set_level(spdlog::level::debug);
            spdlog::info("Verbose logging enabled");
        } else {
            spdlog::set_level(spdlog::level::info);
        }

        spdlog::info("CLI Audio Player started");
        spdlog::info("Log file: {}", log_path.string());

    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to initialize logger: " << e.what() << std::endl;
    }
}

void Cleanup()
{
    set_mouse_mode(false);
    set_raw_mode(false);
}

void PrintHelp()
{
    std::cout << "\nCommands:\n"
              << "  p - Play\n"
              << "  s - Stop\n"
              << "  u - Pause\n"
              << "  + - Volume up\n"
              << "  - - Volume down\n"
              << "  f - Forward 10s\n"
              << "  b - Back 10s\n"
              << "  n - Next track (directory mode)\n"
              << "  h - Help\n"
              << "  q - Quit\n"
              << std::endl;
}

void PrintStatus(AudioPlayer &player,
                 const std::vector<TrackMetadata> &playlist,
                 size_t current_track_index,
                 bool directory_mode)
{
    int pos = player.getPosition();
    int dur = player.getDuration();
    float vol = player.getVolume();

    std::string state;
    if (player.isPlaying())
        state = "Playing";
    else if (player.isPaused())
        state = "Paused";
    else
        state = "Stopped";

    const TrackMetadata& track = playlist[current_track_index];

    // Build display string: Artist - Album - Song
    std::string display;
    if (track.artist) {
        display += *track.artist + " - ";
    }
    if (track.album) {
        display += *track.album + " - ";
    }
    if (track.title) {
        display += *track.title;
    } else {
        display += track.filename;
    }

    std::cout << "\r[" << state << "] "
              << display
              << " | " << std::setfill('0') << std::setw(2) << (pos / 60000) << ":"
              << std::setw(2) << ((pos/1000) % 60) << " / "
              << std::setw(2) << ((dur/1000) / 60) << ":"
              << std::setw(2) << ((dur/1000) % 60)
              << " | Vol: " << std::setfill(' ') << std::setw(3) << (int)(vol * 100) << "%";

    if (directory_mode)
    {
        std::cout << " | Track " << (current_track_index + 1) << "/" << playlist.size();
    }

    std::cout << "          " << std::flush;
}

bool HandleCommand(char ch,
                          AudioPlayer &player,
                          std::vector<TrackMetadata> &playlist,
                          size_t &current_track_index,
                          bool directory_mode)
{
    bool running = true;

    const auto cmd = std::tolower(ch);
    switch (cmd)
    {
    case 'q':
        running = false;
        break;
    case 'h':
        PrintHelp();
        break;
    case 'p':
        player.play();
        break;
    case 's':
        player.stop();
        break;
    case 'u':
        player.pause();
        break;
    case ' ':
        if (player.isPlaying())
        {
            player.pause();
        }
        else
        {
            player.play();
        }
        break;
    case '+':
        player.setVolume(std::min(1.0f, player.getVolume() + 0.05f));
        break;
    case '-':
        player.setVolume(std::max(0.0f, player.getVolume() - 0.05f));
        break;
    case 'f':
    case RIGHT_ARROW:
        player.seek(player.getPosition() + 10*1000);
        break;
    case 'b':
    case LEFT_ARROW:
        player.seek(std::max(0LL, player.getPosition() - 10*1000LL));
        break;
    case 'n':
        if (directory_mode && current_track_index < playlist.size() - 1)
        {
            current_track_index++;
            player.cleanup();
            player.loadFile(playlist[current_track_index].filepath);
            player.play();
        }
        break;
    default:
        break;
    }
    return running;
}

bool CheckAutoAdvance(AudioPlayer &player,
                      std::vector<TrackMetadata> &playlist,
                      size_t &current_track_index,
                      bool directory_mode,
                      bool repeat,
                      bool &was_playing)
{
    if (was_playing && !player.isPlaying() && !player.isPaused())
    {
        was_playing = false;

        // Check if we've reached the end of the playlist
        if (directory_mode && (current_track_index == playlist.size() - 1) && !repeat)
        {
            // End of playlist reached, signal to exit
            return true;
        }

        if (repeat && directory_mode && (current_track_index == playlist.size() - 1))
        {
            current_track_index = 0;
        }

        if (directory_mode && (current_track_index < playlist.size() - 1))
        {
            current_track_index++;
            player.cleanup();
            if (player.loadFile(playlist[current_track_index].filepath))
            {
                player.play();
                was_playing = true;
                std::cout << "\n";
                return false;
            }
        }
    }
    else if (player.isPlaying())
    {
        was_playing = true;
    }
    return false;
}

std::vector<TrackMetadata> ScanDirectoryForAudio(const std::string &dir_path, bool verbose = false)
{
    // Use the existing metadata extractor from the metadata module
    return MetadataExtractor::extractFromDirectory(dir_path, true, verbose);
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
            std::cout << "Using cached metadata (" << cached->size()
                     << " tracks)" << std::endl;
            return *cached;
        }
    }

    std::cout << "Scanning library and extracting metadata..." << std::endl;
    auto metadata = MetadataExtractor::extractFromDirectory(library_path, true, verbose);

    std::cout << "Extracted metadata for " << metadata.size() << " tracks" << std::endl;

    if (!cache.save(library_path, metadata)) {
        std::cerr << "Warning: Failed to save metadata cache" << std::endl;
    }

    return metadata;
}

int main(int argc, char *argv[])
{ 
    // Parse command line arguments using cxxopts
    cxxopts::Options options("cli-player",
                             "CLI Audio Player - Play audio files from command line");

    options.add_options()("d,directory", "Play all audio files in a directory", cxxopts::value<std::string>())
                         ("f,file", "Play a single audio file", cxxopts::value<std::string>())
                         ("l,library", "Music library path for AI playlist generation", cxxopts::value<std::string>())
                         ("p,prompt", "Generate AI playlist from description", cxxopts::value<std::string>())
                         ("ai-backend", "AI backend: 'claude' or 'llamacpp' (default: claude)",
                             cxxopts::value<std::string>()->default_value("claude"))
                         ("claude-model", "Claude model preset: 'fast' (Haiku), 'balanced' (Sonnet), 'best' (Opus) or full model ID (default: fast)",
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
                         ("r,repeat", "Repeat playlist")
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
    const bool repeat = result.count("repeat") > 0;
    const bool force_scan = result.count("force-scan") > 0;
    const bool verbose = result.count("verbose") > 0;

    // Initialize logger
    InitializeLogger(verbose);

    // Determine mode and input path
    bool directory_mode = false;
    std::string input_path;
    std::vector<TrackMetadata> playlist;
    size_t current_track_index = 0;

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

        } else {
            std::cerr << "Error: Invalid AI backend '" << backend_type << "'" << std::endl;
            std::cerr << "Valid options: 'claude' or 'llamacpp'" << std::endl;
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
                playlist.push_back(library_metadata[idx]);
            }
        }

        if (playlist.empty())
        {
            std::cerr << "Error: AI generated empty playlist" << std::endl;
            return 1;
        }

        // Apply shuffle if requested
        if (shuffle)
        {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(playlist.begin(), playlist.end(), g);
        }

        // Set directory_mode = true for auto-advance
        directory_mode = true;

        std::cout << "\nGenerated AI playlist with " << playlist.size()
                  << " tracks" << std::endl;

        // Show selected tracks
        for (size_t i = 0; i < playlist.size(); i++)
        {
            const TrackMetadata& track = playlist[i];
            std::cout << "  " << (i + 1) << ". ";
            if (track.artist) {
                std::cout << *track.artist << " - ";
            }
            if (track.title) {
                std::cout << *track.title;
            } else {
                std::cout << track.filename;
            }
            std::cout << std::endl;
        }
    }
    else if (result.count("directory"))
    {
        // Directory mode
        directory_mode = true;
        input_path = result["directory"].as<std::string>();

        std::cout << "Scanning directory and extracting metadata..." << std::endl;
        playlist = ScanDirectoryForAudio(input_path, verbose);

        if (playlist.empty())
        {
            std::cerr << "No audio files found in directory: " << input_path << std::endl;
            return 1;
        }

        if (shuffle)
        {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(playlist.begin(), playlist.end(), g);
        }

        std::cout << "Found " << playlist.size() << " audio file(s) in directory" << std::endl;
        for (size_t i = 0; i < playlist.size(); ++i)
        {
            const TrackMetadata& track = playlist[i];
            std::cout << "  " << (i + 1) << ". ";
            if (track.artist) {
                std::cout << *track.artist << " - ";
            }
            if (track.title) {
                std::cout << *track.title;
            } else {
                std::cout << track.filename;
            }
            std::cout << std::endl;
        }

    }
    else if (result.count("file"))
    {
        // Single file mode
        directory_mode = false;
        input_path = result["file"].as<std::string>();

        // Extract metadata for the single file
        auto metadata = MetadataExtractor::extract(input_path, verbose);
        if (metadata) {
            playlist.push_back(*metadata);
        } else {
            std::cerr << "Error: Failed to extract metadata from file: " << input_path << std::endl;
            return 1;
        }
    }
    else
    {
        std::cerr << "Error: Please specify an audio file, directory, or AI prompt" << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    AudioPlayer player;

    // Setup signal handlers
    atexit(Cleanup);
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // Load first track
    if (!player.loadFile(playlist[current_track_index].filepath))
    {
        return 1;
    }

    std::cout << "\nCLI Audio Player - Press 'h' for help, 'p' to play\n" << std::endl;

    // Main event loop
    bool running = true;
    bool was_playing = false;

    set_raw_mode(true);

    // Start playing automatically
    player.play();
    was_playing = true;

    while (running && !signal_received)
    {
        // Print status
        PrintStatus(player, playlist, current_track_index, directory_mode);

        // Check for input
        char ch;
        if ((ch = quick_read()) != ERR)
        {
            running = HandleCommand(ch, player, playlist, current_track_index, directory_mode);
        }

        // Check for auto-advance and playlist end
        if (CheckAutoAdvance(player, playlist, current_track_index, directory_mode, repeat, was_playing))
        {
            // Playlist has ended, exit
            running = false;
        }

        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl;

    // Restore stdin to blocking mode
    set_raw_mode(false);

    player.cleanup();

    return 0;
}
