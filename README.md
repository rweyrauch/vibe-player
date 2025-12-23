# Vibe Player - AI Music Player & Playlist Generator

**Note: This app was vibe-coded with Claude, including this document.**
**You have to love Claude's enthusiasm for documentation.**

> **🎵 "upbeat workout songs" → Perfect 25-track playlist in 10 seconds**
>
> Stop manually building playlists. Just describe what you want to hear.

**AI-powered music curation** meets Unix philosophy. A modular command-line audio toolset for Linux:

- **vibe-playlist**: Generate playlists from directories, files, or **AI prompts** ✨
- **vibe-player**: Play playlists with full interactive controls (simple CLI)
- **tui-player**: Beautiful terminal UI player with album art and rich display

```bash
# Just describe what you want to hear (with beautiful TUI and album art!)
./vibe-playlist --library ~/Music --prompt "upbeat workout music" | ./tui-player --stdin
```

## Philosophy

This toolset is designed around separation of concerns:

- **Do one thing well**: Each tool has a single, focused purpose
- **Composable**: Tools work together via pipes and files
- **Reusable output**: Generate playlists once, play many times
- **Minimal dependencies**: Only C++ standard library plus libraries fetched by CMake
- **Unix-style**: Follows conventions (stdout for data, stderr for messages)

## Features

### 🤖 AI-Powered Playlist Curation

**The killer feature:** Describe the music you want in plain English, and AI searches your entire library to create the perfect playlist.

- **"chill jazz for studying"** → 20 tracks of smooth, instrumental jazz
- **"high-energy 90s rock"** → Grunge and alternative hits
- **"upbeat workout music"** → Fast-tempo tracks to pump you up
- **"sad songs for rainy days"** → Melancholic, slow-tempo music

**How it works:**
- 🔍 **Full library search** - Claude searches your *entire* library (not just a sample!)
- 🎯 **Smart curation** - Uses tools to search by artist, genre, album, year, and title
- ⚡ **Fast** - Results in 5-15 seconds
- 🔒 **Private option** - Use llama.cpp for completely offline generation

### vibe-playlist (Playlist Generator)

- ✨ **AI-powered curation** from natural language descriptions
  - Cloud-based with Claude API (fast, high quality)
  - Offline with llama.cpp (private, no API key needed)
  - **Full library search** using Claude's tool use capabilities
- Generate playlists from directories or single files
- Metadata extraction and caching
- Shuffle support
- Output to stdout (for piping) or save to file

### vibe-player (Audio Player)

- Play playlists from files or stdin
- Full interactive controls (play, pause, seek, volume)
- Auto-advance through playlists
- Repeat mode
- Real-time status display with track metadata
- Direct single-file playback
- Support for WAV, MP3, FLAC, and OGG formats

### tui-player (Terminal UI Player)

**Enhanced visual music player with album art and rich terminal interface.**

- 🎨 **Beautiful terminal UI** powered by notcurses
- 🖼️ **Album art display** - Automatically extracts and displays cover art from MP3, FLAC, and M4A files
- 📊 **Rich status display** - Centered layout with song, artist, album, playback state
- 📈 **Visual progress bar** - See playback position at a glance
- 🎯 **Responsive design** - Adapts to terminal size, handles resize gracefully
- 🎨 **Sophisticated color scheme** - Easy-on-the-eyes non-primary colors
- ⌨️ **Same controls as vibe-player** - Familiar keyboard shortcuts
- 📦 **All vibe-player features** - Playlists, stdin, repeat mode, etc.
- 🖥️ **Minimum terminal size** - 30 columns × 15 rows (shows helpful error if too small)

## Requirements

- CMake 3.12 or higher
- C++20 compatible compiler (g++, clang++)
- Unix-like system (Linux, macOS, BSD)
- **For tui-player**: FFmpeg libraries (libavformat, libavcodec, libavutil) for album art support

**That's it!** No external libraries to install - CMake automatically fetches:

- miniaudio (audio playback)
- cxxopts (command-line parsing)
- colors (terminal control)
- TagLib (metadata extraction)
- nlohmann/json (JSON handling)
- cpp-httplib (HTTP client for Claude API)
- spdlog (logging)
- notcurses (terminal UI for tui-player)
- llama.cpp (optional: local LLM inference)

