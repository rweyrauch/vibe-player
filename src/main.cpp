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

void printHelp() {
    std::cout << "CLI Audio Player\n";
    std::cout << "Usage: cli-player <audio_file>\n";
    std::cout << "       cli-player -d <directory>\n";
    std::cout << "       cli-player --directory <directory>\n";
    std::cout << "\nCommands:\n";
    std::cout << "  p, play     - Play audio\n";
    std::cout << "  s, stop     - Stop audio\n";
    std::cout << "  u, pause    - Pause audio\n";
    std::cout << "  r, resume   - Resume audio\n";
    std::cout << "  n, next     - Play next track (directory mode)\n";
    std::cout << "  +, volup    - Increase volume\n";
    std::cout << "  -, voldown   - Decrease volume\n";
    std::cout << "  f, forward  - Forward 10 seconds\n";
    std::cout << "  b, backward - Backward 10 seconds\n";
    std::cout << "  i, info     - Show track info\n";
    std::cout << "  h, help     - Show this help\n";
    std::cout << "  q, quit     - Quit player\n";
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

std::string formatTime(int seconds) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ":" 
        << std::setfill('0') << std::setw(2) << secs;
    return oss.str();
}

void printStatus(AudioPlayer& player) {
    int position = player.getPosition();
    int duration = player.getDuration();
    float vol = player.getVolume();
    
    std::cout << "\r[" << (player.isPlaying() ? "▶" : player.isPaused() ? "⏸" : "⏹") << "] "
              << formatTime(position) << " / " << formatTime(duration) 
              << " | Volume: " << std::fixed << std::setprecision(0) << (vol * 100) << "%"
              << std::flush;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <audio_file>" << std::endl;
        std::cerr << "       " << argv[0] << " -d <directory>" << std::endl;
        std::cerr << "       " << argv[0] << " --directory <directory>" << std::endl;
        return 1;
    }

    // Parse command line arguments
    bool directoryMode = false;
    std::string inputPath;
    std::vector<std::string> playlist;
    size_t currentTrackIndex = 0;

    if (argc == 3 && (std::strcmp(argv[1], "-d") == 0 || std::strcmp(argv[1], "--directory") == 0)) {
        // Directory mode
        directoryMode = true;
        inputPath = argv[2];
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
    } else {
        // Single file mode
        directoryMode = false;
        inputPath = argv[1];
        playlist.push_back(inputPath);
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

    if (!player.loadFile(playlist[currentTrackIndex])) {
        SDL_Quit();
        return 1;
    }

    namespace fs = std::filesystem;
    std::cout << "\nNow playing: " << fs::path(playlist[currentTrackIndex]).filename().string() << std::endl;
    if (directoryMode) {
        std::cout << "Track " << (currentTrackIndex + 1) << " of " << playlist.size() << std::endl;
    }
    std::cout << "Type 'h' or 'help' for commands" << std::endl;

    // Start status update thread
    bool running = true;
    bool wasPlaying = false;
    std::thread statusThread([&]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (player.isPlaying() || player.isPaused()) {
                printStatus(player);
                wasPlaying = true;
            } else if (wasPlaying && directoryMode && currentTrackIndex < playlist.size() - 1) {
                // Track finished, auto-advance to next track
                wasPlaying = false;
                currentTrackIndex++;
                player.cleanup();
                if (player.loadFile(playlist[currentTrackIndex])) {
                    std::cout << "\n\nNow playing: " << fs::path(playlist[currentTrackIndex]).filename().string() << std::endl;
                    std::cout << "Track " << (currentTrackIndex + 1) << " of " << playlist.size() << std::endl;
                    player.play();
                }
            } else if (wasPlaying) {
                wasPlaying = false;
            }
        }
    });

    // Command loop
    std::string command;
    while (running) {
        std::cout << "\n> ";
        std::getline(std::cin, command);

        // Convert to lowercase
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);

        if (command == "q" || command == "quit") {
            running = false;
            break;
        } else if (command == "h" || command == "help") {
            printHelp();
        } else if (command == "p" || command == "play") {
            player.play();
            std::cout << "Playing..." << std::endl;
        } else if (command == "s" || command == "stop") {
            player.stop();
            std::cout << "Stopped" << std::endl;
        } else if (command == "u" || command == "pause") {
            player.pause();
            std::cout << "Paused" << std::endl;
        } else if (command == "r" || command == "resume") {
            player.play();
            std::cout << "Resumed" << std::endl;
        } else if (command == "+" || command == "volup") {
            float vol = player.getVolume();
            player.setVolume(std::min(1.0f, vol + 0.1f));
            std::cout << "Volume: " << (int)(player.getVolume() * 100) << "%" << std::endl;
        } else if (command == "-" || command == "voldown") {
            float vol = player.getVolume();
            player.setVolume(std::max(0.0f, vol - 0.1f));
            std::cout << "Volume: " << (int)(player.getVolume() * 100) << "%" << std::endl;
        } else if (command == "f" || command == "forward") {
            int pos = player.getPosition();
            player.seek(pos + 10);
            std::cout << "Forwarded 10 seconds" << std::endl;
        } else if (command == "b" || command == "backward") {
            int pos = player.getPosition();
            player.seek(std::max(0, pos - 10));
            std::cout << "Rewinded 10 seconds" << std::endl;
        } else if (command == "n" || command == "next") {
            if (directoryMode && currentTrackIndex < playlist.size() - 1) {
                currentTrackIndex++;
                player.cleanup();
                if (player.loadFile(playlist[currentTrackIndex])) {
                    std::cout << "\nNow playing: " << fs::path(playlist[currentTrackIndex]).filename().string() << std::endl;
                    std::cout << "Track " << (currentTrackIndex + 1) << " of " << playlist.size() << std::endl;
                    player.play();
                }
            } else if (directoryMode) {
                std::cout << "Already at the last track" << std::endl;
            } else {
                std::cout << "Next command only available in directory mode" << std::endl;
            }
        } else if (command == "i" || command == "info") {
            std::cout << "\nTrack Info:" << std::endl;
            std::cout << "  File: " << fs::path(playlist[currentTrackIndex]).filename().string() << std::endl;
            if (directoryMode) {
                std::cout << "  Track: " << (currentTrackIndex + 1) << " of " << playlist.size() << std::endl;
            }
            std::cout << "  Duration: " << formatTime(player.getDuration()) << std::endl;
            std::cout << "  Position: " << formatTime(player.getPosition()) << std::endl;
            std::cout << "  Volume: " << (int)(player.getVolume() * 100) << "%" << std::endl;
            std::cout << "  Status: " << (player.isPlaying() ? "Playing" :
                                         player.isPaused() ? "Paused" : "Stopped") << std::endl;
        } else if (!command.empty()) {
            std::cout << "Unknown command. Type 'h' for help." << std::endl;
        }
    }

    running = false;
    statusThread.join();
    
    player.cleanup();
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
    
    std::cout << "\nGoodbye!" << std::endl;
    return 0;
}

