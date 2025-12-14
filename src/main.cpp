#include "player.h"

#include <csignal>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef __unix__
#include <fcntl.h>
#include <unistd.h>
#endif

#include <cxxopts.hpp>

volatile sig_atomic_t signal_received = 0;

void SignalHandler(int signum)
{
    signal_received = signum;
}

void PrintHelp()
{
    std::cout << "\nCommands:\n"
              << "  p - Play\n"
              << "  s - Stop\n"
              << "  u - Pause\n"
              << "  r - Resume\n"
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
                 const std::vector<std::string> &playlist,
                 size_t current_track_index,
                 bool directory_mode)
{
    namespace fs = std::filesystem;

    int pos = player.GetPosition();
    int dur = player.GetDuration();
    float vol = player.GetVolume();

    std::string state;
    if (player.IsPlaying())
        state = "Playing";
    else if (player.IsPaused())
        state = "Paused";
    else
        state = "Stopped";

    std::cout << "\r[" << state << "] "
              << fs::path(playlist[current_track_index]).filename().string()
              << " | " << std::setfill('0') << std::setw(2) << (pos / 60) << ":"
              << std::setw(2) << (pos % 60) << " / "
              << std::setw(2) << (dur / 60) << ":"
              << std::setw(2) << (dur % 60)
              << " | Vol: " << std::setw(3) << (int)(vol * 100) << "%";

    if (directory_mode)
    {
        std::cout << " | Track " << (current_track_index + 1) << "/" << playlist.size();
    }

    std::cout << "          " << std::flush;
}

std::string HandleCommand(char ch,
                          AudioPlayer &player,
                          std::vector<std::string> &playlist,
                          size_t &current_track_index,
                          bool directory_mode,
                          bool &running)
{
    namespace fs = std::filesystem;

    switch (ch)
    {
    case 'q':
    case 'Q':
        running = false;
        return "Quitting...";
    case 'h':
    case 'H':
        PrintHelp();
        return "Help displayed";
    case 'p':
    case 'P':
        player.Play();
        return "Playing";
    case 's':
    case 'S':
        player.Stop();
        return "Stopped";
    case 'u':
    case 'U':
        player.Pause();
        return "Paused";
    case 'r':
    case 'R':
        player.Play();
        return "Resumed";
    case '+':
        player.SetVolume(std::min(1.0f, player.GetVolume() + 0.1f));
        return "Volume up";
    case '-':
        player.SetVolume(std::max(0.0f, player.GetVolume() - 0.1f));
        return "Volume down";
    case 'f':
    case 'F':
        player.Seek(player.GetPosition() + 10);
        return "Forward 10s";
    case 'b':
    case 'B':
        player.Seek(std::max(0, player.GetPosition() - 10));
        return "Back 10s";
    case 'n':
    case 'N':
        if (directory_mode && current_track_index < playlist.size() - 1)
        {
            current_track_index++;
            player.Cleanup();
            player.LoadFile(playlist[current_track_index]);
            player.Play();
            std::cout << "\n";
            return "Next track";
        }
        return directory_mode ? "Last track" : "Next only in directory mode";
    case '\n':
        return "";
    default:
        return "";
    }
}

bool CheckAutoAdvance(AudioPlayer &player,
                      std::vector<std::string> &playlist,
                      size_t &current_track_index,
                      bool directory_mode,
                      bool &was_playing)
{
    if (was_playing && !player.IsPlaying() && !player.IsPaused())
    {
        was_playing = false;
        if (directory_mode && current_track_index < playlist.size() - 1)
        {
            current_track_index++;
            player.Cleanup();
            if (player.LoadFile(playlist[current_track_index]))
            {
                player.Play();
                was_playing = true;
                std::cout << "\n";
                return true;
            }
        }
    }
    else if (player.IsPlaying())
    {
        was_playing = true;
    }
    return false;
}

std::vector<std::string> ScanDirectoryForAudio(const std::string &dir_path)
{
    std::vector<std::string> audio_files;
    const std::vector<std::string> valid_extensions =
        {".wav", ".mp3", ".flac", ".ogg"};

    namespace fs = std::filesystem;

    try
    {
        if (!fs::exists(dir_path) || !fs::is_directory(dir_path))
        {
            std::cerr << "Error: Directory does not exist: " << dir_path
                      << std::endl;
            return audio_files;
        }

        for (const auto &entry : fs::directory_iterator(dir_path))
        {
            if (entry.is_regular_file())
            {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (std::find(valid_extensions.begin(), valid_extensions.end(), ext) != valid_extensions.end())
                {
                    audio_files.push_back(entry.path().string());
                }
            }
        }

        // Sort files alphabetically
        std::sort(audio_files.begin(), audio_files.end());
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }

    return audio_files;
}

int main(int argc, char *argv[])
{
    // Parse command line arguments using cxxopts
    cxxopts::Options options("cli-player",
                             "CLI Audio Player - Play audio files from command line");

    options.add_options()("d,directory", "Play all audio files in a directory", cxxopts::value<std::string>())
                         ("f,file", "Play a single audio file", cxxopts::value<std::string>())
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

    // Determine mode and input path
    bool directory_mode = false;
    std::string input_path;
    std::vector<std::string> playlist;
    size_t current_track_index = 0;

    if (result.count("directory"))
    {
        // Directory mode
        directory_mode = true;
        input_path = result["directory"].as<std::string>();
        playlist = ScanDirectoryForAudio(input_path);

        if (playlist.empty())
        {
            std::cerr << "No audio files found in directory: " << input_path
                      << std::endl;
            return 1;
        }

        std::cout << "Found " << playlist.size() << " audio file(s) in directory"
                  << std::endl;
        for (size_t i = 0; i < playlist.size(); ++i)
        {
            namespace fs = std::filesystem;
            std::cout << "  " << (i + 1) << ". "
                      << fs::path(playlist[i]).filename().string() << std::endl;
        }
    }
    else if (result.count("file"))
    {
        // Single file mode
        directory_mode = false;
        input_path = result["file"].as<std::string>();
        playlist.push_back(input_path);
    }
    else
    {
        std::cerr << "Error: Please specify an audio file or directory"
                  << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    AudioPlayer player;

    // Setup signal handlers
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // Load first track
    if (!player.LoadFile(playlist[current_track_index]))
    {
        return 1;
    }

    std::cout << "\nCLI Audio Player - Press 'h' for help, 'p' to play\n" << std::endl;

    // Main event loop
    bool running = true;
    bool was_playing = false;

    // Set stdin to non-blocking mode on Unix-like systems
    #ifdef __unix__
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    #endif

    while (running && !signal_received)
    {
        // Print status
        PrintStatus(player, playlist, current_track_index, directory_mode);

        // Check for input
        char ch;
        if (read(STDIN_FILENO, &ch, 1) == 1)
        {
            std::string msg = HandleCommand(ch, player, playlist, current_track_index,
                                           directory_mode, running);
            if (!msg.empty() && ch != '\n')
            {
                std::cout << "\n" << msg << std::endl;
            }
        }

        // Check for auto-advance
        CheckAutoAdvance(player, playlist, current_track_index, directory_mode,
                         was_playing);

        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Restore stdin to blocking mode
    #ifdef __unix__
    fcntl(STDIN_FILENO, F_SETFL, flags);
    #endif

    player.Cleanup();

    std::cout << "\n\nGoodbye!" << std::endl;
    return 0;
}
