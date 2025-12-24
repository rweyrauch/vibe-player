/*
 * vibe-player
 * main.cpp
 */

#include "player.h"
#include "metadata.h"
#include "playlist.h"
#include "path_handler.h"
#include "dropbox_state.h"

#include <csignal>
#include <cstring>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <cxxopts.hpp>
#include <notcurses/notcurses.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

// TagLib includes for album art extraction
#include <mpeg/mpegfile.h>
#include <mpeg/id3v2/id3v2tag.h>
#include <mpeg/id3v2/id3v2frame.h>
#include <mpeg/id3v2/frames/attachedpictureframe.h>
#include <flac/flacfile.h>
#include <flac/flacpicture.h>
#include <mp4/mp4file.h>
#include <mp4/mp4tag.h>
#include <mp4/mp4coverart.h>

volatile sig_atomic_t signal_received = 0;
struct notcurses* nc = nullptr;

void SignalHandler(int signum)
{
    signal_received = signum;
}

void Cleanup()
{
    if (nc)
    {
        // Clear the screen before stopping
        struct ncplane* stdplane = notcurses_stdplane(nc);
        ncplane_erase(stdplane);
        notcurses_render(nc);

        notcurses_stop(nc);
        nc = nullptr;
    }

    // Additional terminal cleanup
    std::cout << "\033[2J\033[H" << std::flush;  // Clear screen and move cursor to home
}

std::string TruncateString(const std::string &str, size_t maxLength)
{
    // If the string is already short enough, return it as-is
    if (str.length() <= maxLength)
    {
        return str;
    }

    // If maxLength is too small to include ellipsis, just return the truncated string
    if (maxLength < 3)
    {
        return str.substr(0, maxLength);
    }

    // Truncate and append ellipsis
    return str.substr(0, maxLength - 3) + "...";
}

std::string FixFieldString(const std::string &str, size_t maxLength, char padChar = ' ')
{
    // If the string is longer than maxLength, truncate it
    if (str.length() > maxLength)
    {
        // If maxLength is too small to include ellipsis, just return the truncated string
        if (maxLength < 3)
        {
            return str.substr(0, maxLength);
        }

        // Truncate and append ellipsis
        return str.substr(0, maxLength - 3) + "...";
    }

    // If the string is shorter than maxLength, pad it
    if (str.length() < maxLength)
    {
        return str + std::string(maxLength - str.length(), padChar);
    }

    // If the string is exactly maxLength, return it as-is
    return str;
}

void InitializeLogger(bool verbose)
{
    namespace fs = std::filesystem;

    // Create log directory if it doesn't exist
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    fs::path log_dir = fs::path(home) / ".cache" / "tui-player";

    try
    {
        fs::create_directories(log_dir);

        // Create file logger
        auto log_path = log_dir / "tui-player.log";
        auto logger = spdlog::basic_logger_mt("tui-player", log_path.string());
        spdlog::set_default_logger(logger);

        // Set log level based on verbose flag
        if (verbose)
        {
            spdlog::set_level(spdlog::level::debug);
            spdlog::info("Verbose logging enabled");
        }
        else
        {
            spdlog::set_level(spdlog::level::info);
        }

        spdlog::info("TUI Player started");
        spdlog::info("Log file: {}", log_path.string());
    }
    catch (const std::exception &e)
    {
        std::cerr << "Warning: Failed to initialize logger: " << e.what() << std::endl;
    }
}

