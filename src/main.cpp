#include "player.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <cstring>
#include <ncurses.h>
#include <csignal>
#include <cxxopts.hpp>

volatile sig_atomic_t signal_received = 0;

void signalHandler(int signum) {
    signal_received = signum;
}

void initializeNcurses() {
    initscr();              // Initialize screen
    cbreak();               // Disable line buffering
    noecho();               // Don't echo input
    keypad(stdscr, TRUE);   // Enable special keys
    timeout(100);           // 100ms timeout for getch()
    curs_set(0);            // Hide cursor
}

void cleanupNcurses() {
    echo();
    nocbreak();
    endwin();
}

void drawProgressBar(int y, int x, int width, float progress) {
    move(y, x);
    addch('[');
    int filled = (int)(width * progress);
    for (int i = 0; i < width; i++) {
        addch(i < filled ? '=' : ' ');
    }
    addch(']');
}

void drawInterface(AudioPlayer& player,
                   const std::vector<std::string>& playlist,
                   size_t currentTrackIndex,
                   bool directoryMode,
                   const std::string& statusMessage) {
    clear();

    namespace fs = std::filesystem;

    // Track info (lines 0-1)
    mvprintw(0, 0, "Now Playing: %s",
             fs::path(playlist[currentTrackIndex]).filename().string().c_str());
    if (directoryMode) {
        mvprintw(1, 0, "Track %zu of %zu", currentTrackIndex + 1, playlist.size());
    }

    // Status bar (line 3)
    const char* icon = player.isPlaying() ? ">" : player.isPaused() ? "||" : "[]";
    int pos = player.getPosition();
    int dur = player.getDuration();
    float vol = player.getVolume();

    mvprintw(3, 0, "[%s] %02d:%02d / %02d:%02d | Volume: %d%% | ",
             icon, pos/60, pos%60, dur/60, dur%60, (int)(vol*100));

    // Progress bar
    float progress = dur > 0 ? (float)pos / dur : 0.0f;
    drawProgressBar(3, 35, 20, progress);

    // Command help (lines 5-6)
    mvprintw(5, 0, "Commands: (p)lay (s)top (u)pause (r)resume (+/-) vol");
    mvprintw(6, 0, "         (f)orward (b)ack (n)ext (i)nfo (h)elp (q)uit");

    // Status message (line 8)
    mvprintw(8, 0, "Status: %s", statusMessage.c_str());

    refresh();
}

std::string handleCommand(int ch,
                          AudioPlayer& player,
                          std::vector<std::string>& playlist,
                          size_t& currentTrackIndex,
                          bool directoryMode,
                          bool& running) {
    namespace fs = std::filesystem;

    switch (ch) {
        case 'q': case 'Q':
            running = false;
            return "Quitting...";
        case 'h': case 'H':
            return "Press keys for commands (no Enter needed)";
        case 'p': case 'P':
            player.play();
            return "Playing...";
        case 's': case 'S':
            player.stop();
            return "Stopped";
        case 'u': case 'U':
            player.pause();
            return "Paused";
        case 'r': case 'R':
            player.play();
            return "Resumed";
        case '+':
            player.setVolume(std::min(1.0f, player.getVolume() + 0.1f));
            return "Volume up";
        case '-':
            player.setVolume(std::max(0.0f, player.getVolume() - 0.1f));
            return "Volume down";
        case 'f': case 'F':
            player.seek(player.getPosition() + 10);
            return "Forward 10s";
        case 'b': case 'B':
            player.seek(std::max(0, player.getPosition() - 10));
            return "Back 10s";
        case 'n': case 'N':
            if (directoryMode && currentTrackIndex < playlist.size() - 1) {
                currentTrackIndex++;
                player.cleanup();
                player.loadFile(playlist[currentTrackIndex]);
                player.play();
                return "Next track";
            }
            return directoryMode ? "Last track" : "Next only in directory mode";
        case 'i': case 'I':
            return "Track info displayed above";
        default:
            return "Unknown command (press h for help)";
    }
}

