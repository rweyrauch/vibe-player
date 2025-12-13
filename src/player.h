#ifndef PLAYER_H
#define PLAYER_H

#include <chrono>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

class AudioPlayer {
 public:
  AudioPlayer();
  ~AudioPlayer();

  bool LoadFile(const std::string& filename);
  void Play();
  void Pause();
  void Stop();
  bool IsPlaying() const;
  bool IsPaused() const;
  void SetVolume(float volume);  // 0.0 to 1.0
  float GetVolume() const;
  void Seek(int position);  // position in seconds
  int GetPosition() const;  // position in seconds
  int GetDuration() const;  // duration in seconds
  void Cleanup();
  void GetFrequencyLevels(float& bass, float& mid, float& treble);

 private:
  Mix_Music* music_;
  Mix_Chunk* chunk_;
  bool is_music_;  // true for music (MP3, FLAC, OGG), false for WAV
  bool playing_;
  bool paused_;
  float volume_;
  int duration_;  // duration in seconds
  std::chrono::steady_clock::time_point start_time_;
  int paused_position_;  // position when paused, in seconds
  float bass_level_;     // 0.0 to 1.0
  float mid_level_;      // 0.0 to 1.0
  float treble_level_;   // 0.0 to 1.0
  std::chrono::steady_clock::time_point last_viz_update_;

  void UpdatePosition();
  void UpdateFrequencyLevels();
};

#endif  // PLAYER_H
