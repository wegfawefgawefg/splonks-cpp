#include "audio_filter.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::audio_filter {

namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kParameterSmoothingHz = 24.0F;
constexpr float kStereoPanSmoothingHz = 96.0F;

float ComputeOnePoleAlpha(float sample_rate_hz, float cutoff_hz) {
    const float clamped_sample_rate = std::max(sample_rate_hz, 1.0F);
    const float clamped_cutoff_hz = ClampLowPassCutoffHz(cutoff_hz);
    const float dt = 1.0F / clamped_sample_rate;
    const float rc = 1.0F / (2.0F * kPi * clamped_cutoff_hz);
    return dt / (rc + dt);
}

} // namespace

float ClampLowPassCutoffHz(float cutoff_hz) {
    return std::clamp(cutoff_hz, kMinLowPassCutoffHz, kMaxLowPassCutoffHz);
}

float ClampLowPassWet(float wet) {
    return std::clamp(wet, 0.0F, 1.0F);
}

float ClampStereoGain(float gain) {
    return std::clamp(gain, 0.0F, 1.0F);
}

void StereoPanProcessor::Reset(std::uint32_t generation, float left, float right) {
    callback_generation = generation;
    initialized = false;
    current_left = ClampStereoGain(left);
    current_right = ClampStereoGain(right);
}

void StereoPanProcessor::ProcessBlock(
    const SDL_AudioSpec& spec,
    float* pcm,
    int samples,
    float target_left,
    float target_right
) {
    if (pcm == nullptr || samples <= 0) {
        return;
    }

    const int channels = std::max(static_cast<int>(spec.channels), 1);
    const float desired_left = ClampStereoGain(target_left);
    const float desired_right = ClampStereoGain(target_right);
    const float sample_rate_hz = static_cast<float>(std::max(spec.freq, 1));
    const float smoothing_alpha =
        ComputeOnePoleAlpha(sample_rate_hz, kStereoPanSmoothingHz);

    for (int sample_index = 0; sample_index < samples; sample_index += channels) {
        current_left += (desired_left - current_left) * smoothing_alpha;
        current_right += (desired_right - current_right) * smoothing_alpha;

        if (channels == 1) {
            pcm[sample_index] *= 0.5F * (current_left + current_right);
            continue;
        }

        pcm[sample_index] *= current_left;
        pcm[sample_index + 1] *= current_right;
    }

    initialized = true;
}

void LowPassProcessor::Reset(std::uint32_t generation, float cutoff_hz, float wet) {
    callback_generation = generation;
    initialized = false;
    current_cutoff_hz = ClampLowPassCutoffHz(cutoff_hz);
    current_wet = ClampLowPassWet(wet);
    history.fill(0.0F);
}

void LowPassProcessor::ProcessBlock(
    const SDL_AudioSpec& spec,
    float* pcm,
    int samples,
    bool enabled,
    float target_cutoff_hz,
    float target_wet
) {
    if (pcm == nullptr || samples <= 0) {
        return;
    }

    const int channels = std::max(static_cast<int>(spec.channels), 1);
    const int processed_channels =
        std::min(channels, static_cast<int>(kMaxTrackChannels));
    if (processed_channels <= 0) {
        return;
    }

    const float desired_cutoff_hz = ClampLowPassCutoffHz(target_cutoff_hz);
    const float desired_wet = enabled ? ClampLowPassWet(target_wet) : 0.0F;
    const float sample_rate_hz = static_cast<float>(std::max(spec.freq, 1));
    const float smoothing_alpha =
        ComputeOnePoleAlpha(sample_rate_hz, kParameterSmoothingHz);

    for (int sample_index = 0; sample_index < samples; sample_index += channels) {
        current_cutoff_hz +=
            (desired_cutoff_hz - current_cutoff_hz) * smoothing_alpha;
        current_wet += (desired_wet - current_wet) * smoothing_alpha;

        const float filter_alpha =
            ComputeOnePoleAlpha(sample_rate_hz, current_cutoff_hz);
        for (int channel = 0; channel < processed_channels; ++channel) {
            const int pcm_index = sample_index + channel;
            const float input = pcm[pcm_index];
            if (!initialized) {
                history[static_cast<std::size_t>(channel)] = input;
            }

            float& last = history[static_cast<std::size_t>(channel)];
            last += filter_alpha * (input - last);
            pcm[pcm_index] = input + ((last - input) * current_wet);
        }

        initialized = true;
    }
}

} // namespace splonks::audio_filter