**Install FFmpeg (for tui-player album art):**
```bash
# Ubuntu/Debian
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev

# Fedora/RHEL
sudo dnf install ffmpeg-devel

# Arch Linux
sudo pacman -S ffmpeg
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

This creates three executables:
- `vibe-playlist` - Playlist generator
- `vibe-player` - Simple CLI player
- `tui-player` - Terminal UI player with album art

## Quick Start

### AI Playlists in 30 Seconds

1. **Set your API key** (get one free at [console.anthropic.com](https://console.anthropic.com)):
   ```bash
   export ANTHROPIC_API_KEY="sk-ant-your-key-here"
   ```

2. **Describe what you want to hear:**
   ```bash
   # Simple CLI player
   ./vibe-playlist --library ~/Music --prompt "upbeat workout songs" | ./vibe-player --stdin

   # Or use the beautiful TUI player with album art
   ./vibe-playlist --library ~/Music --prompt "upbeat workout songs" | ./tui-player --stdin
   ```

That's it! Claude will search your entire library and create a perfect playlist in seconds.

### Traditional Playlists

```bash
# From a directory
./vibe-playlist --directory ~/Music/Jazz > jazz.json
./vibe-player jazz.json
# or with TUI
./tui-player jazz.json

# Direct pipe
./vibe-playlist --directory ~/Music | ./vibe-player --stdin
# or with TUI
./vibe-playlist --directory ~/Music | ./tui-player --stdin
```

## Why AI Playlists?

### Traditional playlist tools force you to:
- ❌ Manually browse and select songs
- ❌ Remember which artists/albums fit the mood
- ❌ Use rigid filters (genre only, no context)
- ❌ Build playlists one song at a time

### With AI-powered generation:
- ✅ **Just describe what you want** - "sad songs for rainy days"
- ✅ **Understands context** - mood, energy, era, activity
- ✅ **Searches intelligently** - combines genre, artist, tempo, etc.
- ✅ **Full library coverage** - searches *all* your music, not a random sample
- ✅ **Fast results** - 5-15 seconds from prompt to playlist

### Real Examples

```bash
# Traditional approach:
# 1. Browse folders, 2. Filter by genre, 3. Manually add 20+ songs, 4. Hope it fits

# AI approach (one command):
./vibe-playlist --library ~/Music --prompt "energetic indie rock for coding"
```

**The AI understands:**
- "energetic" → high tempo, driving rhythm
- "indie rock" → genre + subculture
- "for coding" → no distracting lyrics, consistent energy

**Result:** A perfectly curated playlist in seconds, not hours.

## Usage

### vibe-playlist: Generate Playlists

**From a directory:**
```bash
# Output to stdout (for piping)
./vibe-playlist --directory ~/Music/Jazz

# Save to file
./vibe-playlist --directory ~/Music/Jazz --save jazz.json

# With shuffle
./vibe-playlist --directory ~/Music --shuffle > shuffled.json
```

**From a single file:**
```bash
./vibe-playlist --file song.mp3
```

**AI-powered generation (see [AI Playlist Generation](#ai-playlist-generation)):**
```bash
# Using Claude API (default)
./vibe-playlist --library ~/Music --prompt "chill vibes for studying"

# Using ChatGPT API
./vibe-playlist --library ~/Music --prompt "chill vibes for studying" --ai-backend chatgpt

# Save AI playlist
./vibe-playlist --library ~/Music --prompt "90s rock" --save rock90s.json

# With model selection
./vibe-playlist --library ~/Music --prompt "jazz" --claude-model balanced
./vibe-playlist --library ~/Music --prompt "jazz" --ai-backend chatgpt --chatgpt-model balanced
```

**Options:**
- `--directory <path>` - Generate from directory
- `--file <path>` - Generate from single file
- `--library <path>` - Music library for AI generation (required with --prompt)
- `--prompt <text>` - AI playlist generation
- `--ai-backend <type>` - AI backend: 'claude', 'chatgpt', 'llamacpp', or 'keyword' (default: claude)
- `--claude-model <model>` - Claude model: 'fast', 'balanced', 'best' or full model ID (default: fast)
- `--chatgpt-model <model>` - ChatGPT model: 'fast', 'balanced', 'best' or full model ID (default: fast)
- `--shuffle` - Shuffle the playlist
- `--save <file>` - Save to file (default: stdout)
- `--force-scan` - Force metadata rescan (ignore cache)
- `--verbose` - Enable debug logging

### vibe-player: Play Playlists

**From a file:**
```bash
./vibe-player playlist.json
```

**From stdin (piped):**
```bash
cat playlist.json | ./vibe-player --stdin
./vibe-playlist --directory ~/Music | ./vibe-player --stdin
```

**Direct file playback:**
```bash
./vibe-player --file song.mp3
```

**With options:**
```bash
# Repeat mode
./vibe-player playlist.json --repeat

