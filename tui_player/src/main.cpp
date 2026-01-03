/*
 * vibe-player
 * main.cpp
 */

#include "player.h"
#include "metadata.h"
#include "playlist.h"

#include <csignal>
#include <cstring>

#include <algorithm>
#include <atomic>
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

// Structure to hold all UI planes
struct UIPlanes {
    struct ncplane* stdplane = nullptr;
    struct ncplane* status_plane = nullptr;
    struct ncplane* help_plane = nullptr;
    struct ncplane* art_plane = nullptr;
    struct ncvisual* album_art_visual = nullptr;
    struct ncplane* playlist_plane = nullptr;
};

// Parse blitter name from command-line string
ncblitter_e ParseBlitter(const std::string& blitter_name) {
    std::string lower = blitter_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "default" || lower == "auto") {
        return NCBLIT_DEFAULT;
    } else if (lower == "ascii" || lower == "1x1") {
        return NCBLIT_1x1;
    } else if (lower == "half" || lower == "2x1") {
        return NCBLIT_2x1;
    } else if (lower == "quad" || lower == "quadrant" || lower == "2x2") {
        return NCBLIT_2x2;
    } else if (lower == "sextant" || lower == "sex" || lower == "3x2") {
        return NCBLIT_3x2;
    } else if (lower == "braille") {
        return NCBLIT_BRAILLE;
    } else if (lower == "pixel" || lower == "sixel" || lower == "kitty") {
        return NCBLIT_PIXEL;
    } else {
        spdlog::warn("Unknown blitter '{}', using default", blitter_name);
        return NCBLIT_DEFAULT;
    }
}

// Get human-readable name for blitter
const char* BlitterName(ncblitter_e blitter) {
    switch (blitter) {
        case NCBLIT_DEFAULT: return "default";
        case NCBLIT_1x1: return "ascii (1x1)";
        case NCBLIT_2x1: return "half-block (2x1)";
        case NCBLIT_2x2: return "quadrant (2x2)";
        case NCBLIT_3x2: return "sextant (3x2)";
        case NCBLIT_BRAILLE: return "braille";
        case NCBLIT_PIXEL: return "pixel";
        default: return "unknown";
    }
}

// Try to blit with fallback support
struct ncplane* BlitWithFallback(struct notcurses* nc, struct ncvisual* visual,
                                 struct ncvisual_options* vopts, ncblitter_e preferred_blitter) {
    // Fallback chain: try preferred, then degrade gracefully
    ncblitter_e fallback_chain[] = {
        preferred_blitter,
        NCBLIT_2x2,      // Quadrant (good compatibility)
        NCBLIT_2x1,      // Half-block (even better compatibility)
        NCBLIT_1x1,      // ASCII (maximum compatibility)
    };

    struct ncplane* result = nullptr;

    for (ncblitter_e blitter : fallback_chain) {
        // Skip duplicates in fallback chain
        if (result == nullptr || blitter != preferred_blitter) {
            vopts->blitter = blitter;
            result = ncvisual_blit(nc, visual, vopts);

            if (result != nullptr) {
                if (blitter != preferred_blitter) {
                    spdlog::info("Fell back to {} blitter", BlitterName(blitter));
                } else {
                    spdlog::debug("Using {} blitter", BlitterName(blitter));
                }
                return result;
            } else {
                spdlog::debug("Failed to blit with {} blitter, trying fallback", BlitterName(blitter));
            }
        }
    }

    spdlog::error("Failed to blit album art with any blitter");
    return nullptr;
}

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

