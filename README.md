# CLI Audio Player

A command-line audio media player application built with CMake for Linux.

## Features

- Play, pause, stop audio files
- Volume control
- Seek forward/backward
- Real-time playback status
- Support for WAV, MP3, and FLAC audio files

## Requirements

- CMake 3.12 or higher
- C++17 compatible compiler (g++, clang++)
- SDL2 development libraries
- SDL2_mixer development libraries

### Installing Dependencies

On Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install cmake build-essential libsdl2-dev libsdl2-mixer-dev
```

On Fedora/RHEL:
```bash
sudo dnf install cmake gcc-c++ SDL2-devel SDL2_mixer-devel
```

On Arch Linux:
```bash
sudo pacman -S cmake gcc sdl2 sdl2_mixer
```

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
./cli-player <audio_file>
```

### Play all audio files in a directory

```bash
./cli-player -d <directory>
./cli-player --directory <directory>
```

When playing a directory, the player will:
- Scan for all supported audio files (WAV, MP3, FLAC, OGG)
- Sort files alphabetically
- Automatically advance to the next track when the current one finishes
- Allow manual track navigation with the `next` command

### Commands

Once the player is running, you can use the following commands:

- `p` or `play` - Play audio
- `s` or `stop` - Stop audio
- `u` or `pause` - Pause audio
- `r` or `resume` - Resume audio
- `n` or `next` - Play next track (directory mode only)
- `+` or `volup` - Increase volume by 10%
- `-` or `voldown` - Decrease volume by 10%
- `f` or `forward` - Forward 10 seconds
- `b` or `backward` - Backward 10 seconds
- `i` or `info` - Show track information
- `h` or `help` - Show help message
- `q` or `quit` - Quit player

## Example

### Single file mode

```bash
./cli-player music.mp3
Now playing: music.mp3
Type 'h' or 'help' for commands
> p
Playing...
> +
Volume: 80%
> i
Track Info:
  File: music.mp3
  Duration: 03:45
  Position: 01:23
  Volume: 80%
  Status: Playing
> q
Goodbye!
```

### Directory mode

```bash
./cli-player -d ~/Music/Album
Found 12 audio file(s) in directory
  1. 01-intro.mp3
  2. 02-first-song.mp3
  3. 03-second-song.mp3
  ...

Now playing: 01-intro.mp3
Track 1 of 12
Type 'h' or 'help' for commands
> p
Playing...
> n

Now playing: 02-first-song.mp3
Track 2 of 12
> i
Track Info:
  File: 02-first-song.mp3
  Track: 2 of 12
  Duration: 03:45
  Position: 00:15
  Volume: 70%
  Status: Playing
> q
Goodbye!
```

## Supported Formats

The player supports the following audio formats:
- **WAV** - Uncompressed audio files
- **MP3** - MPEG Audio Layer 3
- **FLAC** - Free Lossless Audio Codec
- **OGG** - Ogg Vorbis (if supported by SDL2_mixer)

Note: Format support depends on SDL2_mixer's compiled codecs. Some distributions may require additional codec libraries for MP3 support.

## License

This project is provided as-is for educational purposes.

