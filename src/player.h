#ifndef PLAYER_H
#define PLAYER_H

#include <chrono>
#include <string>

#include "miniaudio.h"

class AudioPlayer
{
public:
    AudioPlayer();
    ~AudioPlayer();

    bool LoadFile(const std::string &filename);
    void Play();
    void Pause();
    void Stop();
    bool IsPlaying() const;
    bool IsPaused() const;
    void SetVolume(float volume); // 0.0 to 1.0
    float GetVolume() const;
    void Seek(int position); // position in seconds
    int GetPosition() const; // position in seconds
    int GetDuration() const; // duration in seconds
    void Cleanup();

private:
    ma_decoder decoder_;
    ma_device device_;
    bool decoder_initialized_;
    bool device_initialized_;
    bool playing_;
    bool paused_;
    float volume_;
    int duration_; // duration in seconds
    std::chrono::steady_clock::time_point start_time_;
    int paused_position_; // position when paused, in seconds
    ma_uint64 paused_frame_; // frame position when paused

    void UpdatePosition();

    static void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
};

#endif // PLAYER_H
