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

    bool loadFile(const std::string &filename);
    void play();
    void pause();
    void stop();
    bool isPlaying() const;
    bool isPaused() const;
    void setVolume(float volume); // 0.0 to 1.0
    float getVolume() const;
    void seek(int position); // position in milliseconds
    int getPosition() const; // position in milliseconds
    int getDuration() const; // duration in milliseconds
    void cleanup();

private:
    ma_decoder decoder_;
    ma_device device_;
    bool decoder_initialized_ = false;
    bool device_initialized_ = false;
    bool playing_ = false;
    bool paused_ = false;
    float volume_ = 0.5f; // volume level (0.0 to 1.0)
    int duration_ms_ = 0; // duration in milliseconds
    std::chrono::steady_clock::time_point start_time_;
    int paused_position_ = 0; // position when paused, in milliseconds
    ma_uint64 paused_frame_ = 0; // frame position when paused

    void updatePosition();

    static void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
};

#endif // PLAYER_H
