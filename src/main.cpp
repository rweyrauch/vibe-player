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

#include <ncurses.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <cxxopts.hpp>

volatile sig_atomic_t signal_received = 0;

void SignalHandler(int signum)
{
    signal_received = signum;
}

void InitializeNcurses()
{
    initscr();            // Initialize screen
    cbreak();             // Disable line buffering
    noecho();             // Don't echo input
    keypad(stdscr, TRUE); // Enable special keys
    timeout(100);         // 100ms timeout for getch()
    curs_set(0);          // Hide cursor
}

void CleanupNcurses()
{
    echo();
    nocbreak();
    endwin();
}

void DrawProgressBar(int y, int x, int width, float progress)
{
    move(y, x);
    addch('[');
    int filled = (int)(width * progress);
    for (int i = 0; i < width; i++)
    {
        addch(i < filled ? '=' : ' ');
    }
    addch(']');
}

void DrawBar(int y, int x, int width, float level)
{
    move(y, x);
    addch('[');
    int filled = (int)(width * level);
    for (int i = 0; i < width; i++)
    {
        addch(i < filled ? '#' : ' ');
    }
    addch(']');
}

void DrawFrequencyBars(int y, AudioPlayer &player)
{
    float bass, mid, treble;
    player.GetFrequencyLevels(bass, mid, treble);

    // Draw label and bars
    mvprintw(y, 0, "Frequency: ");
    DrawBar(y, 11, 8, bass);
    DrawBar(y, 22, 8, mid);
    DrawBar(y, 33, 8, treble);

    // Draw frequency band labels
    mvprintw(y + 1, 11, "Bass");
    mvprintw(y + 1, 22, "Mid");
    mvprintw(y + 1, 33, "Treble");
}

void DrawInterface(AudioPlayer &player,
                   const std::vector<std::string> &playlist,
                   size_t current_track_index,
                   bool directory_mode,
                   const std::string &status_message)
{
    clear();

    namespace fs = std::filesystem;

    // Track info (lines 0-1)
    mvprintw(0, 0, "Now Playing: %s",
             fs::path(playlist[current_track_index]).filename().string().c_str());
    if (directory_mode)
    {
        mvprintw(1, 0, "Track %zu of %zu",
                 current_track_index + 1, playlist.size());
    }

    // Status bar (line 3)
    const char *icon = player.IsPlaying() ? ">" : player.IsPaused() ? "||"
                                                                    : "[]";
    int pos = player.GetPosition();
    int dur = player.GetDuration();
    float vol = player.GetVolume();

    mvprintw(3, 0, "[%s] %02d:%02d / %02d:%02d | Volume: %d%% | ",
             icon, pos / 60, pos % 60, dur / 60, dur % 60, (int)(vol * 100));

    // Progress bar
    float progress = dur > 0 ? (float)pos / dur : 0.0f;
    DrawProgressBar(3, 35, 20, progress);

    // Frequency visualization (lines 5-6)
    DrawFrequencyBars(5, player);

    // Command help (lines 8-9)
    mvprintw(8, 0, "Commands: (p)lay (s)top (u)pause (r)resume (+/-) vol");
    mvprintw(9, 0, "         (f)orward (b)ack (n)ext (i)nfo (h)elp (q)uit");

    // Status message (line 11)
    mvprintw(11, 0, "Status: %s", status_message.c_str());

    refresh();
}

std::string HandleCommand(int ch,
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
        return "Press keys for commands (no Enter needed)";
    case 'p':
    case 'P':
        player.Play();
        return "Playing...";
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
            return "Next track";
        }
        return directory_mode ? "Last track" : "Next only in directory mode";
    case 'i':
    case 'I':
        return "Track info displayed above";
    default:
        return "Unknown command (press h for help)";
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

    options.add_options()("d,directory", "Play all audio files in a directory",
                          cxxopts::value<std::string>())("f,file", "Play a single audio file", cxxopts::value<std::string>())("h,help", "Print usage");

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

    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Initialize SDL_mixer
    int flags = MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_OGG;
    int initialized = Mix_Init(flags);
    if ((initialized & flags) != flags)
    {
        std::cerr << "Warning: Some audio formats may not be available: "
                  << Mix_GetError() << std::endl;
    }

    // Open audio device
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        std::cerr << "SDL_mixer initialization failed: " << Mix_GetError()
                  << std::endl;
        SDL_Quit();
        return 1;
    }

    AudioPlayer player;

    // Setup signal handlers
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    atexit(CleanupNcurses);

    // Initialize ncurses
    InitializeNcurses();

    // Load first track
    if (!player.LoadFile(playlist[current_track_index]))
    {
        CleanupNcurses();
        Mix_CloseAudio();
        Mix_Quit();
        SDL_Quit();
        return 1;
    }

    // Main event loop
    bool running = true;
    bool was_playing = false;
    std::string status_message = "Press 'h' for help, 'p' to play";

    while (running && !signal_received)
    {
        // Draw interface
        DrawInterface(player, playlist, current_track_index, directory_mode,
                      status_message);

        // Get input (100ms timeout)
        int ch = getch();

        // Process command if key pressed
        if (ch != ERR)
        {
            status_message = HandleCommand(ch, player, playlist, current_track_index,
                                           directory_mode, running);
        }

        // Check for auto-advance
        CheckAutoAdvance(player, playlist, current_track_index, directory_mode,
                         was_playing);
    }

    // Cleanup ncurses
    CleanupNcurses();

    player.Cleanup();
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();

    std::cout << "\nGoodbye!" << std::endl;
    return 0;
}