// Extract album art from audio file and save to temporary file
std::string ExtractAlbumArt(const std::string &filepath)
{
    namespace fs = std::filesystem;
    std::string ext = fs::path(filepath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::vector<unsigned char> image_data;
    std::string mime_type;

    // Try MP3/ID3v2
    if (ext == ".mp3")
    {
        TagLib::MPEG::File file(filepath.c_str());
        if (file.isValid() && file.ID3v2Tag())
        {
            auto frameList = file.ID3v2Tag()->frameList("APIC");
            if (!frameList.isEmpty())
            {
                auto *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frameList.front());
                if (frame)
                {
                    auto picture = frame->picture();
                    image_data.assign(picture.data(), picture.data() + picture.size());
                    mime_type = frame->mimeType().to8Bit();
                }
            }
        }
    }
    // Try FLAC
    else if (ext == ".flac")
    {
        TagLib::FLAC::File file(filepath.c_str());
        if (file.isValid())
        {
            auto pictureList = file.pictureList();
            if (!pictureList.isEmpty())
            {
                auto picture = pictureList.front();
                auto data = picture->data();
                image_data.assign(data.data(), data.data() + data.size());
                mime_type = picture->mimeType().to8Bit();
            }
        }
    }
    // Try MP4/M4A
    else if (ext == ".m4a" || ext == ".mp4")
    {
        TagLib::MP4::File file(filepath.c_str());
        if (file.isValid() && file.tag())
        {
            auto itemMap = file.tag()->itemMap();
            if (itemMap.contains("covr"))
            {
                auto coverList = itemMap["covr"].toCoverArtList();
                if (!coverList.isEmpty())
                {
                    auto cover = coverList.front();
                    auto data = cover.data();
                    image_data.assign(data.data(), data.data() + data.size());

                    // Determine extension from format
                    switch (cover.format())
                    {
                        case TagLib::MP4::CoverArt::JPEG:
                            mime_type = "image/jpeg";
                            break;
                        case TagLib::MP4::CoverArt::PNG:
                            mime_type = "image/png";
                            break;
                        default:
                            mime_type = "image/jpeg";
                            break;
                    }
                }
            }
        }
    }

    // If no album art found, return empty string
    if (image_data.empty())
    {
        return "";
    }

    // Determine file extension from MIME type
    std::string file_ext = ".jpg";
    if (mime_type == "image/png")
    {
        file_ext = ".png";
    }

    // Save to temporary file
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    fs::path temp_path = fs::path(home) / ".cache" / "tui-player" / ("album_art" + file_ext);

    try
    {
        std::ofstream out(temp_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());
        out.close();

        spdlog::debug("Extracted album art to: {}", temp_path.string());
        return temp_path.string();
    }
    catch (const std::exception &e)
    {
        spdlog::error("Failed to save album art: {}", e.what());
        return "";
    }
}

