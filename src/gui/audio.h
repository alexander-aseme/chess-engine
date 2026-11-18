#pragma once
#include <memory>

namespace gui {

enum class Sound { Move, Capture, Castle, Check, Promote, GameEnd, Count };

// Plays the WAVs in assets/sounds/ through miniaudio. If no audio device is
// available, init() returns false and play() does nothing, so the game still runs.
class Audio {
public:
    Audio();
    ~Audio();

    bool init();
    void shutdown();
    void play(Sound s);
    void setVolume(float volume);  // 0..1, applies to every effect

private:
    float volume_ = 1.0f;  // kept so a volume set before init() still applies
    struct Impl;  // holds the miniaudio engine, so miniaudio.h stays out of this header
    std::unique_ptr<Impl> impl_;
};

}  // namespace gui
