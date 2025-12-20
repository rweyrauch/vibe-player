# CLI Audio Player & Playlist Generator

A modular command-line audio toolset for Linux following Unix philosophy - two focused tools that work together:

- **cli-playlist**: Generate playlists from directories, files, or AI prompts
- **cli-player**: Play playlists with full interactive controls

## Philosophy

This toolset is designed around separation of concerns:

- **Do one thing well**: Each tool has a single, focused purpose
- **Composable**: Tools work together via pipes and files
- **Reusable output**: Generate playlists once, play many times
- **Minimal dependencies**: Only C++ standard library plus libraries fetched by CMake
- **Unix-style**: Follows conventions (stdout for data, stderr for messages)

## Features

### cli-playlist (Playlist Generator)

- Generate playlists from directories or single files
- **AI-powered curation** from natural language descriptions
  - Cloud-based with Claude API (fast, high quality)
  - Offline with llama.cpp (private, no API key needed)
  - **Full library search** using Claude's tool use capabilities
- Metadata extraction and caching
- Shuffle support
- Output to stdout (for piping) or save to file

### cli-player (Audio Player)

- Play playlists from files or stdin
- Full interactive controls (play, pause, seek, volume)
- Auto-advance through playlists
- Repeat mode
- Real-time status display with track metadata
- Direct single-file playback
- Support for WAV, MP3, FLAC, and OGG formats

## Requirements

- CMake 3.12 or higher
- C++20 compatible compiler (g++, clang++)
- Unix-like system (Linux, macOS, BSD)

**That's it!** No external libraries to install - CMake automatically fetches:

- miniaudio (audio playback)
- cxxopts (command-line parsing)
- colors (terminal control)
- TagLib (metadata extraction)
- nlohmann/json (JSON handling)
- cpp-httplib (HTTP client for Claude API)
- spdlog (logging)
- llama.cpp (optional: local LLM inference)

## Building

```bash
mkdir build
cd build
cmake ..
make
```

This creates two executables: `cli-playlist` and `cli-player`.

## Quick Start

```bash
# Generate a playlist from a directory
./cli-playlist --directory ~/Music > playlist.json

# Play the playlist
./cli-player playlist.json

# Or pipe directly
./cli-playlist --directory ~/Music | ./cli-player --stdin

# AI-generated playlist (requires ANTHROPIC_API_KEY)
./cli-playlist --library ~/Music --prompt "upbeat workout songs" | ./cli-player --stdin
```

## Usage

### cli-playlist: Generate Playlists

**From a directory:**
```bash
# Output to stdout (for piping)
./cli-playlist --directory ~/Music/Jazz

# Save to file
./cli-playlist --directory ~/Music/Jazz --save jazz.json

# With shuffle
./cli-playlist --directory ~/Music --shuffle > shuffled.json
```

**From a single file:**
```bash
./cli-playlist --file song.mp3
```