// Update only the dynamic status information (time, progress, volume, state)
void DrawStatusUpdate(struct ncplane* status_plane, AudioPlayer &player, const Playlist &playlist)
{
    // If status_plane is null (terminal too small), skip drawing
    if (!status_plane)
    {
        return;
    }

    // Get dimensions for centering
    unsigned int rows, cols;
    ncplane_dim_yx(status_plane, &rows, &cols);
    unsigned int term_cols;
    ncplane_dim_yx(ncplane_parent(status_plane), nullptr, &term_cols);

    // Calculate max width for centering (use 80% of terminal width or less)
    int max_status_width = std::min(static_cast<int>(term_cols * 0.8), 80);

    // Get player state
    int pos = player.getPosition();
    int dur = player.getDuration();
    float vol = player.getVolume();

    // State indicator with sophisticated colors
    std::string state;
    uint32_t state_color;
    if (player.isPlaying())
    {
        state = "▶ Playing";
        state_color = 0x98D8C8; // Soft mint green
    }
    else if (player.isPaused())
    {
        state = "⏸ Paused";
        state_color = 0xF4BF75; // Warm amber
    }
    else
    {
        state = "⏹ Stopped";
        state_color = 0xF09A8A; // Soft coral
    }

    // Clear only lines 4-6 (state, progress, volume - the dynamic parts)
    ncplane_putstr_yx(status_plane, 4, 0, std::string(term_cols, ' ').c_str());
    ncplane_putstr_yx(status_plane, 5, 0, std::string(term_cols, ' ').c_str());
    ncplane_putstr_yx(status_plane, 6, 0, std::string(term_cols, ' ').c_str());

    // Line 4: State and time
    char time_buf[32];
    snprintf(time_buf, sizeof(time_buf), "  %02d:%02d / %02d:%02d",
             pos / 60000, (pos / 1000) % 60,
             (dur / 1000) / 60, (dur / 1000) % 60);
    std::string state_line = state + time_buf;
    int state_x = (term_cols - state_line.length()) / 2;

    ncplane_set_fg_rgb8(status_plane, (state_color >> 16) & 0xFF,
                        (state_color >> 8) & 0xFF, state_color & 0xFF);
    ncplane_set_styles(status_plane, NCSTYLE_BOLD);
    ncplane_putstr_yx(status_plane, 4, state_x, state.c_str());
    ncplane_set_styles(status_plane, NCSTYLE_NONE);
    ncplane_set_fg_rgb8(status_plane, 0x7F, 0xC8, 0xA0); // Sea green
    ncplane_putstr(status_plane, time_buf);

    // Progress bar (line 5) - centered with max width
    int progress_width = std::min(max_status_width, static_cast<int>(term_cols - 4));
    if (progress_width > 0)
    {
        float progress = dur > 0 ? static_cast<float>(pos) / dur : 0.0f;
        int filled = static_cast<int>(progress * (progress_width - 2));
        int progress_x = (term_cols - progress_width) / 2;

        ncplane_set_fg_rgb8(status_plane, 0x7F, 0xC8, 0xA0); // Sea green
        ncplane_putstr_yx(status_plane, 5, progress_x, "[");
        for (int i = 0; i < progress_width - 2; ++i)
        {
            if (i < filled)
            {
                ncplane_set_fg_rgb8(status_plane, 0x7F, 0xC8, 0xA0); // Sea green
                ncplane_putstr(status_plane, "━");
            }
            else if (i == filled)
            {
                ncplane_set_fg_rgb8(status_plane, 0x98, 0xD8, 0xC8); // Lighter mint
                ncplane_putstr(status_plane, "▶");
            }
            else
            {
                ncplane_set_fg_rgb8(status_plane, 0x50, 0x50, 0x48); // Dark warm gray
                ncplane_putstr(status_plane, "─");
            }
        }
        ncplane_set_fg_rgb8(status_plane, 0x7F, 0xC8, 0xA0); // Sea green
        ncplane_putstr(status_plane, "]");
    }

    // Line 6: Volume and track info - centered
    char info_buf[64];
    if (playlist.size() > 1)
    {
        snprintf(info_buf, sizeof(info_buf), "Volume: %3d%%  |  Track %zu of %zu",
                 (int)(vol * 100), playlist.currentIndex() + 1, playlist.size());
    }
    else
    {
        snprintf(info_buf, sizeof(info_buf), "Volume: %3d%%", (int)(vol * 100));
    }
    int info_x = (term_cols - strlen(info_buf)) / 2;
    ncplane_set_fg_rgb8(status_plane, 0xC4, 0xA7, 0xD6); // Soft purple
    ncplane_putstr_yx(status_plane, 6, info_x, info_buf);
}

