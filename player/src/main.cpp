#include "player.h"
#include "metadata.h"
#include "playlist.h"

#include <csignal>
#include <cstring>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <cxxopts.hpp>
#include <colors.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

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
              << "  n - Next track\n"
              << "  h - Help\n"
              << "  q - Quit\n"
              << std::endl;
}

void PrintStatus(AudioPlayer &player, const Playlist &playlist)
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

    const TrackMetadata& track = playlist.current();

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

    if (playlist.size() > 1)
    {
        std::cout << " | Track " << (playlist.currentIndex() + 1) << "/" << playlist.size();
    }

    std::cout << "          " << std::flush;
}

bool HandleCommand(char ch,
                  AudioPlayer &player,
                  Playlist &playlist,
                  bool &running)
{
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
        if (playlist.hasNext())
        {
            playlist.advance();
            player.cleanup();
            player.loadFile(playlist.current().filepath);
            player.play();
        }
        break;
    default:
        break;
    }
    return running;
}

bool CheckAutoAdvance(AudioPlayer &player,
                      Playlist &playlist,
                      bool repeat,
                      bool &was_playing)
{
    if (was_playing && !player.isPlaying() && !player.isPaused())
    {
        was_playing = false;

        // Check if we've reached the end of the playlist
        if (!playlist.hasNext() && !repeat)
        {
            // End of playlist reached, signal to exit
            return true;
        }

        if (repeat && !playlist.hasNext())
        {
            playlist.reset();
        }

        if (playlist.hasNext())
        {
            playlist.advance();
            player.cleanup();
            if (player.loadFile(playlist.current().filepath))
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

std::string readStdin()
{
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

int main(int argc, char *argv[])
{
    // Parse command line arguments using cxxopts
    cxxopts::Options options("vibe-player",
                             "Vibe Player - Play audio playlists");

    options.add_options()
        ("playlist", "Playlist file to play", cxxopts::value<std::string>())
        ("f,file", "Play a single audio file", cxxopts::value<std::string>())
        ("stdin", "Read playlist from stdin")
        ("r,repeat", "Repeat playlist")
        ("no-interactive", "Disable interactive controls (auto-play only)")
        ("h,help", "Print usage");

    options.parse_positional({"playlist"});
    options.positional_help("<playlist_file>");

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

    const bool repeat = result.count("repeat") > 0;
    const bool stdin_mode = result.count("stdin") > 0;
    const bool file_mode = result.count("file") > 0;
    const bool interactive = result.count("no-interactive") == 0;

    // Load playlist
    std::optional<Playlist> playlist_opt;

    if (stdin_mode) {
        // Read from stdin
        std::string content = readStdin();

        // Auto-detect format
        std::istringstream iss(content);
        char first_char = '\0';
        iss >> std::ws;
        if (iss.peek() != EOF) {
            first_char = iss.peek();
        }

        if (first_char == '{' || first_char == '[') {
            // JSON format
            playlist_opt = Playlist::fromJson(content);
        } else {
            // Text format - parse paths
            std::vector<std::string> paths;
            std::istringstream stream(content);
            std::string line;
            while (std::getline(stream, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                if (!line.empty() && line[0] != '#') {
                    paths.push_back(line);
                }
            }
            playlist_opt = Playlist::fromPaths(paths);
        }

        if (!playlist_opt) {
            std::cerr << "Error: Failed to parse playlist from stdin" << std::endl;
            return 1;
        }

        // Reopen stdin to /dev/tty for keyboard input in interactive mode
        if (interactive) {
            if (!freopen("/dev/tty", "r", stdin)) {
                std::cerr << "Warning: Could not reopen stdin for keyboard input" << std::endl;
            }
        }
    } else if (file_mode) {
        // Single file mode
        std::string filepath = result["file"].as<std::string>();
        auto metadata = MetadataExtractor::extract(filepath, false);
        if (!metadata) {
            std::cerr << "Error: Failed to extract metadata from file: " << filepath << std::endl;
            return 1;
        }
        playlist_opt = Playlist::fromTracks({*metadata});
    } else if (result.count("playlist")) {
        // Playlist file mode
        std::string playlist_file = result["playlist"].as<std::string>();
        playlist_opt = Playlist::fromFile(playlist_file);
        if (!playlist_opt) {
            std::cerr << "Error: Failed to load playlist from file: " << playlist_file << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Error: Please specify a playlist file, --stdin, or --file" << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    Playlist playlist = *playlist_opt;

    // Extract metadata for all tracks upfront (for text-based playlists)
    playlist.extractAllMetadata();

    if (playlist.empty()) {
        std::cerr << "Error: Playlist is empty" << std::endl;
        return 1;
    }

    AudioPlayer player;

    // Setup signal handlers
    atexit(Cleanup);
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // Load first track
    if (!player.loadFile(playlist.current().filepath))
    {
        return 1;
    }

    std::cout << "\nVibe Player - " << playlist.size() << " track(s)";
    if (interactive) {
        std::cout << " - Press 'h' for help, 'p' to play\n" << std::endl;
    } else {
        std::cout << " - Auto-play mode\n" << std::endl;
    }

    // Main event loop
    bool running = true;
    bool was_playing = false;

    if (interactive) {
        set_raw_mode(true);
    }

    // Start playing automatically
    player.play();
    was_playing = true;

    while (running && !signal_received)
    {
        // Print status
        PrintStatus(player, playlist);

        // Check for input (only in interactive mode)
        if (interactive) {
            char ch;
            if ((ch = quick_read()) != ERR)
            {
                HandleCommand(ch, player, playlist, running);
            }
        }

        // Check for auto-advance and playlist end
        if (CheckAutoAdvance(player, playlist, repeat, was_playing))
        {
            // Playlist has ended, exit
            running = false;
        }

        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl;

    // Restore stdin to blocking mode
    if (interactive) {
        set_raw_mode(false);
    }

    player.cleanup();

    return 0;
}
