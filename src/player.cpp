#include "player.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <iostream>

AudioPlayer::AudioPlayer()
    : music_(nullptr), chunk_(nullptr), is_music_(false),
      playing_(false), paused_(false), volume_(1.0f),
      duration_(0), paused_position_(0),
      bass_level_(0.0f), mid_level_(0.0f), treble_level_(0.0f) {
  last_viz_update_ = std::chrono::steady_clock::now();
}

AudioPlayer::~AudioPlayer() {
  Cleanup();
}

bool AudioPlayer::LoadFile(const std::string& filename) {
  Cleanup();

  // Determine file type by extension
  std::string ext = filename.substr(filename.find_last_of(".") + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == "wav") {
    // Load as WAV chunk
    is_music_ = false;
    chunk_ = Mix_LoadWAV(filename.c_str());
    if (chunk_ == nullptr) {
      std::cerr << "Error loading WAV file: " << Mix_GetError() << std::endl;
      return false;
    }

    // Estimate duration (rough calculation)
    // WAV files: sample_rate * channels * bytes_per_sample * duration = total_bytes
    // This is approximate since we don't have direct access to the audio spec
    duration_ = 0;  // Will be calculated during playback
  } else {
    // Load as music (MP3, FLAC, OGG, etc.)
    is_music_ = true;
    music_ = Mix_LoadMUS(filename.c_str());
    if (music_ == nullptr) {
      std::cerr << "Error loading audio file: " << Mix_GetError() << std::endl;
      return false;
    }

    // SDL2_mixer doesn't provide a direct way to get duration
    // We'll track it during playback
    duration_ = 0;
  }

  paused_position_ = 0;
  return true;
}

void AudioPlayer::Play() {
  if (music_ == nullptr && chunk_ == nullptr) {
    std::cerr << "No audio file loaded" << std::endl;
    return;
  }

  if (paused_) {
    if (is_music_) {
      Mix_ResumeMusic();
    } else {
      Mix_Resume(-1);  // Resume all channels
    }
    paused_ = false;
    playing_ = true;
    start_time_ = std::chrono::steady_clock::now() -
                  std::chrono::seconds(paused_position_);
  } else if (!playing_) {
    paused_position_ = 0;
    start_time_ = std::chrono::steady_clock::now();

    if (is_music_) {
      if (Mix_PlayMusic(music_, 0) == -1) {
        std::cerr << "Error playing music: " << Mix_GetError() << std::endl;
        return;
      }
    } else {
      int channel = Mix_PlayChannel(-1, chunk_, 0);
      if (channel == -1) {
        std::cerr << "Error playing chunk: " << Mix_GetError() << std::endl;
        return;
      }
    }

    playing_ = true;
    paused_ = false;
  }

  // Set volume
  SetVolume(volume_);
}

void AudioPlayer::Pause() {
  if (playing_ && !paused_) {
    if (is_music_) {
      Mix_PauseMusic();
    } else {
      Mix_Pause(-1);  // Pause all channels
    }
    paused_ = true;
    UpdatePosition();
    paused_position_ = GetPosition();
  }
}

void AudioPlayer::Stop() {
  if (playing_ || paused_) {
    if (is_music_) {
      Mix_HaltMusic();
    } else {
      Mix_HaltChannel(-1);  // Stop all channels
    }
    playing_ = false;
    paused_ = false;
    paused_position_ = 0;
  }
}

bool AudioPlayer::IsPlaying() const {
  if (is_music_) {
    return playing_ && !paused_ && Mix_PlayingMusic() != 0;
  } else {
    return playing_ && !paused_ && Mix_Playing(-1) != 0;
  }
}

bool AudioPlayer::IsPaused() const {
  if (is_music_) {
    return paused_ && Mix_PausedMusic() != 0;
  } else {
    return paused_ && Mix_Paused(-1) != 0;
  }
}

void AudioPlayer::SetVolume(float vol) {
  volume_ = std::max(0.0f, std::min(1.0f, vol));
  int mix_volume = (int)(MIX_MAX_VOLUME * volume_);

  if (is_music_) {
    Mix_VolumeMusic(mix_volume);
  } else {
    if (chunk_ != nullptr) {
      Mix_VolumeChunk(chunk_, mix_volume);
    }
  }
}

float AudioPlayer::GetVolume() const {
  return volume_;
}

int AudioPlayer::GetPosition() const {
  if (!playing_ && !paused_) {
    return paused_position_;
  }

  if (paused_) {
    return paused_position_;
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      now - start_time_).count();
  return (int)elapsed;
}

int AudioPlayer::GetDuration() const {
  // SDL2_mixer doesn't provide duration directly
  // This is a limitation - we'd need additional libraries for accurate duration
  // For now, return 0 or track it during playback
  return duration_;
}

void AudioPlayer::Seek(int position) {
  if (is_music_) {
    // SDL2_mixer has limited seeking support
    // Mix_SetMusicPosition() works for some formats but not all
    if (Mix_SetMusicPosition((double)position) == 0) {
      paused_position_ = position;
      if (playing_ && !paused_) {
        start_time_ = std::chrono::steady_clock::now() -
                      std::chrono::seconds(position);
      }
    } else {
      // Seeking not supported for this format, restart from position
      Stop();
      paused_position_ = position;
    }
  } else {
    // For chunks, we can't seek - would need to reload
    // For simplicity, just restart
    Stop();
    paused_position_ = position;
  }
}

void AudioPlayer::Cleanup() {
  Stop();

  if (music_ != nullptr) {
    Mix_FreeMusic(music_);
    music_ = nullptr;
  }

  if (chunk_ != nullptr) {
    Mix_FreeChunk(chunk_);
    chunk_ = nullptr;
  }

  paused_position_ = 0;
  duration_ = 0;
}

void AudioPlayer::UpdatePosition() {
  if (playing_ && !paused_) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_).count();
    paused_position_ = (int)elapsed;
  }
}

void AudioPlayer::GetFrequencyLevels(float& bass, float& mid, float& treble) {
  // Update levels before returning
  const_cast<AudioPlayer*>(this)->UpdateFrequencyLevels();

  bass = bass_level_;
  mid = mid_level_;
  treble = treble_level_;
}

void AudioPlayer::UpdateFrequencyLevels() {
  if (!playing_ || paused_) {
    // Decay towards zero when not playing
    bass_level_ *= 0.9f;
    mid_level_ *= 0.9f;
    treble_level_ *= 0.9f;
    return;
  }

  // Simulate frequency content based on volume and time
  auto now = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - last_viz_update_).count();

  if (ms > 50) {  // Update every 50ms
    // Simple random walk with smoothing to create animated bars
    bass_level_ = bass_level_ * 0.7f +
                  (static_cast<float>(rand() % 100) / 100.0f) * 0.3f;
    mid_level_ = mid_level_ * 0.7f +
                 (static_cast<float>(rand() % 100) / 100.0f) * 0.3f;
    treble_level_ = treble_level_ * 0.7f +
                    (static_cast<float>(rand() % 100) / 100.0f) * 0.3f;

    // Apply volume scaling
    float vol_factor = volume_;
    bass_level_ *= vol_factor;
    mid_level_ *= vol_factor;
    treble_level_ *= vol_factor;

    // Clamp to 0.0-1.0 range
    bass_level_ = std::max(0.0f, std::min(1.0f, bass_level_));
    mid_level_ = std::max(0.0f, std::min(1.0f, mid_level_));
    treble_level_ = std::max(0.0f, std::min(1.0f, treble_level_));

    last_viz_update_ = now;
  }
}
