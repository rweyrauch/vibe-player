#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <chrono>

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    bool loadFile(const std::string& filename);
    void play();
    void pause();
    void stop();
    bool isPlaying() const;
    bool isPaused() const;
    void setVolume(float volume); // 0.0 to 1.0
    float getVolume() const;
    void seek(int position); // position in seconds
    int getPosition() const; // position in seconds
    int getDuration() const; // duration in seconds
    void cleanup();

private:
    Mix_Music* music;
    Mix_Chunk* chunk;
    bool isMusic; // true for music (MP3, FLAC, OGG), false for WAV
    bool playing;
    bool paused;
    float volume;
    int duration; // duration in seconds
    std::chrono::steady_clock::time_point startTime;
    int pausedPosition; // position when paused, in seconds

    void updatePosition();
};

#endif // PLAYER_H