void DrawUI(UIPlanes& planes, AudioPlayer &player, const Playlist &playlist, bool show_help, ncblitter_e blitter)
{
    // If planes are null (terminal too small), skip drawing
    if (!planes.status_plane || !planes.help_plane || !planes.art_plane)
    {
        return;
    }

    // Clear planes
    ncplane_erase(planes.stdplane);
    ncplane_erase(planes.status_plane);
    ncplane_erase(planes.help_plane);
    ncplane_erase(planes.art_plane);

    // Get dimensions
    unsigned int rows, cols;
    ncplane_dim_yx(planes.stdplane, &rows, &cols);

    // Draw title bar
    ncplane_set_fg_rgb8(planes.stdplane, 0x8F, 0xC8, 0xD8); // Soft sky blue
    ncplane_set_styles(planes.stdplane, NCSTYLE_BOLD);
    std::string title = " TUI Player ";
    ncplane_putstr_yx(planes.stdplane, 0, (cols - title.length()) / 2, title.c_str());
    ncplane_set_styles(planes.stdplane, NCSTYLE_NONE);

    // Draw album art if available
    if (planes.album_art_visual)
    {
        struct ncvisual_options vopts = {};
        vopts.n = planes.art_plane;
        vopts.scaling = NCSCALE_SCALE;
        vopts.flags = 0;

        // Render the visual with fallback support
        struct ncplane* result = BlitWithFallback(nc, planes.album_art_visual, &vopts, blitter);
        if (result == nullptr)
        {
            spdlog::warn("Failed to blit album art visual with any blitter");
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

    ncplane_set_fg_rgb8(planes.status_plane, 0xF0, 0xF0, 0xE8); // Soft cream
    ncplane_putstr_yx(planes.status_plane, 0, song_x, song_label.c_str());
    ncplane_set_styles(planes.status_plane, NCSTYLE_BOLD);
    ncplane_putstr(planes.status_plane, song_value.c_str());
    ncplane_set_styles(planes.status_plane, NCSTYLE_NONE);

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

    ncplane_set_fg_rgb8(planes.status_plane, 0x5F, 0xD4, 0xD4); // Turquoise
    ncplane_putstr_yx(planes.status_plane, 1, artist_x, artist_label.c_str());
    ncplane_set_styles(planes.status_plane, NCSTYLE_BOLD);
    if (track.artist)
    {
        ncplane_putstr(planes.status_plane, artist_value.c_str());
    }
    else
    {
        ncplane_set_fg_rgb8(planes.status_plane, 0xA0, 0xA0, 0x98); // Warm gray
        ncplane_putstr(planes.status_plane, artist_value.c_str());
    }
    ncplane_set_styles(planes.status_plane, NCSTYLE_NONE);

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

    ncplane_set_fg_rgb8(planes.status_plane, 0xB4, 0xA7, 0xD6); // Soft lavender
    ncplane_putstr_yx(planes.status_plane, 2, album_x, album_label.c_str());
    ncplane_set_styles(planes.status_plane, NCSTYLE_BOLD);
    if (track.album)
    {
        ncplane_putstr(planes.status_plane, album_value.c_str());
    }
    else
    {
        ncplane_set_fg_rgb8(planes.status_plane, 0xA0, 0xA0, 0x98); // Warm gray
        ncplane_putstr(planes.status_plane, album_value.c_str());
    }
    ncplane_set_styles(planes.status_plane, NCSTYLE_NONE);

    // Line 3: Empty line for spacing

    // Lines 4-6: Dynamic status (state, progress, volume) - drawn by DrawStatusUpdate
    DrawStatusUpdate(planes.status_plane, player, playlist);

    // Help text
    if (show_help)
    {
        ncplane_set_fg_rgb8(planes.help_plane, 0xF0, 0xF0, 0xE8); // Soft cream
        ncplane_set_styles(planes.help_plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(planes.help_plane, 0, 2, "Keyboard Controls:");
        ncplane_set_styles(planes.help_plane, NCSTYLE_NONE);

        ncplane_set_fg_rgb8(planes.help_plane, 0xD4, 0xC8, 0xA8); // Warm beige
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
            ncplane_putstr_yx(planes.help_plane, i + 2, 2, help_lines[i]);
        }
    }
    else
    {
        ncplane_set_fg_rgb8(planes.help_plane, 0xA0, 0xA0, 0x98); // Warm gray
        ncplane_putstr_yx(planes.help_plane, 0, 2, "Press 'h' for help, 'q' to quit");
    }
}

void DrawPlaylistView(struct ncplane* playlist_plane,
                      const Playlist& playlist,
                      size_t cursor_position,
                      size_t scroll_offset)
{
    if (!playlist_plane)
    {
        return;
    }

    // Clear the plane
    ncplane_erase(playlist_plane);

    // Get dimensions
    unsigned int rows, cols;
    ncplane_dim_yx(playlist_plane, &rows, &cols);

    // Draw border and title
    ncplane_set_fg_rgb8(playlist_plane, 0x7F, 0xC8, 0xA0);  // Sea green
    ncplane_set_styles(playlist_plane, NCSTYLE_BOLD);

    // Top border - build as string
    std::string top_border;
    top_border += "┌";
    for (unsigned int i = 1; i < cols - 1; ++i)
    {
        top_border += "─";
    }
    top_border += "┐";
    ncplane_putstr_yx(playlist_plane, 0, 0, top_border.c_str());

    // Title
    std::string title = " Playlist ";
    int title_x = (cols - title.length()) / 2;
    ncplane_putstr_yx(playlist_plane, 0, title_x, title.c_str());

    // Side borders
    for (unsigned int i = 1; i < rows - 1; ++i)
    {
        ncplane_putstr_yx(playlist_plane, i, 0, "│");
        ncplane_putstr_yx(playlist_plane, i, cols - 1, "│");
    }

    // Bottom border - build as string
    std::string bottom_border;
    bottom_border += "└";
    for (unsigned int i = 1; i < cols - 1; ++i)
    {
        bottom_border += "─";
    }
    bottom_border += "┘";
    ncplane_putstr_yx(playlist_plane, rows - 1, 0, bottom_border.c_str());

    ncplane_set_styles(playlist_plane, NCSTYLE_NONE);

    // Calculate available space for tracks
    int content_height = rows - 2;  // Subtract top and bottom borders
    int content_width = cols - 4;   // Subtract left border, padding, right border

    // Get playlist tracks
    const auto& tracks = playlist.tracks();
    size_t current_playing_index = playlist.currentIndex();

    // Draw tracks
    int display_row = 1;  // Start below top border
    for (size_t i = scroll_offset; i < tracks.size() && display_row < static_cast<int>(rows - 1); ++i)
    {
        const auto& track = tracks[i];

        // Format: "Artist - Title" or fallback to filename
        std::string display_text;
        if (track.artist && track.title)
        {
            display_text = *track.artist + " - " + *track.title;
        }
        else if (track.title)
        {
            display_text = *track.title;
        }
        else
        {
            display_text = track.filename;
        }

        // Truncate if needed
        if (display_text.length() > static_cast<size_t>(content_width))
        {
            display_text = TruncateString(display_text, content_width);
        }

        // Determine styling
        bool is_cursor = (i == cursor_position);
        bool is_playing = (i == current_playing_index);

        // Clear background first
        ncplane_set_bg_rgb8(playlist_plane, 0x00, 0x00, 0x00);  // Default black background

        // Apply styling
        if (is_cursor && is_playing)
        {
            // Both cursor and playing - bright highlight with white background
            ncplane_set_bg_rgb8(playlist_plane, 0xFF, 0xFF, 0xFF);  // White background
            ncplane_set_fg_rgb8(playlist_plane, 0x00, 0x00, 0x00);  // Black text
            ncplane_set_styles(playlist_plane, NCSTYLE_BOLD);
            ncplane_putstr_yx(playlist_plane, display_row, 2, "▶ ");
            ncplane_putstr(playlist_plane, display_text.c_str());
        }
        else if (is_cursor)
        {
            // Just cursor - selection highlight with gray background
            ncplane_set_bg_rgb8(playlist_plane, 0x40, 0x40, 0x40);  // Dark gray background
            ncplane_set_fg_rgb8(playlist_plane, 0xFF, 0xFF, 0xFF);  // White text
            ncplane_set_styles(playlist_plane, NCSTYLE_NONE);
            ncplane_putstr_yx(playlist_plane, display_row, 2, "  ");
            ncplane_putstr(playlist_plane, display_text.c_str());
        }
        else if (is_playing)
        {
            // Just playing - marker only
            ncplane_set_fg_rgb8(playlist_plane, 0x98, 0xD8, 0xC8);  // Mint green
            ncplane_set_styles(playlist_plane, NCSTYLE_BOLD);
            ncplane_putstr_yx(playlist_plane, display_row, 2, "▶ ");
            ncplane_putstr(playlist_plane, display_text.c_str());
        }
        else
        {
            // Regular track
            ncplane_set_fg_rgb8(playlist_plane, 0xD4, 0xC8, 0xA8);  // Warm beige
            ncplane_set_styles(playlist_plane, NCSTYLE_NONE);
            ncplane_putstr_yx(playlist_plane, display_row, 2, "  ");
            ncplane_putstr(playlist_plane, display_text.c_str());
        }

        // Reset background
        ncplane_set_bg_rgb8(playlist_plane, 0x00, 0x00, 0x00);
        ncplane_set_styles(playlist_plane, NCSTYLE_NONE);

        display_row++;
    }

    // Draw scrollbar if needed
    if (tracks.size() > static_cast<size_t>(content_height))
    {
        int scrollbar_height = std::max(1, (content_height * content_height) / static_cast<int>(tracks.size()));
        int scrollbar_position = 1 + (scroll_offset * (content_height - scrollbar_height)) /
                                 std::max(1, static_cast<int>(tracks.size()) - content_height);

        ncplane_set_fg_rgb8(playlist_plane, 0x50, 0x50, 0x48);  // Dark gray
        for (int i = 1; i < static_cast<int>(rows - 1); ++i)
        {
            if (i >= scrollbar_position && i < scrollbar_position + scrollbar_height)
            {
                ncplane_putstr_yx(playlist_plane, i, cols - 2, "█");
            }
            else
            {
                ncplane_putstr_yx(playlist_plane, i, cols - 2, "░");
            }
        }
    }

    // Draw footer with hints
    std::string footer = " j/k: navigate | Enter: play | l: close ";
    int footer_x = (cols - footer.length()) / 2;
    if (footer_x >= 0 && footer.length() <= cols - 2)
    {
        ncplane_set_fg_rgb8(playlist_plane, 0xA0, 0xA0, 0x98);  // Warm gray
        ncplane_putstr_yx(playlist_plane, rows - 1, footer_x, footer.c_str());
    }
}

void UpdatePlaylistScroll(size_t cursor_position,
                          size_t& scroll_offset,
                          size_t playlist_size,
                          int visible_height)
{
    // Ensure cursor is visible
    if (cursor_position < scroll_offset)
    {
        // Cursor moved above visible area
        scroll_offset = cursor_position;
    }
    else if (cursor_position >= scroll_offset + static_cast<size_t>(visible_height))
    {
        // Cursor moved below visible area
        scroll_offset = cursor_position - visible_height + 1;
    }

    // Clamp scroll_offset to valid range
    size_t max_scroll = (playlist_size > static_cast<size_t>(visible_height))
                        ? playlist_size - visible_height
                        : 0;
    if (scroll_offset > max_scroll)
    {
        scroll_offset = max_scroll;
    }
}

bool HandleCommand(char32_t ch,
                   AudioPlayer &player,
                   Playlist &playlist,
                   bool &running,
                   bool &show_help,
                   bool &show_playlist,
                   size_t &playlist_cursor)
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
    case 'l':
    case 'L':
        // Toggle playlist view
        show_playlist = !show_playlist;
        if (show_playlist)
        {
            // Initialize cursor to current playing track
            playlist_cursor = playlist.currentIndex();
        }
        break;
    case 'j':
    case 'J':
        if (show_playlist && playlist.size() > 0)
        {
            // Move cursor down
            if (playlist_cursor < playlist.size() - 1)
            {
                playlist_cursor++;
            }
        }
        break;
    case 'k':
    case 'K':
        if (show_playlist && playlist.size() > 0)
        {
            // Move cursor up
            if (playlist_cursor > 0)
            {
                playlist_cursor--;
            }
        }
        break;
    case NCKEY_ENTER:
    case '\n':
    case '\r':
        if (show_playlist)
        {
            // Play selected track
            if (playlist_cursor < playlist.size())
            {
                playlist.setIndex(playlist_cursor);
                player.cleanup();
                player.loadFile(playlist.current().filepath);
                player.play();
                show_playlist = false;  // Close playlist after selection
            }
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
    // Parse command line arguments using cxxopts
    cxxopts::Options options("tui-player",
                             "TUI Player - Play audio playlists");

    // clang-format off
    options.add_options()
        ("playlist", "Playlist file to play", cxxopts::value<std::string>())
        ("f,file", "Play a single audio file", cxxopts::value<std::string>())
        ("stdin", "Read playlist from stdin")
        ("r,repeat", "Repeat playlist")
        ("b,blitter", "Image blitter for album art (default|ascii|half|quad|sextant|braille|pixel)",
         cxxopts::value<std::string>()->default_value("default"))
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
    const bool verbose = result.count("verbose") > 0;

    // Parse blitter selection
    std::string blitter_str = result["blitter"].as<std::string>();
    ncblitter_e selected_blitter = ParseBlitter(blitter_str);

    // Initialize logger
    InitializeLogger(verbose);

    spdlog::info("Selected blitter: {}", BlitterName(selected_blitter));

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

        // Reopen stdin to /dev/tty for keyboard input
        if (!freopen("/dev/tty", "r", stdin))
        {
            std::cerr << "Warning: Could not reopen stdin for keyboard input" << std::endl;
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

    UIPlanes planes;
    planes.stdplane = notcurses_stdplane(nc);

    // Clear and render initial display
    ncplane_erase(planes.stdplane);
    notcurses_render(nc);

    // Lambda to create/recreate planes
    auto createPlanes = [&]() {
        // Destroy old planes if they exist
        if (planes.status_plane) ncplane_destroy(planes.status_plane);
        if (planes.help_plane) ncplane_destroy(planes.help_plane);
        if (planes.art_plane) ncplane_destroy(planes.art_plane);
        if (planes.playlist_plane) ncplane_destroy(planes.playlist_plane);

        // Reset to nullptr to avoid using stale pointers
        planes.status_plane = nullptr;
        planes.help_plane = nullptr;
        planes.art_plane = nullptr;
        planes.playlist_plane = nullptr;

        // Get current dimensions
        unsigned int rows, cols;
        ncplane_dim_yx(planes.stdplane, &rows, &cols);

        // Minimum terminal size check
        const unsigned int MIN_ROWS = 15;
        const unsigned int MIN_COLS = 30;

        if (rows < MIN_ROWS || cols < MIN_COLS)
        {
            spdlog::warn("Terminal too small ({}x{}), minimum is {}x{}", cols, rows, MIN_COLS, MIN_ROWS);
            // Display error message on stdplane
            ncplane_erase(planes.stdplane);
            ncplane_set_fg_rgb8(planes.stdplane, 0xFF, 0x80, 0x80);
            std::string error_msg = "Terminal too small!";
            std::string min_msg = "Minimum: " + std::to_string(MIN_COLS) + "x" + std::to_string(MIN_ROWS);
            if (rows >= 2 && cols >= error_msg.length())
            {
                ncplane_putstr_yx(planes.stdplane, rows / 2, (cols - error_msg.length()) / 2, error_msg.c_str());
            }
            if (rows >= 3 && cols >= min_msg.length())
            {
                ncplane_putstr_yx(planes.stdplane, rows / 2 + 1, (cols - min_msg.length()) / 2, min_msg.c_str());
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
        planes.status_plane = ncplane_create(planes.stdplane, &status_opts);
        if (!planes.status_plane)
        {
            spdlog::error("Failed to create status plane");
            return;
        }

        // Album art plane - use ncvisual_geom for correct aspect ratio
        int available_rows = static_cast<int>(rows) - 12;
        int art_rows = std::min(30, available_rows);
        art_rows = std::max(5, art_rows);
        int art_cols = art_rows; // Default 1:1

        // Use ncvisual_geom to calculate proper dimensions if we have album art
        if (planes.album_art_visual)
        {
            struct ncvgeom geom;
            struct ncvisual_options temp_vopts = {};
            temp_vopts.scaling = NCSCALE_SCALE;
            temp_vopts.blitter = NCBLIT_2x1;  // Match the rendering blitter

            if (ncvisual_geom(nc, planes.album_art_visual, &temp_vopts, &geom) == 0)
            {
                spdlog::debug("Image geometry: {}x{} pixels, rendered as {}x{} cells",
                    geom.pixx, geom.pixy, geom.rcellx, geom.rcelly);

                // Calculate aspect ratio from ncvisual_geom rendered cells
                // notcurses calculates the correct cell ratio for this image+blitter+terminal
                if (geom.rcellx > 0 && geom.rcelly > 0)
                {
                    // Scale the rendered geometry to our desired height
                    float scale = (float)art_rows / (float)geom.rcelly;
                    art_cols = (int)(geom.rcellx * scale);

                    spdlog::debug("Calculated plane dimensions: {}x{} cells (scaled from rendered {}x{})",
                        art_cols, art_rows, geom.rcellx, geom.rcelly);
                }
            }
            else
            {
                spdlog::warn("ncvisual_geom failed, using default 1:1 aspect");
            }
        }

        art_cols = std::min(art_cols, static_cast<int>(cols * 0.8));
        art_cols = std::max(10, art_cols);

        spdlog::debug("Final art plane dimensions: {}x{} cells", art_cols, art_rows);

        struct ncplane_options art_opts = {};
        art_opts.y = 2;
        art_opts.x = cols > static_cast<unsigned>(art_cols) ? (cols - art_cols) / 2 : 0;
        art_opts.rows = art_rows;
        art_opts.cols = art_cols;
        planes.art_plane = ncplane_create(planes.stdplane, &art_opts);
        if (!planes.art_plane)
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
        planes.help_plane = ncplane_create(planes.stdplane, &help_opts);
        if (!planes.help_plane)
        {
            spdlog::error("Failed to create help plane");
            return;
        }

        // Playlist plane - overlay on album art, leave status bar visible
        struct ncplane_options playlist_opts = {};
        int playlist_height = art_rows;  // Use same height as album art
        int playlist_width = std::min(60, static_cast<int>(cols * 0.7));  // 70% of screen width, max 60 cols
        playlist_opts.y = art_opts.y;  // Align with album art
        playlist_opts.x = (cols - playlist_width) / 2;  // Center horizontally
        playlist_opts.rows = playlist_height;
        playlist_opts.cols = playlist_width;
        planes.playlist_plane = ncplane_create(planes.stdplane, &playlist_opts);
        if (!planes.playlist_plane)
        {
            spdlog::error("Failed to create playlist plane");
            return;
        }

        // Clear and move playlist plane to bottom so it doesn't obscure album art initially
        ncplane_erase(planes.playlist_plane);
        ncplane_move_bottom(planes.playlist_plane);
    };

    // Extract album art for first track before creating planes
    size_t current_track_index = playlist.currentIndex();
    std::string art_path = ExtractAlbumArt(playlist.current().filepath);
    spdlog::info("Album art extraction result: {}", art_path.empty() ? "none" : art_path);
    if (!art_path.empty())
    {
        planes.album_art_visual = ncvisual_from_file(art_path.c_str());
        if (!planes.album_art_visual)
        {
            spdlog::warn("Failed to load album art from: {}", art_path);
        }
        else
        {
            spdlog::info("Successfully loaded album art visual");
        }
    }

    // Create initial planes (after loading album art)
    createPlanes();

    // Main event loop
    bool running = true;
    bool was_playing = false;
    bool show_help = false;

    // Playlist view state
    bool show_playlist = false;           // Is playlist view visible
    size_t playlist_cursor = 0;           // Currently selected song in playlist view
    size_t playlist_scroll_offset = 0;    // Top visible line for scrolling

    // Start playing automatically
    player.play();
    was_playing = true;

    // Track terminal dimensions for resize detection
    unsigned int last_rows = 0, last_cols = 0;
    ncplane_dim_yx(planes.stdplane, &last_rows, &last_cols);

    // Track state for detecting changes
    bool needs_full_redraw = true;  // Full redraw needed (track change, resize, help toggle)
    bool needs_status_update = false;  // Only status update needed (position change)

    while (running && !signal_received)
    {
        {
            unsigned int current_rows, current_cols;
            ncplane_dim_yx(planes.stdplane, &current_rows, &current_cols);

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
            if (planes.album_art_visual)
            {
                ncvisual_destroy(planes.album_art_visual);
                planes.album_art_visual = nullptr;
            }

            // Extract and load new album art
            art_path = ExtractAlbumArt(playlist.current().filepath);
            if (!art_path.empty())
            {
                planes.album_art_visual = ncvisual_from_file(art_path.c_str());
                if (!planes.album_art_visual)
                {
                    spdlog::warn("Failed to load album art from: {}", art_path);
                }
            }
            needs_full_redraw = true;
        }

        // Render based on what needs updating
        if (needs_full_redraw)
        {
            DrawUI(planes, player, playlist, show_help, selected_blitter);

            // Draw playlist overlay if visible, otherwise move it to bottom of stack
            if (show_playlist && planes.playlist_plane)
            {
                // Move playlist to top so it's visible over album art
                ncplane_move_top(planes.playlist_plane);
                DrawPlaylistView(planes.playlist_plane, playlist,
                                playlist_cursor, playlist_scroll_offset);
            }
            else if (planes.playlist_plane)
            {
                // Move playlist to bottom of stack so it doesn't obscure album art
                ncplane_move_bottom(planes.playlist_plane);
                ncplane_erase(planes.playlist_plane);
            }

            notcurses_render(nc);

            needs_full_redraw = false;
            needs_status_update = false;  // Status is already drawn
        }
        else if (needs_status_update)
        {
            DrawStatusUpdate(planes.status_plane, player, playlist);
            notcurses_render(nc);
            needs_status_update = false;
        }

        // Use longer timeout when paused/stopped to reduce CPU usage
        ncinput ni;
        struct timespec ts = {0, 100000000};
        char32_t ch = notcurses_get(nc, &ts, &ni);

        if (ch != (char32_t)-1)
        {
            // Store previous help state to detect toggle
            bool prev_show_help = show_help;

            HandleCommand(ch, player, playlist, running, show_help, show_playlist, playlist_cursor);

            // Update playlist scroll if playlist is visible
            if (show_playlist && planes.playlist_plane)
            {
                unsigned int playlist_rows, playlist_cols;
                ncplane_dim_yx(planes.playlist_plane, &playlist_rows, &playlist_cols);
                int content_height = playlist_rows - 2;  // Subtract borders
                UpdatePlaylistScroll(playlist_cursor, playlist_scroll_offset,
                                     playlist.size(), content_height);
            }

            // Full redraw needed if help toggled, playlist toggled, or track changed
            // For other commands (volume, seek, play/pause), status update is sufficient
            if (show_help != prev_show_help || ch == 'n' || ch == 'N' || ch == 'p' || ch == 'P' ||
                ch == 'l' || ch == 'L')
            {
                needs_full_redraw = true;
            }
            else if (show_playlist && (ch == 'j' || ch == 'J' || ch == 'k' || ch == 'K' ||
                                       ch == NCKEY_ENTER || ch == '\n' || ch == '\r'))
            {
                // Playlist navigation also needs redraw
                needs_full_redraw = true;
            }
            else
            {
                needs_status_update = true;
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

    // Cleanup
    if (planes.album_art_visual)
    {
        ncvisual_destroy(planes.album_art_visual);
    }
    ncplane_destroy(planes.art_plane);
    ncplane_destroy(planes.help_plane);
    ncplane_destroy(planes.status_plane);
    if (planes.playlist_plane)
    {
        ncplane_destroy(planes.playlist_plane);
    }

    // Clear the screen before stopping
    ncplane_erase(planes.stdplane);
    notcurses_render(nc);

    notcurses_stop(nc);
    nc = nullptr;

    player.cleanup();

    // Final terminal cleanup
    std::cout << "\033[2J\033[H" << std::flush;  // Clear screen and move cursor to home

    return EXIT_SUCCESS;
}
