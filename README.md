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

## Requirements

- CMake 3.12 or higher
- C++20 compatible compiler (g++, clang++)
- Unix-like system (Linux, macOS, BSD)

**That's it!** No external libraries to install - CMake automatically fetches:
- miniaudio (header-only audio library)
- cxxopts (header-only command-line parser)
- colors (header-only terminal control)

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

## Controls

Once running, use single-key commands (no need to press Enter):

| Key | Action |
|-----|--------|
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
- **Build system**: CMake with FetchContent
- **Language**: C++20

## License

This project is provided as-is for educational and personal use.

## Contributing

This is intended to be a minimal reference implementation. If you want more features, consider forking or using a more full-featured player.
