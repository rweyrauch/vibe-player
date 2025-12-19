# CLI Audio Player

A minimal, stripped-down command-line audio player for Linux with zero external dependencies (except those automatically fetched by CMake).

## Philosophy

This is a no-frills audio player designed for simplicity:

- **Minimal dependencies**: Only C++ standard library plus header-only libraries fetched by CMake
- **Simple interface**: Single-line status display with single-key commands
- **Fast startup**: No complex UI, just load and play
- **Portable**: Uses miniaudio for cross-platform audio playback

## Features

- Play single audio files or entire directories
- Auto-play on startup
- Shuffle and repeat modes
- Simple playback controls (play, pause, stop, seek)
- Volume control
- Auto-advance to next track in directory mode
- Real-time status display
- Support for WAV, MP3, FLAC, and OGG audio formats
- **AI-powered playlist generation** from natural language descriptions
  - Cloud-based (Claude API) or offline (llama.cpp)
  - Smart sampling for large music libraries
  - Metadata caching for fast subsequent runs

## Requirements

- CMake 3.12 or higher
- C++20 compatible compiler (g++, clang++)
- Unix-like system (Linux, macOS, BSD)

**That's it!** No external libraries to install - CMake automatically fetches:

- miniaudio (header-only audio library)
- cxxopts (header-only command-line parser)
- colors (header-only terminal control)
- TagLib (audio metadata extraction)
- nlohmann/json (JSON parsing)
- cpp-httplib (HTTP client for Claude API)
- llama.cpp (optional: local LLM inference)

## Building

```bash
mkdir build
cd build
cmake ..
make
```

The executable will be created as `cli-player` in the `build` directory.

## Usage

### Play a single audio file

```bash
./cli-player audio.mp3
```

The player will load the file and start playing automatically.

### Play all audio files in a directory

```bash
./cli-player -d /path/to/music
```

Options:

- `-s, --shuffle`: Shuffle the playlist
- `-r, --repeat`: Repeat the playlist when it ends

Example:

```bash
./cli-player -d ~/Music/Album --shuffle --repeat
```

## AI Playlist Generation

Generate playlists from natural language descriptions using AI. The player supports two backends:

### Claude Backend (Cloud API)

Uses Anthropic's Claude API for high-quality playlist curation. Requires an API key.

**Setup:**

```bash
export ANTHROPIC_API_KEY="sk-ant-your-key-here"
```

