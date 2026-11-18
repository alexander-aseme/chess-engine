#include "gui/audio.h"

#include <cstdio>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING  // playback only
#define MA_NO_GENERATION
#include "miniaudio.h"

namespace gui {

namespace {

constexpr const char* kFiles[] = {
    "assets/sounds/move.wav",    "assets/sounds/capture.wav", "assets/sounds/castle.wav",
    "assets/sounds/check.wav",   "assets/sounds/promote.wav", "assets/sounds/game_end.wav",
};
static_assert(std::size(kFiles) == size_t(Sound::Count));

}  // namespace

struct Audio::Impl {
    ma_engine engine{};
    ma_sound sounds[size_t(Sound::Count)]{};
    bool loaded[size_t(Sound::Count)]{};
};

Audio::Audio() = default;
Audio::~Audio() { shutdown(); }

bool Audio::init() {
    auto impl = std::make_unique<Impl>();
    if (ma_engine_init(nullptr, &impl->engine) != MA_SUCCESS) {
        std::fprintf(stderr, "No audio device; sound effects are off.\n");
        return false;
    }
    for (size_t i = 0; i < size_t(Sound::Count); ++i) {
        // DECODE loads the whole file into memory up front, so playback never waits on disk.
        ma_result r = ma_sound_init_from_file(&impl->engine, kFiles[i], MA_SOUND_FLAG_DECODE,
                                              nullptr, nullptr, &impl->sounds[i]);
        impl->loaded[i] = r == MA_SUCCESS;
        if (!impl->loaded[i]) std::fprintf(stderr, "Could not load %s\n", kFiles[i]);
    }
    impl_ = std::move(impl);
    ma_engine_set_volume(&impl_->engine, volume_);
    return true;
}

void Audio::setVolume(float volume) {
    volume_ = volume;
    if (impl_) ma_engine_set_volume(&impl_->engine, volume);
}

void Audio::shutdown() {
    if (!impl_) return;
    for (size_t i = 0; i < size_t(Sound::Count); ++i)
        if (impl_->loaded[i]) ma_sound_uninit(&impl_->sounds[i]);
    ma_engine_uninit(&impl_->engine);
    impl_.reset();
}

void Audio::play(Sound s) {
    if (!impl_ || !impl_->loaded[size_t(s)]) return;
    ma_sound& sound = impl_->sounds[size_t(s)];
    ma_sound_seek_to_pcm_frame(&sound, 0);  // restart if it's still playing from last time
    ma_sound_start(&sound);
}

}  // namespace gui
