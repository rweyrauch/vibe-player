#ifndef PLAYER_H
#define PLAYER_H

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
    void seek(int64_t position); // position in milliseconds
    int64_t getPosition() const; // position in milliseconds
    int64_t getDuration() const; // duration in milliseconds
    void cleanup();

private:
    mutable ma_decoder decoder_;  // mutable to allow cursor queries in const methods
    ma_device device_;
    bool decoder_initialized_ = false;
    bool device_initialized_ = false;
    bool playing_ = false;
    bool paused_ = false;
    float volume_ = 0.25f; // volume level (0.0 to 1.0)
    int64_t duration_ms_ = 0; // duration in milliseconds
    ma_uint64 paused_frame_ = 0; // frame position when paused

    static void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
};

#endif // PLAYER_H
