#pragma once

#include <SDL3/SDL_audio.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace splonks::audio_filter {

constexpr std::size_t kMaxTrackChannels = 8;
constexpr float kMinLowPassCutoffHz = 40.0F;
constexpr float kMaxLowPassCutoffHz = 20000.0F;

float ClampLowPassCutoffHz(float cutoff_hz);
float ClampLowPassWet(float wet);
float ClampStereoGain(float gain);

struct StereoPanProcessor {
    std::uint32_t callback_generation = 0;
    bool initialized = false;
    float current_left = 1.0F;
    float current_right = 1.0F;

    void Reset(std::uint32_t generation, float left, float right);
    void ProcessBlock(
        const SDL_AudioSpec& spec,
        float* pcm,
        int samples,
        float target_left,
        float target_right
    );
};

struct LowPassProcessor {
    std::uint32_t callback_generation = 0;
    bool initialized = false;
    float current_cutoff_hz = kMaxLowPassCutoffHz;
    float current_wet = 0.0F;
    std::array<float, kMaxTrackChannels> history{};

    void Reset(std::uint32_t generation, float cutoff_hz, float wet);
    void ProcessBlock(
        const SDL_AudioSpec& spec,
        float* pcm,
        int samples,
        bool enabled,
        float target_cutoff_hz,
        float target_wet
    );
};

} // namespace splonks::audio_filter