# Non-interactive (auto-play, no controls)
./vibe-player playlist.json --no-interactive
```

**Options:**
- `<playlist_file>` - Playlist JSON file (positional)
- `--stdin` - Read playlist from stdin
- `--file <path>` - Play single audio file
- `--repeat` - Repeat playlist when finished
- `--no-interactive` - Disable interactive controls

### tui-player: Beautiful Terminal UI Player

**All the same usage as vibe-player, but with a rich visual interface!**

**From a file:**
```bash
./tui-player playlist.json
```

**From stdin (piped):**
```bash
cat playlist.json | ./tui-player --stdin
./vibe-playlist --library ~/Music --prompt "chill vibes" | ./tui-player --stdin
```

**Direct file playback:**
```bash
./tui-player --file song.mp3
```

**With options:**
```bash
# Repeat mode
./tui-player playlist.json --repeat

# Non-interactive (minimal UI, no controls)
./tui-player playlist.json --no-interactive
```

**Options:**
- Same as vibe-player (100% compatible)
- `<playlist_file>` - Playlist JSON file (positional)
- `--stdin` - Read playlist from stdin
- `--file <path>` - Play single audio file
- `--repeat` - Repeat playlist when finished
- `--no-interactive` - Disable interactive controls (shows minimal UI)

**Features:**
- 🎨 **Centered layout** - Album art, song info, and controls beautifully arranged
- 🖼️ **Automatic album art** - Extracted from MP3 ID3v2, FLAC, and M4A/MP4 files
- 📊 **Visual progress bar** - See exactly where you are in the track
- 🎯 **Adaptive UI** - Automatically adjusts to terminal size and resize
- ⌨️ **Help overlay** - Press `h` to show all keyboard shortcuts
- 🖥️ **Graceful degradation** - Shows helpful message if terminal is too small

**Tips:**
- For best experience, use a terminal with at least 80 columns × 24 rows
- Supports 256-color and true-color terminals
- Album art looks best in terminals with good Unicode support

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

## 🎵 AI Playlist Generation

**Stop browsing, start describing.** Generate perfect playlists from natural language using AI.

### 🌟 Claude Backend (Cloud API) - Recommended

**The most advanced music curation available.** Uses Anthropic's Claude API with intelligent **full library search**. Unlike traditional playlist generators that only sample your library, Claude uses sophisticated tool-calling to explore your *entire* music collection strategically.

**Setup:**

```bash
export ANTHROPIC_API_KEY="sk-ant-your-key-here"
```

Get your API key from [console.anthropic.com](https://console.anthropic.com)

**Usage:**

```bash
# Basic usage (fast model)
./vibe-playlist --library ~/Music --prompt "upbeat workout songs"

# With model selection
./vibe-playlist --library ~/Music --prompt "chill jazz" --claude-model balanced

# Model options:
#   fast     - Claude 3.5 Haiku (fastest, cheapest)
#   balanced - Claude 3.5 Sonnet (recommended - good quality/speed balance)
#   best     - Claude Sonnet 4.5 (highest quality, most capable)

# Save the playlist
./vibe-playlist --library ~/Music --prompt "90s alternative rock" --save 90s.json
```

**How it works:**

1. Claude gets tools to search your library by artist, genre, album, title, year
2. It strategically explores your music using multiple searches
3. Finds the best matches across your **entire library** (not just a sample!)
4. Returns a curated playlist

**Example:**
```bash
$ ./vibe-playlist --library ~/Music --prompt "upbeat workout songs" --claude-model balanced
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

### 🤖 ChatGPT Backend (OpenAI API)

**Alternative cloud-based AI** using OpenAI's ChatGPT with function calling. Like Claude, ChatGPT uses intelligent **full library search** to explore your entire music collection.

**Setup:**

```bash
export OPENAI_API_KEY="your-openai-api-key"
```