**AI-powered generation (see [AI Playlist Generation](#ai-playlist-generation)):**
```bash
# Using Claude API
./cli-playlist --library ~/Music --prompt "chill vibes for studying"

# Save AI playlist
./cli-playlist --library ~/Music --prompt "90s rock" --save rock90s.json

# With model selection
./cli-playlist --library ~/Music --prompt "jazz" --claude-model balanced
```

**Options:**
- `--directory <path>` - Generate from directory
- `--file <path>` - Generate from single file
- `--library <path>` - Music library for AI generation (required with --prompt)
- `--prompt <text>` - AI playlist generation
- `--shuffle` - Shuffle the playlist
- `--save <file>` - Save to file (default: stdout)
- `--force-scan` - Force metadata rescan (ignore cache)
- `--verbose` - Enable debug logging

### cli-player: Play Playlists

**From a file:**
```bash
./cli-player playlist.json
```

**From stdin (piped):**
```bash
cat playlist.json | ./cli-player --stdin
./cli-playlist --directory ~/Music | ./cli-player --stdin
```

**Direct file playback:**
```bash
./cli-player --file song.mp3
```

**With options:**
```bash
# Repeat mode
./cli-player playlist.json --repeat

# Non-interactive (auto-play, no controls)
./cli-player playlist.json --no-interactive
```

**Options:**
- `<playlist_file>` - Playlist JSON file (positional)
- `--stdin` - Read playlist from stdin
- `--file <path>` - Play single audio file
- `--repeat` - Repeat playlist when finished
- `--no-interactive` - Disable interactive controls

## Playlist Format

Playlists are JSON files with full track metadata:

```json
{
  "version": "1.0",
  "tracks": [
    {
      "filepath": "/absolute/path/to/song.mp3",
      "filename": "song.mp3",
      "title": "Song Title",
      "artist": "Artist Name",
      "album": "Album Name",
      "genre": "Rock",
      "year": 2020,
      "duration_ms": 245000,
      "file_mtime": 1234567890
    }
  ]
}
```

This format allows the player to display rich metadata during playback.

## AI Playlist Generation

Generate playlists from natural language using AI. Two backends available:

### Claude Backend (Cloud API) - Recommended

Uses Anthropic's Claude API with **full library search** via tool use. Claude can search your entire library intelligently, not just a random sample.

**Setup:**

```bash
export ANTHROPIC_API_KEY="sk-ant-your-key-here"
```

Get your API key from [console.anthropic.com](https://console.anthropic.com)

**Usage:**

```bash
# Basic usage (fast model)
./cli-playlist --library ~/Music --prompt "upbeat workout songs"

# With model selection
./cli-playlist --library ~/Music --prompt "chill jazz" --claude-model balanced

# Model options:
#   fast     - Claude 3.5 Haiku (fastest, cheapest)
#   balanced - Claude 3.5 Sonnet (recommended - good quality/speed balance)
#   best     - Claude Sonnet 4.5 (highest quality, most capable)

# Save the playlist
./cli-playlist --library ~/Music --prompt "90s alternative rock" --save 90s.json
```

**How it works:**

1. Claude gets tools to search your library by artist, genre, album, title, year
2. It strategically explores your music using multiple searches
3. Finds the best matches across your **entire library** (not just a sample!)
4. Returns a curated playlist

**Example:**
```bash
$ ./cli-playlist --library ~/Music --prompt "upbeat workout songs" --claude-model balanced
Using cached metadata (7299 tracks)
Generating AI playlist using tool search...
Searching library...
Generated AI playlist with 25 tracks
```

**Features:**
- ✅ Searches entire library (no sampling)
- ✅ Intelligent multi-step search process
- ✅ High-quality curation
- ✅ Fast (5-15 seconds)
- ✅ Automatic metadata caching

### llama.cpp Backend (Offline/Local)

Uses local LLM inference for completely offline playlist generation.

**Setup:**

1. Download a GGUF model:

| Model | Size | Quality | Speed |
| ----- | ---- | ------- | ----- |
| TinyLlama-1.1B | ~600MB | Basic | Fast |
| Mistral-7B-Instruct | ~4GB | Good | Medium |
| Llama-3-8B-Instruct | ~5GB | Best | Slower |

```bash
mkdir -p ~/models
cd ~/models
# Download Mistral-7B (recommended)
wget https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf
```

2. Generate playlist:

```bash
./cli-playlist \
    --library ~/Music \
    --prompt "relaxing ambient music" \
    --ai-backend llamacpp \
    --ai-model ~/models/mistral-7b-instruct-v0.2.Q4_K_M.gguf
```

**Advanced options:**
```bash
--ai-threads 8              # CPU threads (default: 4)
--ai-context-size 4096      # Context window (default: 2048)
```

**Features:**
- ✅ Completely offline
- ✅ Free to use
- ✅ Privacy-preserving
- ✅ No API key needed
- ⚠️  Samples library (up to 1000 tracks)

### Comparison: Claude vs llama.cpp

| Feature | Claude | llama.cpp |
| ------- | ------ | --------- |
| **Setup** | API key | Download model (~1-5GB) |
| **Cost** | Pay per request | Free |
| **Speed** | Fast (5-15s) | Slower (10-60s) |
| **Quality** | Excellent | Good |
| **Library coverage** | Full library search | Samples 1000 tracks |
| **Offline** | No | Yes |
| **Privacy** | Data sent to API | Fully local |

**Recommendation:** Use Claude with the `balanced` model for best results. Use llama.cpp if you need offline capability or want zero cost.

### Example Prompts

```bash
# Genre-based
--prompt "heavy metal with fast guitar solos"
--prompt "smooth jazz with saxophone"
--prompt "classical piano concertos"

# Mood-based
--prompt "sad songs for rainy days"
--prompt "uplifting songs to start the day"
--prompt "intense focus music for coding"

# Era/style
--prompt "80s synth-pop classics"
--prompt "90s grunge and alternative rock"
--prompt "modern indie folk"

# Activity
--prompt "high-energy workout music"
--prompt "relaxing music for meditation"
--prompt "party songs to dance to"

# Specific
--prompt "songs similar to Pink Floyd"
--prompt "instrumental music without vocals"
--prompt "upbeat songs under 3 minutes"
```

## Composing the Tools

The power of this design is in composition:

### Save and Reuse Playlists

```bash
# Generate once
./cli-playlist --library ~/Music --prompt "workout music" --save workout.json

# Play many times
./cli-player workout.json
./cli-player workout.json --repeat
```

### Quick Playback

```bash
# Generate and play immediately
./cli-playlist --directory ~/Music/Jazz | ./cli-player --stdin
```

### Build Your Own Workflow

```bash
# Generate multiple playlists
./cli-playlist --directory ~/Music/Rock --save rock.json
./cli-playlist --library ~/Music --prompt "chill vibes" --save chill.json

# Choose which to play
./cli-player rock.json
./cli-player chill.json --repeat

# Combine with other tools
./cli-playlist --directory ~/Music | jq '.tracks | length'  # Count tracks
```

## Interactive Controls

While playing, use single-key commands (no Enter needed):

| Key | Action |
| --- | ------ |
| `p` | Play |
| `s` | Stop |
| `u` | Pause |
| `Space` | Toggle play/pause |
| `+` | Volume up (5%) |
| `-` | Volume down (5%) |
| `f` or `→` | Forward 10 seconds |
| `b` or `←` | Back 10 seconds |
| `n` | Next track |
| `h` | Show help |
| `q` | Quit |

## Status Display

The player shows a single updating status line with track metadata:

```
[Playing] Artist - Album - Song Title | 01:23 / 03:45 | Vol: 70% | Track 3/12
```

- **State**: Playing, Paused, or Stopped
- **Track info**: Artist - Album - Title (from metadata)
- **Time**: Position / Duration
- **Volume**: Current level (0-100%)
- **Track number**: Position in playlist

## Troubleshooting

### Playlist Generation

**"ANTHROPIC_API_KEY not set"**
```bash
export ANTHROPIC_API_KEY="your-key-here"
# Add to ~/.bashrc or ~/.zshrc to make permanent
```

**"Model file not found" (llama.cpp)**
- Verify the model path: `--ai-model=/absolute/path/to/model.gguf`
- Check file exists: `ls -lh ~/models/*.gguf`

**Poor quality playlists**
- Be more specific in your prompt
- For Claude: try `--claude-model balanced` or `best`
- For llama.cpp: use a larger model (Mistral-7B or Llama-3-8B)
- Ensure your music files have proper metadata tags

### Playback

**"Error loading audio file"**
- Check file exists and is readable
- Verify format is supported (WAV, MP3, FLAC, OGG)
- Try playing the file directly: `./cli-player --file song.mp3`

**No sound**
- Check system volume
- Try increasing volume with `+` key
- Verify audio device is working: `aplay -l`

### Piping Issues

**"Failed to parse playlist from stdin"**
- Ensure cli-playlist writes valid JSON: `./cli-playlist ... | jq .`
- Check for errors on stderr: `./cli-playlist ... 2>&1 | less`

## Debugging

Enable verbose logging for detailed diagnostics:

```bash
./cli-playlist --library ~/Music --prompt "jazz" --verbose
```

Logs are written to:
- `~/.cache/cli-playlist/cli-playlist.log` (generator)
- `~/.cache/cli-player/cli-player.log` (player)

View logs:
```bash
tail -f ~/.cache/cli-playlist/cli-playlist.log
```

Logs include:
- AI prompts and responses
- Tool use operations (Claude)
- Metadata extraction details
- Cache operations
- Error details

## Supported Audio Formats

- **WAV** - Uncompressed audio
- **MP3** - MPEG Audio Layer 3
- **FLAC** - Free Lossless Audio Codec
- **OGG** - Ogg Vorbis

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  User Input: Natural language or directory path             │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
         ┌─────────────────────┐
         │   cli-playlist      │
         │  (Generator)        │
         ├─────────────────────┤
         │ • Directory scan    │
         │ • AI generation     │
         │ • Metadata cache    │
         │ • JSON output       │
         └──────────┬──────────┘
                    │
                    ▼ JSON (stdout or file)
         ┌──────────────────────┐
         │   cli-player         │
         │  (Playback)          │
         ├──────────────────────┤
         │ • JSON parser        │
         │ • Audio playback     │
         │ • Interactive UI     │
         │ • Playlist nav       │
         └──────────────────────┘
```

**AI Backend Architecture:**

```
AIBackend (interface)
    ├── ClaudeBackend
    │   ├── Tool use for library search
    │   └── Multi-turn conversations
    │
    └── LlamaCppBackend
        └── Local inference with llama.cpp

LibrarySearch
    ├── searchByArtist()
    ├── searchByGenre()
    ├── searchByAlbum()
    ├── searchByTitle()
    └── searchByYearRange()
```

## Design Goals

This toolset prioritizes:

1. **Separation of concerns**: Each tool does one thing well
2. **Composability**: Tools work together via standard Unix interfaces
3. **Reusability**: Generate playlists once, use many times
4. **Minimal dependencies**: Everything fetched by CMake
5. **Simple codebase**: Easy to understand and modify

What this is NOT:

- Not a music library manager
- Not a full-featured media player like VLC
- Not a streaming service client
- Not highly configurable (simple by design)

## Technical Details

### Technologies

- **Audio**: [miniaudio](https://github.com/mackron/miniaudio) (cross-platform audio playback)
- **Metadata**: [TagLib](https://github.com/taglib/taglib) (ID3, Vorbis, etc.)
- **JSON**: [nlohmann/json](https://github.com/nlohmann/json)
- **HTTP**: [cpp-httplib](https://github.com/yhirose/cpp-httplib) (Claude API)
- **Logging**: [spdlog](https://github.com/gabime/spdlog)
- **Local LLM**: [llama.cpp](https://github.com/ggerganov/llama.cpp)
- **CLI parsing**: [cxxopts](https://github.com/jarro2783/cxxopts)
- **Terminal**: [colors](https://github.com/ShakaUVM/colors)
- **Build**: CMake 3.12+ with FetchContent
- **Language**: C++20

### Project Structure

```
cli-player/
├── src/
│   ├── main_playlist_gen.cpp    # cli-playlist application
│   ├── main_player.cpp          # cli-player application
│   ├── playlist.{h,cpp}         # Playlist data structure
│   ├── player.{h,cpp}           # Audio playback engine
│   ├── metadata.{h,cpp}         # Metadata extraction
│   ├── metadata_cache.{h,cpp}   # Persistent caching
│   ├── ai_backend*.{h,cpp}      # AI backend implementations
│   └── library_search.{h,cpp}   # Library search tools
└── CMakeLists.txt               # Build configuration
```

## License

This project is provided as-is for educational and personal use.

## Contributing

This is intended to be a reference implementation showcasing Unix-style tool composition. Feel free to fork and extend!

## Credits

Built with ❤️ using excellent open-source libraries. Special thanks to:
- [miniaudio](https://github.com/mackron/miniaudio) for simple, portable audio
- [TagLib](https://github.com/taglib/taglib) for comprehensive metadata support
- [llama.cpp](https://github.com/ggerganov/llama.cpp) for efficient local LLM inference
- [Anthropic](https://anthropic.com) for the Claude API
