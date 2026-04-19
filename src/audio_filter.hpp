#pragma once

#include <SDL3/SDL_audio.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace splonks::audio_filter {

constexpr std::size_t kMaxTrackChannels = 8;
constexpr float kMinLowPassCutoffHz = 40.0F;
constexpr float kMaxLowPassCutoffHz = 20000.0F;
constexpr float kMinReverbDelayMs = 10.0F;
constexpr float kMaxReverbDelayMs = 350.0F;

float ClampLowPassCutoffHz(float cutoff_hz);
float ClampLowPassWet(float wet);
float ClampStereoGain(float gain);
float ClampDirectGain(float gain);
float ClampReverbDelayMs(float delay_ms);
float ClampReverbFeedback(float feedback);

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

struct GainProcessor {
    std::uint32_t callback_generation = 0;
    bool initialized = false;
    float current_gain = 1.0F;

    void Reset(std::uint32_t generation, float gain);
    void ProcessBlock(
        const SDL_AudioSpec& spec,
        float* pcm,
        int samples,
        float target_gain
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

struct DelayReverbProcessor {
    std::uint32_t callback_generation = 0;
    bool initialized = false;
    int buffer_sample_rate = 0;
    std::size_t write_index = 0;
    float current_wet = 0.0F;
    float current_feedback = 0.0F;
    float current_delay_ms = 80.0F;
    float current_cutoff_hz = kMaxLowPassCutoffHz;
    float wet_history = 0.0F;
    std::vector<float> delay_buffer{};

    void Reset(
        std::uint32_t generation,
        float wet,
        float feedback,
        float delay_ms,
        float cutoff_hz
    );
    void ProcessBlock(
        const SDL_AudioSpec& spec,
        const float* input_pcm,
        float* output_pcm,
        int samples,
        bool enabled,
        float target_wet,
        float target_feedback,
        float target_delay_ms,
        float target_cutoff_hz
    );
};

} // namespace splonks::audio_filter