bool checkAutoAdvance(AudioPlayer& player,
                      std::vector<std::string>& playlist,
                      size_t& currentTrackIndex,
                      bool directoryMode,
                      bool& wasPlaying) {
    if (wasPlaying && !player.isPlaying() && !player.isPaused()) {
        wasPlaying = false;
        if (directoryMode && currentTrackIndex < playlist.size() - 1) {
            currentTrackIndex++;
            player.cleanup();
            if (player.loadFile(playlist[currentTrackIndex])) {
                player.play();
                wasPlaying = true;
                return true;
            }
        }
    } else if (player.isPlaying()) {
        wasPlaying = true;
    }
    return false;
}

std::vector<std::string> scanDirectoryForAudio(const std::string& dirPath) {
    std::vector<std::string> audioFiles;
    const std::vector<std::string> validExtensions = {".wav", ".mp3", ".flac", ".ogg"};

    namespace fs = std::filesystem;

    try {
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            std::cerr << "Error: Directory does not exist: " << dirPath << std::endl;
            return audioFiles;
        }

        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (std::find(validExtensions.begin(), validExtensions.end(), ext) != validExtensions.end()) {
                    audioFiles.push_back(entry.path().string());
                }
            }
        }

        // Sort files alphabetically
        std::sort(audioFiles.begin(), audioFiles.end());

    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }

    return audioFiles;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments using cxxopts
    cxxopts::Options options("cli-player", "CLI Audio Player - Play audio files from command line");

    options.add_options()
        ("d,directory", "Play all audio files in a directory", cxxopts::value<std::string>())
        ("f,file", "Play a single audio file", cxxopts::value<std::string>())
        ("h,help", "Print usage");

    options.parse_positional({"file"});
    options.positional_help("<audio_file>");

    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    // Determine mode and input path
    bool directoryMode = false;
    std::string inputPath;
    std::vector<std::string> playlist;
    size_t currentTrackIndex = 0;

    if (result.count("directory")) {
        // Directory mode
        directoryMode = true;
        inputPath = result["directory"].as<std::string>();
        playlist = scanDirectoryForAudio(inputPath);

        if (playlist.empty()) {
            std::cerr << "No audio files found in directory: " << inputPath << std::endl;
            return 1;
        }

        std::cout << "Found " << playlist.size() << " audio file(s) in directory" << std::endl;
        for (size_t i = 0; i < playlist.size(); ++i) {
            namespace fs = std::filesystem;
            std::cout << "  " << (i + 1) << ". " << fs::path(playlist[i]).filename().string() << std::endl;
        }
    } else if (result.count("file")) {
        // Single file mode
        directoryMode = false;
        inputPath = result["file"].as<std::string>();
        playlist.push_back(inputPath);
    } else {
        std::cerr << "Error: Please specify an audio file or directory" << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Initialize SDL_mixer
    int flags = MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_OGG;
    int initialized = Mix_Init(flags);
    if ((initialized & flags) != flags) {
        std::cerr << "Warning: Some audio formats may not be available: " << Mix_GetError() << std::endl;
    }

    // Open audio device
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer initialization failed: " << Mix_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    AudioPlayer player;

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    atexit(cleanupNcurses);

    // Initialize ncurses
    initializeNcurses();

    // Load first track
    if (!player.loadFile(playlist[currentTrackIndex])) {
        cleanupNcurses();
        Mix_CloseAudio();
        Mix_Quit();
        SDL_Quit();
        return 1;
    }

    // Main event loop
    bool running = true;
    bool wasPlaying = false;
    std::string statusMessage = "Press 'h' for help, 'p' to play";

    while (running && !signal_received) {
        // Draw interface
        drawInterface(player, playlist, currentTrackIndex, directoryMode, statusMessage);

        // Get input (100ms timeout)
        int ch = getch();

        // Process command if key pressed
        if (ch != ERR) {
            statusMessage = handleCommand(ch, player, playlist, currentTrackIndex,
                                         directoryMode, running);
        }

        // Check for auto-advance
        checkAutoAdvance(player, playlist, currentTrackIndex, directoryMode, wasPlaying);
    }

    // Cleanup ncurses
    cleanupNcurses();
    
    player.cleanup();
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
    
    std::cout << "\nGoodbye!" << std::endl;
    return 0;
}