Get your API key from [platform.openai.com/api-keys](https://platform.openai.com/api-keys)

**Usage:**

```bash
# Basic usage (fast model)
./vibe-playlist --library ~/Music --prompt "upbeat workout songs" --ai-backend chatgpt

# With model selection
./vibe-playlist --library ~/Music --prompt "chill jazz" --ai-backend chatgpt --chatgpt-model balanced

# Model options:
#   fast     - GPT-4o Mini (fastest, cheapest)
#   balanced - GPT-4o (recommended - good quality/speed balance)
#   best     - GPT-4 (highest quality, most capable)

# Save the playlist
./vibe-playlist --library ~/Music --prompt "90s rock" --ai-backend chatgpt --save 90s.json
```

**How it works:**

1. ChatGPT gets the same search tools: artist, genre, album, title, year
2. Uses function calling to explore your music library strategically
3. Searches your **entire library** (not just a sample!)
4. Returns a curated playlist based on your prompt

**Example:**
```bash
$ ./vibe-playlist --library ~/Music --prompt "energetic indie rock" --ai-backend chatgpt --chatgpt-model balanced
Using cached metadata (7299 tracks)
Generating AI playlist using function calling...
ChatGPT is using functions to search the library...
Generated AI playlist with 28 tracks
```

**Features:**
- ✅ Searches entire library (no sampling)
- ✅ Function calling for intelligent search
- ✅ High-quality curation
- ✅ Fast (5-15 seconds)
- ✅ Automatic metadata caching
- ✅ Alternative to Claude with different "taste"

**When to use ChatGPT vs Claude:**
- **ChatGPT**: If you already have OpenAI credits, prefer OpenAI's models, or want a different AI's "musical taste"
- **Claude**: Generally more nuanced understanding, better reasoning about music context
- Both work excellently - try both and see which you prefer!

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
./vibe-playlist \
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

### Comparison: AI Backends

| Feature | Claude ⭐ | ChatGPT | llama.cpp |
| ------- | -------- | ------- | --------- |
| **Setup** | ANTHROPIC_API_KEY | OPENAI_API_KEY | Download 1-5GB model |
| **Cost** | ~$0.01 per playlist | ~$0.01 per playlist | Free |
| **Speed** | ⚡ Fast (5-15s) | ⚡ Fast (5-15s) | Slower (10-60s) |
| **Quality** | 🏆 Excellent | 🏆 Excellent | Good |
| **Library coverage** | 🔍 **Full library search** | 🔍 **Full library search** | ⚠️ Samples 1000 tracks |
| **Intelligence** | 🧠 Advanced tool use | 🧠 Function calling | Basic inference |
| **Offline** | No | No | ✅ Yes |
| **Privacy** | Data sent to Anthropic | Data sent to OpenAI | 🔒 Fully local |

**Why cloud AI (Claude/ChatGPT) is worth it:**
- 💎 **Quality difference is massive** - Understands nuance and context
- 🎯 **Searches your entire library** - No random sampling, finds the *perfect* tracks
- ⚡ **10x faster** - Get results while your coffee is still hot
- 💰 **Cheap** - ~1 cent per playlist (first playlists often free)

**Claude vs ChatGPT:**
- Both are excellent choices with similar capabilities
- **Claude**: Generally more nuanced reasoning about music and context
- **ChatGPT**: Great alternative if you prefer OpenAI or already have credits
- Try both and use whichever gives you better results for your taste!

**When to use llama.cpp:**
- No internet connection
- Privacy is critical (data never leaves your machine)
- You have a powerful CPU and time to spare

**Recommendation:** Start with Claude's `balanced` or ChatGPT's `balanced` model - the quality difference will blow you away. Switch to llama.cpp only if you need offline capability.

### ✨ Example Prompts - Get Creative!

The AI understands nuance, context, and combinations you couldn't express with traditional filters.

**Genre + Mood:**
```bash
--prompt "heavy metal with fast guitar solos and aggressive vocals"
--prompt "smooth jazz with saxophone, perfect for a dinner party"
--prompt "atmospheric electronic music with no vocals"
```

**Context-Aware:**
```bash
--prompt "sad songs for rainy days when feeling melancholic"
--prompt "uplifting songs to start the day with energy"
--prompt "intense focus music for coding - no lyrics, consistent tempo"
--prompt "relaxing music for meditation or yoga practice"
```

**Era + Style:**
```bash
--prompt "80s synth-pop classics with upbeat tempo"
--prompt "90s grunge and alternative rock, loud and raw"
--prompt "modern indie folk with acoustic guitar"
--prompt "early 2000s hip-hop with positive vibes"
```

**Activity-Based:**
```bash
--prompt "high-energy workout music, 150+ BPM"
--prompt "party songs to dance to, popular hits"
--prompt "background music for studying, instrumental only"
--prompt "driving music for long road trips"
```

**Similarity Matching:**
```bash
--prompt "songs similar to Pink Floyd - progressive and atmospheric"
--prompt "artists like Radiohead but more upbeat"
--prompt "music that sounds like The Beatles' later experimental work"
```

**Complex Combinations:**
```bash
--prompt "upbeat indie rock from the 2010s with female vocals"
--prompt "instrumental hip-hop beats, chill and jazzy"
--prompt "dark ambient electronic music for late night working"
--prompt "classic rock ballads, emotional and guitar-heavy"
```

**Pro tip:** The more specific you are, the better the results. Don't just say "rock" - say "energetic punk rock with fast drums and rebellious lyrics."

## Composing the Tools

The power of this design is in composition:

### Save and Reuse Playlists

```bash
# Generate once
./vibe-playlist --library ~/Music --prompt "workout music" --save workout.json

# Play many times
./vibe-player workout.json
./vibe-player workout.json --repeat
```

### Quick Playback

```bash
# Generate and play immediately
./vibe-playlist --directory ~/Music/Jazz | ./vibe-player --stdin
```

### Build Your Own Workflow

```bash
# Generate multiple playlists
./vibe-playlist --directory ~/Music/Rock --save rock.json
./vibe-playlist --library ~/Music --prompt "chill vibes" --save chill.json

# Choose which to play
./vibe-player rock.json
./vibe-player chill.json --repeat

# Combine with other tools
./vibe-playlist --directory ~/Music | jq '.tracks | length'  # Count tracks
```

## Interactive Controls

**Both vibe-player and tui-player use the same keyboard shortcuts.**

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
| `h` | Show help (tui-player shows overlay, vibe-player prints to stderr) |
| `q` | Quit |

## Status Display

### vibe-player (Simple CLI)

Shows a single updating status line with track metadata:

```
[Playing] Artist - Album - Song Title | 01:23 / 03:45 | Vol: 70% | Track 3/12
```

### tui-player (Terminal UI)

Shows a rich visual interface with:
- **Album art** - Cover art displayed in the center (if available)
- **Centered song info** - Song, artist, album in beautiful typography
- **Playback state** - Color-coded (mint green = playing, amber = paused, coral = stopped)
- **Visual progress bar** - Filled bar showing playback position
- **Time display** - Current position and total duration
- **Volume and track info** - Current volume and track number
- **Help overlay** - Press `h` to toggle keyboard shortcuts

All information updates in real-time with sophisticated non-primary colors for easy viewing.

## Troubleshooting

### Playlist Generation

**"ANTHROPIC_API_KEY not set"**
```bash
export ANTHROPIC_API_KEY="your-key-here"
# Add to ~/.bashrc or ~/.zshrc to make permanent
```

**"OPENAI_API_KEY not set"**
```bash
export OPENAI_API_KEY="your-key-here"
# Add to ~/.bashrc or ~/.zshrc to make permanent
```

**"Model file not found" (llama.cpp)**
- Verify the model path: `--ai-model=/absolute/path/to/model.gguf`
- Check file exists: `ls -lh ~/models/*.gguf`

**Poor quality playlists**
- Be more specific in your prompt
- For Claude: try `--claude-model balanced` or `best`
- For ChatGPT: try `--chatgpt-model balanced` or `best`
- For llama.cpp: use a larger model (Mistral-7B or Llama-3-8B)
- Ensure your music files have proper metadata tags

### Playback

**"Error loading audio file"**
- Check file exists and is readable
- Verify format is supported (WAV, MP3, FLAC, OGG)
- Try playing the file directly: `./vibe-player --file song.mp3`

**No sound**
- Check system volume
- Try increasing volume with `+` key
- Verify audio device is working: `aplay -l`

### Piping Issues

**"Failed to parse playlist from stdin"**
- Ensure vibe-playlist writes valid JSON: `./vibe-playlist ... | jq .`
- Check for errors on stderr: `./vibe-playlist ... 2>&1 | less`

## Debugging

Enable verbose logging for detailed diagnostics:

```bash
./vibe-playlist --library ~/Music --prompt "jazz" --verbose
```

Logs are written to:
- `~/.cache/vibe-playlist/vibe-playlist.log` (generator)
- `~/.cache/vibe-player/vibe-player.log` (player)

View logs:
```bash
tail -f ~/.cache/vibe-playlist/vibe-playlist.log
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
         │   vibe-playlist     │
         │  (Generator)        │
         ├─────────────────────┤
         │ • Directory scan    │
         │ • AI generation     │
         │ • Metadata cache    │
         │ • JSON output       │
         └──────────┬──────────┘
                    │
                    ▼ JSON (stdout or file)
                    │
         ┌──────────┴───────────┐
         │                      │
         ▼                      ▼
┌──────────────────┐   ┌──────────────────────┐
│   vibe-player    │   │    tui-player        │
│  (Simple CLI)    │   │  (Terminal UI)       │
├──────────────────┤   ├──────────────────────┤
│ • JSON parser    │   │ • JSON parser        │
│ • Audio playback │   │ • Audio playback     │
│ • Status line    │   │ • Album art display  │
│ • Keyboard UI    │   │ • Rich notcurses UI  │
│ • Playlist nav   │   │ • Visual progress    │
└──────────────────┘   │ • Responsive layout  │
                       └──────────────────────┘
```

**AI Backend Architecture:**

```
AIBackend (interface)
    ├── ClaudeBackend
    │   ├── Tool use for library search
    │   └── Multi-turn conversations
    │
    ├── ChatGPTBackend
    │   ├── Function calling for library search
    │   └── Multi-turn conversations
    │
    └── LlamaCppBackend
        └── Local inference with llama.cpp

LibrarySearch (used by all backends)
    ├── searchByArtist()
    ├── searchByGenre()
    ├── searchByAlbum()
    ├── searchByTitle()
    ├── searchByYearRange()
    └── getLibraryOverview()
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
- **Metadata**: [TagLib](https://github.com/taglib/taglib) (ID3, Vorbis, album art, etc.)
- **JSON**: [nlohmann/json](https://github.com/nlohmann/json)
- **HTTP**: [cpp-httplib](https://github.com/yhirose/cpp-httplib) (Claude API)
- **Logging**: [spdlog](https://github.com/gabime/spdlog)
- **Local LLM**: [llama.cpp](https://github.com/ggerganov/llama.cpp)
- **CLI parsing**: [cxxopts](https://github.com/jarro2783/cxxopts)
- **Terminal**: [colors](https://github.com/ShakaUVM/colors) (vibe-player)
- **Terminal UI**: [notcurses](https://github.com/dankamongmen/notcurses) (tui-player)
- **Multimedia**: FFmpeg (for album art decoding in tui-player)
- **Build**: CMake 3.12+ with FetchContent
- **Language**: C++20

### Project Structure

```
vibe-player/
├── common/                      # Shared libraries
│   ├── include/
│   │   ├── metadata.h          # Metadata structures
│   │   ├── metadata_cache.h    # Cache management
│   │   ├── playlist.h          # Playlist data structure
│   │   ├── player.h            # Audio playback engine
│   │   ├── ai_backend*.h       # AI backend interfaces
│   │   └── library_search.h    # Library search tools
│   └── src/                    # Implementation files
├── list/                        # vibe-playlist application
│   └── src/main.cpp
├── player/                      # vibe-player application (simple CLI)
│   └── src/main.cpp
├── tui_player/                  # tui-player application (terminal UI)
│   └── src/main.cpp            # Rich TUI with notcurses & album art
└── CMakeLists.txt               # Build configuration
```

## License

This project is provided as-is for educational and personal use.

## Contributing

This is intended to be a reference implementation showcasing Unix-style tool composition. Feel free to fork and extend!

## Credits

Built with ❤️ using excellent open-source libraries. Special thanks to:
- [miniaudio](https://github.com/mackron/miniaudio) for simple, portable audio
- [TagLib](https://github.com/taglib/taglib) for comprehensive metadata and album art support
- [notcurses](https://github.com/dankamongmen/notcurses) for the beautiful terminal UI
- [llama.cpp](https://github.com/ggerganov/llama.cpp) for efficient local LLM inference
- [Anthropic](https://anthropic.com) for the Claude API
- FFmpeg community for multimedia codecs
