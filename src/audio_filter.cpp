#include "audio_filter.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::audio_filter {

namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kParameterSmoothingHz = 24.0F;
constexpr float kStereoPanSmoothingHz = 96.0F;
constexpr float kGainSmoothingHz = 48.0F;

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

float ClampDirectGain(float gain) {
    return std::clamp(gain, 0.0F, 1.0F);
}

float ClampReverbDelayMs(float delay_ms) {
    return std::clamp(delay_ms, kMinReverbDelayMs, kMaxReverbDelayMs);
}

float ClampReverbFeedback(float feedback) {
    return std::clamp(feedback, 0.0F, 0.95F);
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

void GainProcessor::Reset(std::uint32_t generation, float gain) {
    callback_generation = generation;
    initialized = false;
    current_gain = ClampDirectGain(gain);
}

void GainProcessor::ProcessBlock(
    const SDL_AudioSpec& spec,
    float* pcm,
    int samples,
    float target_gain
) {
    if (pcm == nullptr || samples <= 0) {
        return;
    }

    const int channels = std::max(static_cast<int>(spec.channels), 1);
    const float desired_gain = ClampDirectGain(target_gain);
    const float sample_rate_hz = static_cast<float>(std::max(spec.freq, 1));
    const float smoothing_alpha =
        ComputeOnePoleAlpha(sample_rate_hz, kGainSmoothingHz);

    for (int sample_index = 0; sample_index < samples; sample_index += channels) {
        current_gain += (desired_gain - current_gain) * smoothing_alpha;
        for (int channel = 0; channel < channels; ++channel) {
            pcm[sample_index + channel] *= current_gain;
        }
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

void DelayReverbProcessor::Reset(
    std::uint32_t generation,
    float wet,
    float feedback,
    float delay_ms,
    float cutoff_hz
) {
    callback_generation = generation;
    initialized = false;
    current_wet = ClampLowPassWet(wet);
    current_feedback = ClampReverbFeedback(feedback);
    current_delay_ms = ClampReverbDelayMs(delay_ms);
    current_cutoff_hz = ClampLowPassCutoffHz(cutoff_hz);
    wet_history = 0.0F;
    write_index = 0;
    std::fill(delay_buffer.begin(), delay_buffer.end(), 0.0F);
}

void DelayReverbProcessor::ProcessBlock(
    const SDL_AudioSpec& spec,
    const float* input_pcm,
    float* output_pcm,
    int samples,
    bool enabled,
    float target_wet,
    float target_feedback,
    float target_delay_ms,
    float target_cutoff_hz
) {
    if (input_pcm == nullptr || output_pcm == nullptr || samples <= 0) {
        return;
    }

    const int channels = std::max(static_cast<int>(spec.channels), 1);
    const int desired_sample_rate = std::max(spec.freq, 1);
    const std::size_t desired_buffer_size = static_cast<std::size_t>(
        std::ceil((static_cast<double>(desired_sample_rate) * kMaxReverbDelayMs) / 1000.0)
    ) + 2U;
    if (buffer_sample_rate != desired_sample_rate || delay_buffer.size() != desired_buffer_size) {
        buffer_sample_rate = desired_sample_rate;
        delay_buffer.assign(desired_buffer_size, 0.0F);
        write_index = 0;
        wet_history = 0.0F;
        initialized = false;
    }
    if (delay_buffer.empty()) {
        return;
    }

    const float sample_rate_hz = static_cast<float>(desired_sample_rate);
    const float desired_wet = enabled ? ClampLowPassWet(target_wet) : 0.0F;
    const float desired_feedback = enabled ? ClampReverbFeedback(target_feedback) : 0.0F;
    const float desired_delay_ms = ClampReverbDelayMs(target_delay_ms);
    const float desired_cutoff_hz = ClampLowPassCutoffHz(target_cutoff_hz);
    const float smoothing_alpha =
        ComputeOnePoleAlpha(sample_rate_hz, kParameterSmoothingHz);

    for (int sample_index = 0; sample_index < samples; sample_index += channels) {
        current_wet += (desired_wet - current_wet) * smoothing_alpha;
        current_feedback += (desired_feedback - current_feedback) * smoothing_alpha;
        current_delay_ms += (desired_delay_ms - current_delay_ms) * smoothing_alpha;
        current_cutoff_hz += (desired_cutoff_hz - current_cutoff_hz) * smoothing_alpha;

        const std::size_t delay_samples = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::round((current_delay_ms * sample_rate_hz) / 1000.0F)),
            static_cast<std::size_t>(1),
            delay_buffer.size() - 1U
        );
        const std::size_t read_index =
            (write_index + delay_buffer.size() - delay_samples) % delay_buffer.size();

        float mono_input = 0.0F;
        for (int channel = 0; channel < channels; ++channel) {
            mono_input += input_pcm[sample_index + channel];
        }
        mono_input /= static_cast<float>(channels);

        const float delayed = delay_buffer[read_index];
        if (!initialized) {
            wet_history = delayed;
        }
        const float filter_alpha =
            ComputeOnePoleAlpha(sample_rate_hz, current_cutoff_hz);
        wet_history += filter_alpha * (delayed - wet_history);
        const float wet_sample = wet_history;

        delay_buffer[write_index] = mono_input + (wet_sample * current_feedback);
        for (int channel = 0; channel < channels; ++channel) {
            output_pcm[sample_index + channel] += wet_sample * current_wet;
        }

        write_index = (write_index + 1U) % delay_buffer.size();
        initialized = true;
    }
}

} // namespace splonks::audio_filter
