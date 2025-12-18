#include "player.h"

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

volatile sig_atomic_t signal_received = 0;

void SignalHandler(int signum)
{
    signal_received = signum;
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
                 const std::vector<std::string> &playlist,
                 size_t current_track_index,
                 bool directory_mode)
{
    namespace fs = std::filesystem;

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

    std::cout << "\r[" << state << "] "
              << fs::path(playlist[current_track_index]).filename().string()
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
                          std::vector<std::string> &playlist,
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
            player.loadFile(playlist[current_track_index]);
            player.play();
        }
        break;
    default:
        break;
    }
    return running;
}

bool CheckAutoAdvance(AudioPlayer &player,
                      std::vector<std::string> &playlist,
                      size_t &current_track_index,
                      bool directory_mode,
                      bool repeat,
                      bool &was_playing)
{
    if (was_playing && !player.isPlaying() && !player.isPaused())
    {
        was_playing = false;
        if (repeat && directory_mode && (current_track_index == playlist.size() - 1))
        {
            current_track_index = 0;
        }

        if (directory_mode && (current_track_index < playlist.size() - 1))
        {
            current_track_index++;
            player.cleanup();
            if (player.loadFile(playlist[current_track_index]))
            {
                player.play();
                was_playing = true;
                std::cout << "\n";
                return true;
            }
        }
    }
    else if (player.isPlaying())
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
            std::cerr << "Error: Directory does not exist: " << dir_path << std::endl;
            return audio_files;
        }

        for (const auto &entry : fs::recursive_directory_iterator(dir_path))
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
        std::cerr << "Error: Please specify an audio file or directory" << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    AudioPlayer player;

    // Setup signal handlers
    atexit(Cleanup);
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // Load first track
    if (!player.loadFile(playlist[current_track_index]))
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

        // Check for auto-advance
        CheckAutoAdvance(player, playlist, current_track_index, directory_mode, repeat, was_playing);

        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl;

    // Restore stdin to blocking mode
    set_raw_mode(false);

    player.cleanup();

    return 0;
}