Get your API key from [console.anthropic.com](https://console.anthropic.com)

**Usage:**

```bash
./cli-player --prompt "upbeat rock songs from the 90s" --library ~/Music
./cli-player --prompt "chill jazz for studying" --library ~/Music/Jazz
./cli-player --prompt "energetic workout music" --library ~/Music --shuffle

# With verbose output to see the AI prompt
./cli-player --prompt "relaxing ambient music" --library ~/Music --verbose
```

**Features:**

- High-quality curation
- Fast response times
- Handles large libraries (samples 1500 tracks)
- Automatic metadata caching

### llama.cpp Backend (Offline/Local)

Uses local LLM inference for offline playlist generation. No API key required!

**Setup:**

1. Download a GGUF model (recommended models):

| Model | Size | Quality | Speed | Download |
| ----- | ---- | ------- | ----- | -------- |
| TinyLlama-1.1B | ~600MB | Basic | Fast | [Link](https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF) |
| Mistral-7B-Instruct | ~4GB | Good | Medium | [Link](https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF) |
| Llama-3-8B-Instruct | ~5GB | Best | Slower | [Link](https://huggingface.co/bartowski/Meta-Llama-3-8B-Instruct-GGUF) |

Example download:

```bash
mkdir -p ~/models
cd ~/models
wget https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf
```

2. Run with the model:

```bash
./cli-player \
    --prompt "relaxing ambient music" \
    --library ~/Music \
    --ai-backend=llamacpp \
    --ai-model=~/models/mistral-7b-instruct-v0.2.Q4_K_M.gguf
```

**Advanced options:**

```bash
# Performance tuning
./cli-player \
    --prompt "upbeat electronic dance music" \
    --library ~/Music \
    --ai-backend=llamacpp \
    --ai-model=~/models/mistral-7b-instruct-v0.2.Q4_K_M.gguf \
    --ai-threads=8 \
    --ai-context-size=4096

# With verbose output for debugging
./cli-player \
    --prompt "relaxing piano music" \
    --library ~/Music \
    --ai-backend=llamacpp \
    --ai-model=~/models/mistral-7b.gguf \
    --verbose
```

**Features:**

- Completely offline (no internet required)
- Free to use
- Privacy-preserving (data never leaves your machine)
- Streaming progress display
- Handles moderate libraries (samples 1000 tracks)

### AI Playlist Options

| Flag | Description | Default |
| ---- | ----------- | ------- |
| `--prompt` | Natural language description | Required |
| `--library` | Path to music library | Required |
| `--ai-backend` | Backend: `claude` or `llamacpp` | `claude` |
| `--ai-model` | Path to GGUF model (llamacpp only) | None |
| `--ai-threads` | CPU threads for llamacpp | `4` |
| `--ai-context-size` | Context size for llamacpp | `2048` |
| `--force-scan` | Force metadata rescan (ignore cache) | Off |
| `--verbose` | Enable debug logging to file | Off |
| `--shuffle` | Shuffle the generated playlist | Off |

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

# Era/style-based
--prompt "80s synth-pop classics"
--prompt "90s grunge and alternative rock"
--prompt "modern indie folk"

# Activity-based
--prompt "high-energy workout music"
--prompt "relaxing music for meditation"
--prompt "party songs to dance to"

# Specific requests
--prompt "songs similar to Pink Floyd"
--prompt "instrumental music without vocals"
--prompt "upbeat songs under 3 minutes"
```

### How It Works

1. **Metadata Extraction**: Scans your library and extracts track metadata (title, artist, album, genre, year)
2. **Caching**: Saves metadata to `~/.cache/cli-player/` for fast subsequent runs
3. **Sampling**: For large libraries, randomly samples tracks to stay within AI token limits
4. **AI Curation**: Sends prompt + track list to AI, receives playlist indices
5. **Playback**: Loads selected tracks and starts playing with auto-advance

### Comparison: Claude vs llama.cpp

| Feature | Claude | llama.cpp |
| ------- | ------ | --------- |
| **Setup** | API key only | Download model (~1-5GB) |
| **Cost** | Pay per request | Free |
| **Speed** | Fast (2-5 seconds) | Slower (10-60 seconds) |
| **Quality** | Excellent | Good to Very Good |
| **Offline** | No | Yes |
| **Privacy** | Data sent to API | Fully local |
| **Library size** | Samples up to 1500 tracks | Samples up to 1000 tracks |

### Troubleshooting

**"ANTHROPIC_API_KEY not set"**

```bash
export ANTHROPIC_API_KEY="your-key-here"
# Add to ~/.bashrc or ~/.zshrc to make permanent
```

**"Model file not found"**

- Verify the model path is correct
- Use absolute paths: `--ai-model=/home/user/models/model.gguf`

**"Prompt too long" (llamacpp)**

- Use a smaller library or subdirectory
- Reduce context size: `--ai-context-size=2048`
- The player automatically samples large libraries

**Poor quality playlists**

- Be more specific in your prompt
- For llamacpp: try a larger/better model (e.g., Mistral-7B instead of TinyLlama)
- Check that your music library has proper metadata tags
- Use `--verbose` to see the exact prompt being sent to the AI

**Debugging with `--verbose`**

The `--verbose` flag enables detailed logging to help debug issues:

```bash
./cli-player --prompt "upbeat rock" --library ~/Music --verbose
```

When verbose mode is enabled:

- Debug information is written to `~/.cache/cli-player/cli-player.log`
- Logs include:
  - Complete AI prompts sent to backends
  - Track sampling details
  - API requests/responses (Claude backend)
  - Model loading and inference info (llama.cpp backend)
  - Error details and stack traces

View the log file:

```bash
tail -f ~/.cache/cli-player/cli-player.log
```

The log file is useful for:

- Understanding what metadata is sent to the AI
- Debugging playlist generation issues
- Tracking API errors or model loading problems
- Sharing diagnostic info when reporting bugs

## Controls

Once running, use single-key commands (no need to press Enter):

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
| `n` | Next track (directory mode) |
| `h` | Show help |
| `q` | Quit |

## Status Display

The player shows a single updating status line:

```
[Playing] song.mp3 | 01:23 / 03:45 | Vol: 70% | Track 3/12
```

- **State**: Playing, Paused, or Stopped
- **Filename**: Current track name
- **Position/Duration**: Current playback position and total duration
- **Volume**: Current volume level (0-100%)
- **Track**: Current track number (directory mode only)

## Supported Formats

The player supports the following audio formats via miniaudio:

- **WAV** - Uncompressed audio
- **MP3** - MPEG Audio Layer 3
- **FLAC** - Free Lossless Audio Codec
- **OGG** - Ogg Vorbis

## Example Session

```bash
$ ./cli-player -d ~/Music/Favorites --shuffle
Found 15 audio file(s) in directory
  1. track-07.mp3
  2. track-03.mp3
  3. track-11.mp3
  ...

CLI Audio Player - Press 'h' for help, 'p' to play

[Playing] track-07.mp3 | 00:15 / 03:42 | Vol: 25% | Track 1/15
```

Press `+` a few times to increase volume, `n` to skip to next track, `q` to quit.

## Design Goals

This player prioritizes:

1. **Minimal dependencies**: Everything needed is fetched automatically by CMake
2. **Fast startup**: No complex initialization or UI rendering
3. **Simple codebase**: Easy to understand and modify
4. **Reliable playback**: Uses proven miniaudio library for robust audio handling

What this player is NOT:

- Not a feature-rich player like mpg123 or VLC
- Not a music library manager
- Not a visualizer or analyzer
- Not configurable (by design)

## Technical Details

- **Audio backend**: [miniaudio](https://github.com/mackron/miniaudio) (header-only, single-file audio library)
- **Command parsing**: [cxxopts](https://github.com/jarro2783/cxxopts) (header-only)
- **Terminal control**: [colors](https://github.com/ShakaUVM/colors) (header-only, for raw mode and input)
- **Metadata extraction**: [TagLib](https://github.com/taglib/taglib) (audio file metadata)
- **JSON parsing**: [nlohmann/json](https://github.com/nlohmann/json)
- **HTTP client**: [cpp-httplib](https://github.com/yhirose/cpp-httplib) (for Claude API)
- **Local LLM**: [llama.cpp](https://github.com/ggerganov/llama.cpp) (offline inference)
- **Build system**: CMake with FetchContent
- **Language**: C++20

### Architecture

The player uses a multi-backend architecture for AI playlist generation:

```
AIBackend (interface)
    ├── ClaudeBackend - Cloud API using Anthropic's Claude
    └── LlamaCppBackend - Local inference using llama.cpp

AIPromptBuilder - Shared prompt generation and parsing logic
MetadataCache - Persistent caching of track metadata
```

This design makes it easy to add new backends (OpenAI, Ollama, Groq, etc.) in the future.

## License

This project is provided as-is for educational and personal use.

## Contributing

This is intended to be a minimal reference implementation. If you want more features, consider forking or using a more full-featured player.
