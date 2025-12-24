// Include stb_vorbis before miniaudio to enable OGG support
#include "extras/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "player.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <iostream>

void AudioPlayer::DataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    AudioPlayer *pPlayer = (AudioPlayer *)pDevice->pUserData;
    if (pPlayer == nullptr || !pPlayer->decoder_initialized_)
    {
        return;
    }

    // If paused, output silence
    if (pPlayer->paused_)
    {
        memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
        return;
    }

    // Decode frames with volume control
    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(&pPlayer->decoder_, pOutput, frameCount, &framesRead);

    // Apply volume
    if (pPlayer->volume_ < 1.0f)
    {
        float *pSamples = (float *)pOutput;
        ma_uint64 sampleCount = framesRead * pDevice->playback.channels;
        for (ma_uint64 i = 0; i < sampleCount; ++i)
        {
            pSamples[i] *= pPlayer->volume_;
        }
    }

    // If we read fewer frames than requested, we've reached the end
    if (framesRead < frameCount)
    {
        pPlayer->playing_ = false;
    }

    (void)pInput; // Unused
}

AudioPlayer::AudioPlayer()
{
    memset(&decoder_, 0, sizeof(decoder_));
    memset(&device_, 0, sizeof(device_));
}

AudioPlayer::~AudioPlayer()
{
    cleanup();
}

bool AudioPlayer::loadFile(const std::string &filename)
{
    cleanup();

    // Initialize decoder
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_result result = ma_decoder_init_file(filename.c_str(), &decoderConfig, &decoder_);

    if (result != MA_SUCCESS)
    {
        std::cerr << "Error loading audio file: " << filename << " (error code: " << result << ")" << std::endl;
        return false;
    }

    decoder_initialized_ = true;

    // Get duration
    ma_uint64 lengthInFrames;
    result = ma_decoder_get_length_in_pcm_frames(&decoder_, &lengthInFrames);
    if (result == MA_SUCCESS)
    {
        duration_ms_ = static_cast<int>(lengthInFrames / decoder_.outputSampleRate) * 1000; // convert to milliseconds
    }
    else
    {
        duration_ms_ = 0;
    }

    // Initialize device
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = decoder_.outputFormat;
    deviceConfig.playback.channels = decoder_.outputChannels;
    deviceConfig.sampleRate = decoder_.outputSampleRate;
    deviceConfig.dataCallback = DataCallback;
    deviceConfig.pUserData = this;

    result = ma_device_init(NULL, &deviceConfig, &device_);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Error initializing audio device" << std::endl;
        ma_decoder_uninit(&decoder_);
        decoder_initialized_ = false;
        return false;
    }

    device_initialized_ = true;

    return true;
}

void AudioPlayer::play()
{
    if (!decoder_initialized_ || !device_initialized_)
    {
        std::cerr << "No audio file loaded" << std::endl;
        return;
    }

    if (paused_)
    {
        // Resume from pause - decoder cursor is already at correct position
        paused_ = false;
        playing_ = true;
    }
    else if (!playing_)
    {
        // Start playback
        ma_result result = ma_device_start(&device_);
        if (result != MA_SUCCESS)
        {
            std::cerr << "Error starting playback" << std::endl;
            return;
        }

        playing_ = true;
        paused_ = false;
    }
}

void AudioPlayer::pause()
{
    if (playing_ && !paused_)
    {
        paused_ = true;
    }
}

void AudioPlayer::stop()
{
    if (playing_ || paused_)
    {
        if (device_initialized_)
        {
            ma_device_stop(&device_);
        }

        // Reset decoder to beginning
        if (decoder_initialized_)
        {
            ma_decoder_seek_to_pcm_frame(&decoder_, 0);
        }

        playing_ = false;
        paused_ = false;
    }
}

bool AudioPlayer::isPlaying() const
{
    return playing_ && !paused_ && device_initialized_ && ma_device_is_started(&device_);
}

bool AudioPlayer::isPaused() const
{
    return paused_;
}

void AudioPlayer::setVolume(float vol)
{
    volume_ = std::clamp(vol, 0.0f, 1.0f);
}

float AudioPlayer::getVolume() const
{
    return volume_;
}

int64_t AudioPlayer::getPosition() const
{
    if (!decoder_initialized_)
    {
        return 0;
    }

    // Get current cursor position from decoder
    ma_uint64 currentFrame = 0;
    ma_result result = ma_decoder_get_cursor_in_pcm_frames(&decoder_, &currentFrame);

    if (result != MA_SUCCESS)
    {
        return 0;
    }

    // Convert frames to milliseconds
    return static_cast<int64_t>((currentFrame * 1000) / decoder_.outputSampleRate);
}

int64_t AudioPlayer::getDuration() const
{
    return duration_ms_;
}

void AudioPlayer::seek(int64_t position)
{
    if (!decoder_initialized_)
    {
        return;
    }

    ma_uint64 targetFrame = static_cast<ma_uint64>(position / 1000) * decoder_.outputSampleRate;
    ma_result result = ma_decoder_seek_to_pcm_frame(&decoder_, targetFrame);

    if (result != MA_SUCCESS)
    {
        std::cerr << "Seek failed" << std::endl;
    }
}

void AudioPlayer::cleanup()
{
    stop();

    if (device_initialized_)
    {
        ma_device_uninit(&device_);
        device_initialized_ = false;
    }

    if (decoder_initialized_)
    {
        ma_decoder_uninit(&decoder_);
        decoder_initialized_ = false;
    }

    duration_ms_ = 0;
}