void DrawUI(struct ncplane* stdplane, struct ncplane* status_plane, struct ncplane* help_plane,
            struct ncplane* art_plane, struct ncvisual* album_art_visual,
            AudioPlayer &player, const Playlist &playlist, bool show_help)
{
    // If planes are null (terminal too small), skip drawing
    if (!status_plane || !help_plane || !art_plane)
    {
        return;
    }

    // Clear planes
    ncplane_erase(stdplane);
    ncplane_erase(status_plane);
    ncplane_erase(help_plane);
    ncplane_erase(art_plane);

    // Get dimensions
    unsigned int rows, cols;
    ncplane_dim_yx(stdplane, &rows, &cols);

    // Draw title bar
    ncplane_set_fg_rgb8(stdplane, 0x8F, 0xC8, 0xD8); // Soft sky blue
    ncplane_set_styles(stdplane, NCSTYLE_BOLD);
    std::string title = " TUI Player ";
    ncplane_putstr_yx(stdplane, 0, (cols - title.length()) / 2, title.c_str());
    ncplane_set_styles(stdplane, NCSTYLE_NONE);

    // Draw album art if available
    if (album_art_visual)
    {
        struct ncvisual_options vopts = {};
        vopts.n = art_plane;
        vopts.scaling = NCSCALE_SCALE;
        vopts.blitter = NCBLIT_2x1;  // Use half-block blitter for better compatibility
        vopts.flags = 0;

        // Render the visual
        struct ncplane* result = ncvisual_blit(nc, album_art_visual, &vopts);
        if (result == nullptr)
        {
            spdlog::warn("Failed to blit album art visual");
        }
    }

    const TrackMetadata &track = playlist.current();

    // Calculate max width for centering (use 80% of terminal width or less)
    int max_status_width = std::min(static_cast<int>(cols * 0.8), 80);

    // Line 0: Song title
    std::string song_label = "Song: ";
    std::string song_value;
    if (track.title)
    {
        song_value = TruncateString(*track.title, max_status_width - song_label.length());
    }
    else
    {
        song_value = TruncateString(track.filename, max_status_width - song_label.length());
    }
    std::string song_line = song_label + song_value;
    int song_x = (cols - song_line.length()) / 2;

    ncplane_set_fg_rgb8(status_plane, 0xF0, 0xF0, 0xE8); // Soft cream
    ncplane_putstr_yx(status_plane, 0, song_x, song_label.c_str());
    ncplane_set_styles(status_plane, NCSTYLE_BOLD);
    ncplane_putstr(status_plane, song_value.c_str());
    ncplane_set_styles(status_plane, NCSTYLE_NONE);

    // Line 1: Artist
    std::string artist_label = "Artist: ";
    std::string artist_value;
    if (track.artist)
    {
        artist_value = TruncateString(*track.artist, max_status_width - artist_label.length());
    }
    else
    {
        artist_value = "Unknown";
    }
    std::string artist_line = artist_label + artist_value;
    int artist_x = (cols - artist_line.length()) / 2;

    ncplane_set_fg_rgb8(status_plane, 0x5F, 0xD4, 0xD4); // Turquoise
    ncplane_putstr_yx(status_plane, 1, artist_x, artist_label.c_str());
    ncplane_set_styles(status_plane, NCSTYLE_BOLD);
    if (track.artist)
    {
        ncplane_putstr(status_plane, artist_value.c_str());
    }
    else
    {
        ncplane_set_fg_rgb8(status_plane, 0xA0, 0xA0, 0x98); // Warm gray
        ncplane_putstr(status_plane, artist_value.c_str());
    }
    ncplane_set_styles(status_plane, NCSTYLE_NONE);

    // Line 2: Album
    std::string album_label = "Album: ";
    std::string album_value;
    if (track.album)
    {
        album_value = TruncateString(*track.album, max_status_width - album_label.length());
    }
    else
    {
        album_value = "Unknown";
    }
    std::string album_line = album_label + album_value;
    int album_x = (cols - album_line.length()) / 2;

    ncplane_set_fg_rgb8(status_plane, 0xB4, 0xA7, 0xD6); // Soft lavender
    ncplane_putstr_yx(status_plane, 2, album_x, album_label.c_str());
    ncplane_set_styles(status_plane, NCSTYLE_BOLD);
    if (track.album)
    {
        ncplane_putstr(status_plane, album_value.c_str());
    }
    else
    {
        ncplane_set_fg_rgb8(status_plane, 0xA0, 0xA0, 0x98); // Warm gray
        ncplane_putstr(status_plane, album_value.c_str());
    }
    ncplane_set_styles(status_plane, NCSTYLE_NONE);

    // Line 3: Empty line for spacing

    // Lines 4-6: Dynamic status (state, progress, volume) - drawn by DrawStatusUpdate
    DrawStatusUpdate(status_plane, player, playlist);

    // Help text
    if (show_help)
    {
        ncplane_set_fg_rgb8(help_plane, 0xF0, 0xF0, 0xE8); // Soft cream
        ncplane_set_styles(help_plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(help_plane, 0, 2, "Keyboard Controls:");
        ncplane_set_styles(help_plane, NCSTYLE_NONE);

        ncplane_set_fg_rgb8(help_plane, 0xD4, 0xC8, 0xA8); // Warm beige
        const char* help_lines[] = {
            "  Space   - Play/Pause         f/Right - Forward 10s",
            "  s       - Stop               b/Left  - Back 10s",
            "  u       - Pause              n       - Next track",
            "  +/=/Up  - Volume up          p       - Previous track",
            "  -/Down  - Volume down        h       - Toggle help",
            "  q       - Quit"
        };

        for (size_t i = 0; i < sizeof(help_lines) / sizeof(help_lines[0]); ++i)
        {
            ncplane_putstr_yx(help_plane, i + 2, 2, help_lines[i]);
        }
    }
    else
    {
        ncplane_set_fg_rgb8(help_plane, 0xA0, 0xA0, 0x98); // Warm gray
        ncplane_putstr_yx(help_plane, 0, 2, "Press 'h' for help, 'q' to quit");
    }
}

bool HandleCommand(char32_t ch,
                   AudioPlayer &player,
                   Playlist &playlist,
                   bool &running,
                   bool &show_help)
{
    switch (ch)
    {
    case 'q':
    case 'Q':
        running = false;
        break;
    case 'h':
    case 'H':
        show_help = !show_help;
        break;
    case 's':
    case 'S':
        player.stop();
        break;
    case 'u':
    case 'U':
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
    case '=':
    case NCKEY_UP:
        player.setVolume(std::min(1.0f, player.getVolume() + 0.05f));
        break;
    case '-':
    case '_':
    case NCKEY_DOWN:
        player.setVolume(std::max(0.0f, player.getVolume() - 0.05f));
        break;
    case 'f':
    case 'F':
    case NCKEY_RIGHT:
        player.seek(player.getPosition() + 10 * 1000);
        break;
    case 'b':
    case 'B':
    case NCKEY_LEFT:
        player.seek(std::max(0LL, player.getPosition() - 10 * 1000LL));
        break;
    case 'n':
    case 'N':
        if (playlist.hasNext())
        {
            playlist.advance();
            player.cleanup();
            player.loadFile(playlist.current().filepath);
            player.play();
        }
        break;
    case 'p':
    case 'P':
        if (playlist.hasPrevious())
        {
            playlist.previous();
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
    // Register cleanup handler
    std::atexit(cleanupDropboxSupport);

    // Parse command line arguments using cxxopts
    cxxopts::Options options("tui-player",
                             "TUI Player - Play audio playlists");

    // clang-format off
    options.add_options()
        ("playlist", "Playlist file to play", cxxopts::value<std::string>())
        ("f,file", "Play a single audio file", cxxopts::value<std::string>())
        ("stdin", "Read playlist from stdin")
        ("r,repeat", "Repeat playlist")
        ("no-interactive", "Disable interactive controls (auto-play only)")
        ("verbose", "Display status and debug information")
        ("h,help", "Print usage");
    // clang-format on

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
        return EXIT_FAILURE;
    }

    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        return EXIT_SUCCESS;
    }

    const bool repeat = result.count("repeat") > 0;
    const bool stdin_mode = result.count("stdin") > 0;
    const bool file_mode = result.count("file") > 0;
    const bool interactive = result.count("no-interactive") == 0;
    const bool verbose = result.count("verbose") > 0;

    // Initialize logger
    InitializeLogger(verbose);

    // Load playlist
    std::optional<Playlist> playlist_opt;

    if (stdin_mode)
    {
        // Read from stdin
        std::string content = readStdin();

        // Text format - parse paths
        std::vector<std::string> paths;
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line))
        {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (!line.empty() && line[0] != '#')
            {
                paths.push_back(line);
            }
        }
        playlist_opt = Playlist::fromPaths(paths);

        if (!playlist_opt)
        {
            std::cerr << "Error: Failed to parse playlist from stdin" << std::endl;
            return EXIT_FAILURE;
        }

        // Reopen stdin to /dev/tty for keyboard input in interactive mode
        if (interactive)
        {
            if (!freopen("/dev/tty", "r", stdin))
            {
                std::cerr << "Warning: Could not reopen stdin for keyboard input" << std::endl;
            }
        }
    }
    else if (file_mode)
    {
        // Single file mode
        std::string filepath = result["file"].as<std::string>();
        auto metadata = MetadataExtractor::extract(filepath, false);
        if (!metadata)
        {
            std::cerr << "Error: Failed to extract metadata from file: " << filepath << std::endl;
            return EXIT_FAILURE;
        }
        playlist_opt = Playlist::fromTracks({*metadata});
    }
    else if (result.count("playlist"))
    {
        // Playlist file mode
        std::string playlist_file = result["playlist"].as<std::string>();
        playlist_opt = Playlist::fromFile(playlist_file);
        if (!playlist_opt)
        {
            std::cerr << "Error: Failed to load playlist from file: " << playlist_file << std::endl;
            return EXIT_FAILURE;
        }
    }
    else
    {
        std::cerr << "Error: Please specify a playlist file, --stdin, or --file" << std::endl;
        std::cout << options.help() << std::endl;
        return EXIT_FAILURE;
    }

    Playlist playlist = *playlist_opt;

    // Extract metadata for all tracks upfront (for text-based playlists)
    playlist.extractAllMetadata();

    if (playlist.empty())
    {
        std::cerr << "Error: Playlist is empty" << std::endl;
        return EXIT_FAILURE;
    }

    // Initialize Dropbox if playlist contains Dropbox files
    bool has_dropbox = false;
    for (const auto& path : playlist.paths())
    {
        if (PathHandler::isDropboxPath(path))
        {
            has_dropbox = true;
            break;
        }
    }

    if (has_dropbox)
    {
        const char* token = std::getenv("DROPBOX_ACCESS_TOKEN");
        if (!token || strlen(token) == 0)
        {
            std::cerr << "Error: Playlist contains Dropbox files but DROPBOX_ACCESS_TOKEN environment variable not set" << std::endl;
            std::cerr << "Please set your Dropbox access token to play Dropbox files" << std::endl;
            return EXIT_FAILURE;
        }

        try {
            initializeDropboxSupport(token);
        } catch (const std::exception& e) {
            std::cerr << "Error: Failed to initialize Dropbox: " << e.what() << std::endl;
            std::cerr << "Please verify your DROPBOX_ACCESS_TOKEN is valid" << std::endl;
            return EXIT_FAILURE;
        }
    }

    AudioPlayer player;

    // Setup signal handlers
    atexit(Cleanup);
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // Load first track
    if (!player.loadFile(playlist.current().filepath))
    {
        return EXIT_FAILURE;
    }

    // Initialize notcurses only in interactive mode
    if (!interactive)
    {
        std::cout << "\nTUI Player - " << playlist.size() << " track(s)";
        std::cout << " - Auto-play mode\n" << std::endl;

        // Main event loop (non-interactive)
        bool running = true;
        bool was_playing = false;

        // Start playing automatically
        player.play();
        was_playing = true;

        while (running && !signal_received)
        {
            // Check for auto-advance and playlist end
            if (CheckAutoAdvance(player, playlist, repeat, was_playing))
            {
                // Playlist has ended, exit
                running = false;
            }

            // Sleep briefly to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        player.cleanup();
        return EXIT_SUCCESS;
    }

    // Clear screen before starting TUI
    std::cout << "\033[2J\033[H" << std::flush;

    // Initialize notcurses for interactive mode
    notcurses_options opts = {};
    opts.flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_NO_ALTERNATE_SCREEN;
    nc = notcurses_init(&opts, nullptr);
    if (!nc)
    {
        std::cerr << "Error: Failed to initialize notcurses" << std::endl;
        return EXIT_FAILURE;
    }

    struct ncplane* stdplane = notcurses_stdplane(nc);

    // Clear and render initial display
    ncplane_erase(stdplane);
    notcurses_render(nc);
    struct ncplane* status_plane = nullptr;
    struct ncplane* help_plane = nullptr;
    struct ncplane* art_plane = nullptr;

    // Lambda to create/recreate planes
    auto createPlanes = [&]() {
        // Destroy old planes if they exist
        if (status_plane) ncplane_destroy(status_plane);
        if (help_plane) ncplane_destroy(help_plane);
        if (art_plane) ncplane_destroy(art_plane);

        // Reset to nullptr to avoid using stale pointers
        status_plane = nullptr;
        help_plane = nullptr;
        art_plane = nullptr;

        // Get current dimensions
        unsigned int rows, cols;
        ncplane_dim_yx(stdplane, &rows, &cols);

        // Minimum terminal size check
        const unsigned int MIN_ROWS = 15;
        const unsigned int MIN_COLS = 30;

        if (rows < MIN_ROWS || cols < MIN_COLS)
        {
            spdlog::warn("Terminal too small ({}x{}), minimum is {}x{}", cols, rows, MIN_COLS, MIN_ROWS);
            // Display error message on stdplane
            ncplane_erase(stdplane);
            ncplane_set_fg_rgb8(stdplane, 0xFF, 0x80, 0x80);
            std::string error_msg = "Terminal too small!";
            std::string min_msg = "Minimum: " + std::to_string(MIN_COLS) + "x" + std::to_string(MIN_ROWS);
            if (rows >= 2 && cols >= error_msg.length())
            {
                ncplane_putstr_yx(stdplane, rows / 2, (cols - error_msg.length()) / 2, error_msg.c_str());
            }
            if (rows >= 3 && cols >= min_msg.length())
            {
                ncplane_putstr_yx(stdplane, rows / 2 + 1, (cols - min_msg.length()) / 2, min_msg.c_str());
            }
            notcurses_render(nc);
            return;
        }

        // Status plane (7 lines at bottom - 8)
        struct ncplane_options status_opts = {};
        status_opts.y = rows >= 8 ? rows - 8 : 0;
        status_opts.x = 0;
        status_opts.rows = std::min(7u, rows > 0 ? rows - 1 : 1);
        status_opts.cols = cols;
        status_plane = ncplane_create(stdplane, &status_opts);
        if (!status_plane)
        {
            spdlog::error("Failed to create status plane");
            return;
        }

        // Album art plane (centered, larger size for better resolution)
        // Use up to 60 columns or 60% of terminal width, whichever is smaller
        int art_cols = std::min(60, static_cast<int>(cols * 0.6));
        art_cols = std::max(10, art_cols); // Minimum 10 columns

        // Calculate rows to maintain roughly square aspect ratio (each char is ~2:1 height:width)
        int available_rows = static_cast<int>(rows) - 12;
        int art_rows = std::min(static_cast<int>(art_cols / 2), available_rows);
        art_rows = std::max(5, art_rows); // Minimum 5 rows

        struct ncplane_options art_opts = {};
        art_opts.y = 2;
        art_opts.x = cols > static_cast<unsigned>(art_cols) ? (cols - art_cols) / 2 : 0;
        art_opts.rows = art_rows;
        art_opts.cols = art_cols;
        art_plane = ncplane_create(stdplane, &art_opts);
        if (!art_plane)
        {
            spdlog::error("Failed to create art plane");
            return;
        }

        // Help plane (positioned to the right of album art, or below if narrow terminal)
        struct ncplane_options help_opts = {};
        int art_right_edge = art_opts.x + art_cols;
        int space_on_right = static_cast<int>(cols) - art_right_edge;

        if (space_on_right > 35)
        {
            // Enough space on the right - position help text there
            help_opts.y = 2;
            help_opts.x = art_right_edge + 2;
            help_opts.rows = art_rows;
            help_opts.cols = std::max(10, space_on_right - 2);
        }
        else
        {
            // Not enough space on right - position below album art
            help_opts.y = art_opts.y + art_rows + 1;
            help_opts.x = std::min(2u, cols > 4 ? 2u : 0u);
            int help_rows = static_cast<int>(rows) - help_opts.y - 9;
            help_opts.rows = std::max(3, help_rows);
            help_opts.cols = std::max(10, static_cast<int>(cols) - 4);
        }
        help_plane = ncplane_create(stdplane, &help_opts);
        if (!help_plane)
        {
            spdlog::error("Failed to create help plane");
            return;
        }
    };

    // Create initial planes
    createPlanes();

    // Main event loop
    bool running = true;
    bool was_playing = false;
    bool show_help = false;
    struct ncvisual* album_art_visual = nullptr;
    size_t current_track_index = playlist.currentIndex();

    // Extract album art for first track
    std::string art_path = ExtractAlbumArt(playlist.current().filepath);
    spdlog::info("Album art extraction result: {}", art_path.empty() ? "none" : art_path);
    if (!art_path.empty())
    {
        album_art_visual = ncvisual_from_file(art_path.c_str());
        if (!album_art_visual)
        {
            spdlog::warn("Failed to load album art from: {}", art_path);
        }
        else
        {
            spdlog::info("Successfully loaded album art visual");
        }
    }

    // Start playing automatically
    player.play();
    was_playing = true;

    // Track terminal dimensions for resize detection
    unsigned int last_rows = 0, last_cols = 0;
    ncplane_dim_yx(stdplane, &last_rows, &last_cols);

    // Track state for detecting changes
    bool needs_full_redraw = true;  // Full redraw needed (track change, resize, help toggle)
    bool needs_status_update = false;  // Only status update needed (position change)
    int last_position = -1;
    auto last_update = std::chrono::steady_clock::now();
    const auto update_interval = std::chrono::milliseconds(1000); // Update display once per second

    // Track playing state to avoid repeated checks
    bool is_playing = player.isPlaying();
    int loop_counter = 0; // Counter to reduce frequency of some checks

    while (running && !signal_received)
    {
        // Check playing state and update if changed
        bool currently_playing = player.isPlaying();
        if (currently_playing != is_playing)
        {
            is_playing = currently_playing;
            needs_status_update = true;
        }

        // Only check dimensions every 5 iterations (500ms) to reduce overhead
        if (loop_counter % 5 == 0)
        {
            unsigned int current_rows, current_cols;
            ncplane_dim_yx(stdplane, &current_rows, &current_cols);

            if (current_rows != last_rows || current_cols != last_cols)
            {
                spdlog::debug("Terminal resized from {}x{} to {}x{}", last_cols, last_rows, current_cols, current_rows);
                last_rows = current_rows;
                last_cols = current_cols;

                // Recreate planes with new dimensions
                createPlanes();
                needs_full_redraw = true;
            }
        }

        // Check if track changed and load new album art
        if (playlist.currentIndex() != current_track_index)
        {
            current_track_index = playlist.currentIndex();

            // Cleanup old album art
            if (album_art_visual)
            {
                ncvisual_destroy(album_art_visual);
                album_art_visual = nullptr;
            }

            // Extract and load new album art
            art_path = ExtractAlbumArt(playlist.current().filepath);
            if (!art_path.empty())
            {
                album_art_visual = ncvisual_from_file(art_path.c_str());
                if (!album_art_visual)
                {
                    spdlog::warn("Failed to load album art from: {}", art_path);
                }
            }
            needs_full_redraw = true;
        }

        // Only check playback position when actually playing
        if (is_playing)
        {
            int current_position = player.getPosition();
            if (current_position != last_position)
            {
                last_position = current_position;

                // Only update if enough time has passed (throttle updates to once per second)
                auto now = std::chrono::steady_clock::now();
                if (now - last_update >= update_interval)
                {
                    last_update = now;
                    needs_status_update = true;
                }
            }
        }

        // Render based on what needs updating
        if (needs_full_redraw)
        {
            // Full redraw (track change, resize, help toggle, user command)
            DrawUI(stdplane, status_plane, help_plane, art_plane, album_art_visual, player, playlist, show_help);
            notcurses_render(nc);
            needs_full_redraw = false;
            needs_status_update = false;  // Status is already drawn
        }
        else if (needs_status_update)
        {
            // Only update dynamic status info (more efficient)
            DrawStatusUpdate(status_plane, player, playlist);
            notcurses_render(nc);
            needs_status_update = false;
        }

        // Use longer timeout when paused/stopped to reduce CPU usage
        ncinput ni;
        struct timespec ts;
        if (is_playing)
        {
            ts = {0, 100000000}; // 100ms when playing (for responsive progress updates)
        }
        else
        {
            ts = {0, 250000000}; // 250ms when paused/stopped (less frequent wake-ups)
        }
        char32_t ch = notcurses_get(nc, &ts, &ni);

        if (ch != (char32_t)-1)
        {
            // Store previous help state to detect toggle
            bool prev_show_help = show_help;

            HandleCommand(ch, player, playlist, running, show_help);

            // Full redraw needed if help toggled or track changed
            // For other commands (volume, seek, play/pause), status update is sufficient
            if (show_help != prev_show_help || ch == 'n' || ch == 'N' || ch == 'p' || ch == 'P')
            {
                needs_full_redraw = true;
            }
            else
            {
                needs_status_update = true;
            }
        }

        // Check for auto-advance and playlist end (only when playing)
        if (is_playing && CheckAutoAdvance(player, playlist, repeat, was_playing))
        {
            // Playlist has ended, exit
            running = false;
        }

        loop_counter++;
    }

    // Cleanup
    if (album_art_visual)
    {
        ncvisual_destroy(album_art_visual);
    }
    ncplane_destroy(art_plane);
    ncplane_destroy(help_plane);
    ncplane_destroy(status_plane);

    // Clear the screen before stopping
    ncplane_erase(stdplane);
    notcurses_render(nc);

    notcurses_stop(nc);
    nc = nullptr;

    player.cleanup();

    // Final terminal cleanup
    std::cout << "\033[2J\033[H" << std::flush;  // Clear screen and move cursor to home

    return EXIT_SUCCESS;
}
