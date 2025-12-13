#include "player.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

AudioPlayer::AudioPlayer() 
    : music(nullptr), chunk(nullptr), isMusic(false),
      playing(false), paused(false), volume(1.0f), 
      duration(0), pausedPosition(0) {
}

AudioPlayer::~AudioPlayer() {
    cleanup();
}

bool AudioPlayer::loadFile(const std::string& filename) {
    cleanup();

    // Determine file type by extension
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "wav") {
        // Load as WAV chunk
        isMusic = false;
        chunk = Mix_LoadWAV(filename.c_str());
        if (chunk == nullptr) {
            std::cerr << "Error loading WAV file: " << Mix_GetError() << std::endl;
            return false;
        }
        
        // Estimate duration (rough calculation)
        // WAV files: sample_rate * channels * bytes_per_sample * duration = total_bytes
        // This is approximate since we don't have direct access to the audio spec
        duration = 0; // Will be calculated during playback
    } else {
        // Load as music (MP3, FLAC, OGG, etc.)
        isMusic = true;
        music = Mix_LoadMUS(filename.c_str());
        if (music == nullptr) {
            std::cerr << "Error loading audio file: " << Mix_GetError() << std::endl;
            return false;
        }
        
        // SDL2_mixer doesn't provide a direct way to get duration
        // We'll track it during playback
        duration = 0;
    }

    pausedPosition = 0;
    return true;
}

void AudioPlayer::play() {
    if (music == nullptr && chunk == nullptr) {
        std::cerr << "No audio file loaded" << std::endl;
        return;
    }

    if (paused) {
        if (isMusic) {
            Mix_ResumeMusic();
        } else {
            Mix_Resume(-1); // Resume all channels
        }
        paused = false;
        playing = true;
        startTime = std::chrono::steady_clock::now() - 
                    std::chrono::seconds(pausedPosition);
    } else if (!playing) {
        pausedPosition = 0;
        startTime = std::chrono::steady_clock::now();
        
        if (isMusic) {
            if (Mix_PlayMusic(music, 0) == -1) {
                std::cerr << "Error playing music: " << Mix_GetError() << std::endl;
                return;
            }
        } else {
            int channel = Mix_PlayChannel(-1, chunk, 0);
            if (channel == -1) {
                std::cerr << "Error playing chunk: " << Mix_GetError() << std::endl;
                return;
            }
        }
        
        playing = true;
        paused = false;
    }
    
    // Set volume
    setVolume(volume);
}

void AudioPlayer::pause() {
    if (playing && !paused) {
        if (isMusic) {
            Mix_PauseMusic();
        } else {
            Mix_Pause(-1); // Pause all channels
        }
        paused = true;
        updatePosition();
        pausedPosition = getPosition();
    }
}

void AudioPlayer::stop() {
    if (playing || paused) {
        if (isMusic) {
            Mix_HaltMusic();
        } else {
            Mix_HaltChannel(-1); // Stop all channels
        }
        playing = false;
        paused = false;
        pausedPosition = 0;
    }
}

bool AudioPlayer::isPlaying() const {
    if (isMusic) {
        return playing && !paused && Mix_PlayingMusic() != 0;
    } else {
        return playing && !paused && Mix_Playing(-1) != 0;
    }
}

bool AudioPlayer::isPaused() const {
    if (isMusic) {
        return paused && Mix_PausedMusic() != 0;
    } else {
        return paused && Mix_Paused(-1) != 0;
    }
}

void AudioPlayer::setVolume(float vol) {
    volume = std::max(0.0f, std::min(1.0f, vol));
    int mixVolume = (int)(MIX_MAX_VOLUME * volume);
    
    if (isMusic) {
        Mix_VolumeMusic(mixVolume);
    } else {
        if (chunk != nullptr) {
            Mix_VolumeChunk(chunk, mixVolume);
        }
    }
}

float AudioPlayer::getVolume() const {
    return volume;
}

int AudioPlayer::getPosition() const {
    if (!playing && !paused) {
        return pausedPosition;
    }
    
    if (paused) {
        return pausedPosition;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    return (int)elapsed;
}

int AudioPlayer::getDuration() const {
    // SDL2_mixer doesn't provide duration directly
    // This is a limitation - we'd need additional libraries for accurate duration
    // For now, return 0 or track it during playback
    return duration;
}

void AudioPlayer::seek(int position) {
    if (isMusic) {
        // SDL2_mixer has limited seeking support
        // Mix_SetMusicPosition() works for some formats but not all
        if (Mix_SetMusicPosition((double)position) == 0) {
            pausedPosition = position;
            if (playing && !paused) {
                startTime = std::chrono::steady_clock::now() - 
                           std::chrono::seconds(position);
            }
        } else {
            // Seeking not supported for this format, restart from position
            stop();
            pausedPosition = position;
        }
    } else {
        // For chunks, we can't seek - would need to reload
        // For simplicity, just restart
        stop();
        pausedPosition = position;
    }
}

void AudioPlayer::cleanup() {
    stop();
    
    if (music != nullptr) {
        Mix_FreeMusic(music);
        music = nullptr;
    }
    
    if (chunk != nullptr) {
        Mix_FreeChunk(chunk);
        chunk = nullptr;
    }
    
    pausedPosition = 0;
    duration = 0;
}

void AudioPlayer::updatePosition() {
    if (playing && !paused) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        pausedPosition = (int)elapsed;
    }
}
