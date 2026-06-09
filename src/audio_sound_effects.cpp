#include "audio.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace splonks {

namespace {

constexpr float kMinPanHalfWidthPx = 1.0F;
constexpr float kHalfPi = 1.57079632679F;
constexpr MIX_StereoGains kUnityStereoGains{1.0F, 1.0F};

[[noreturn]] void ThrowAudioError(const char* message) {
    throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

float ClampPanHalfWidthPx(float half_width_px) {
    return std::max(half_width_px, kMinPanHalfWidthPx);
}

MIX_StereoGains BuildTrackStereoGains(
    const FVec2& listener_world_pos,
    const FVec2& sound_world_pos,
    float pan_half_width_px
) {
    const float dx = sound_world_pos.x - listener_world_pos.x;
    const float pan =
        std::clamp(dx / ClampPanHalfWidthPx(pan_half_width_px), -1.0F, 1.0F);
    const float t = (pan + 1.0F) * 0.5F;
    return MIX_StereoGains{
        .left = std::cos(t * kHalfPi),
        .right = std::sin(t * kHalfPi),
    };
}

SDL_PropertiesID MakeAudioInstancePlayProperties(int loops) {
    if (loops == 0) {
        return 0;
    }

    SDL_PropertiesID properties = SDL_CreateProperties();
    if (properties != 0) {
        SDL_SetNumberProperty(properties, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    }
    return properties;
}

void ApplyPlaybackParamsToTrack(
    MIX_Track* track,
    audio_detail::AudioInstanceTrackRuntime& runtime,
    const AudioPlaybackParams& params,
    float sound_effects_volume,
    const FVec2& listener_world_pos,
    float pan_half_width_px
) {
    const float gain = std::clamp(
        sound_effects_volume * runtime.asset_default_volume * params.volume_scale,
        0.0F,
        1.0F
    );
    MIX_SetTrackGain(track, gain);

    const MIX_StereoGains gains =
        params.positional
            ? BuildTrackStereoGains(listener_world_pos, params.world_pos, pan_half_width_px)
            : kUnityStereoGains;
    runtime.stereo_target_left.store(
        audio_filter::ClampStereoGain(gains.left),
        std::memory_order_relaxed
    );
    runtime.stereo_target_right.store(
        audio_filter::ClampStereoGain(gains.right),
        std::memory_order_relaxed
    );
    runtime.direct_gain.store(
        audio_filter::ClampDirectGain(params.direct_gain),
        std::memory_order_relaxed
    );

    runtime.low_pass_enabled.store(
        params.low_pass_enabled,
        std::memory_order_relaxed
    );
    runtime.low_pass_target_cutoff_hz.store(
        audio_filter::ClampLowPassCutoffHz(params.low_pass_cutoff_hz),
        std::memory_order_relaxed
    );
    runtime.low_pass_target_wet.store(
        audio_filter::ClampLowPassWet(params.low_pass_wet),
        std::memory_order_relaxed
    );
    runtime.reverb_enabled.store(
        params.reverb_enabled,
        std::memory_order_relaxed
    );
    runtime.reverb_target_wet.store(
        audio_filter::ClampLowPassWet(params.reverb_wet),
        std::memory_order_relaxed
    );
    runtime.reverb_target_feedback.store(
        audio_filter::ClampReverbFeedback(params.reverb_feedback),
        std::memory_order_relaxed
    );
    runtime.reverb_target_delay_ms.store(
        audio_filter::ClampReverbDelayMs(params.reverb_delay_ms),
        std::memory_order_relaxed
    );
    runtime.reverb_target_cutoff_hz.store(
        audio_filter::ClampLowPassCutoffHz(params.reverb_low_pass_cutoff_hz),
        std::memory_order_relaxed
    );
}

} // namespace

void Audio::InitializeAudioInstanceTrack(MIX_Track* track) {
    if (!MIX_SetTrackStereo(track, &kUnityStereoGains)) {
        ThrowAudioError("MIX_SetTrackStereo for audio instance track failed");
    }

    auto runtime = std::make_unique<audio_detail::AudioInstanceTrackRuntime>();
    runtime->track = track;
    runtime->slot_index =
        static_cast<std::uint32_t>(audio_instance_track_runtimes.size());

    if (!MIX_SetTrackCookedCallback(
            track,
            &Audio::OnAudioInstanceTrackCooked,
            runtime.get())) {
        ThrowAudioError("MIX_SetTrackCookedCallback for audio instance track failed");
    }
    if (!MIX_SetTrackStoppedCallback(
            track,
            &Audio::OnAudioInstanceTrackStopped,
            runtime.get())) {
        ThrowAudioError("MIX_SetTrackStoppedCallback for audio instance track failed");
    }

    audio_instance_tracks.push_back(track);
    audio_instance_track_runtimes.push_back(std::move(runtime));
}

void Audio::PlayAudioAsset(AudioAssetId asset_id, float volume_scale) {
    AudioPlaybackParams params;
    params.volume_scale = volume_scale;
    (void)PlayAudioAssetInstance(asset_id, params);
}

VID Audio::PlayAudioAssetInstance(
    AudioAssetId asset_id,
    const AudioPlaybackParams& params
) {
    if (!initialized || audio_instance_track_runtimes.empty()) {
        return kInvalidAudioInstanceVID;
    }

    LoadedAudioAsset* const loaded_asset = FindLoadedAudioAsset(asset_id);
    if (loaded_asset == nullptr || loaded_asset->audio == nullptr) {
        return kInvalidAudioInstanceVID;
    }

    audio_detail::AudioInstanceTrackRuntime* runtime = nullptr;
    for (std::size_t offset = 0; offset < audio_instance_track_runtimes.size(); ++offset) {
        const std::size_t index =
            (next_audio_instance_track + offset) % audio_instance_track_runtimes.size();
        audio_detail::AudioInstanceTrackRuntime& candidate =
            *audio_instance_track_runtimes[index];
        if (!candidate.active.load(std::memory_order_relaxed) ||
            !MIX_TrackPlaying(candidate.track)) {
            runtime = &candidate;
            next_audio_instance_track =
                (index + 1) % audio_instance_track_runtimes.size();
            break;
        }
    }

    if (runtime == nullptr) {
        runtime = audio_instance_track_runtimes[next_audio_instance_track].get();
        next_audio_instance_track =
            (next_audio_instance_track + 1) % audio_instance_track_runtimes.size();
    }

    if (!MIX_SetTrackAudio(runtime->track, loaded_asset->audio)) {
        return kInvalidAudioInstanceVID;
    }

    runtime->asset_default_volume = loaded_asset->default_volume;
    const std::uint32_t generation =
        runtime->generation.fetch_add(1, std::memory_order_relaxed) + 1;
    runtime->active.store(true, std::memory_order_relaxed);
    ApplyPlaybackParamsToTrack(
        runtime->track,
        *runtime,
        params,
        sound_effects_volume,
        listener_world_pos,
        pan_half_width_px
    );

    SDL_PropertiesID properties = MakeAudioInstancePlayProperties(params.loops);
    const bool play_ok = MIX_PlayTrack(runtime->track, properties);
    if (properties != 0) {
        SDL_DestroyProperties(properties);
    }
    if (!play_ok) {
        runtime->active.store(false, std::memory_order_relaxed);
        return kInvalidAudioInstanceVID;
    }

    return VID{
        .id = runtime->slot_index,
        .version = generation,
    };
}

bool Audio::UpdateAudioInstance(
    VID handle,
    const AudioPlaybackParams& params
) {
    audio_detail::AudioInstanceTrackRuntime* const runtime =
        GetAudioInstanceTrackRuntime(handle);
    if (runtime == nullptr) {
        return false;
    }

    ApplyPlaybackParamsToTrack(
        runtime->track,
        *runtime,
        params,
        sound_effects_volume,
        listener_world_pos,
        pan_half_width_px
    );
    return true;
}

bool Audio::StopAudioInstance(VID handle) {
    audio_detail::AudioInstanceTrackRuntime* const runtime =
        GetAudioInstanceTrackRuntime(handle);
    if (runtime == nullptr) {
        return false;
    }

    MIX_StopTrack(runtime->track, 0);
    return true;
}

bool Audio::IsAudioInstancePlaying(VID handle) const {
    const audio_detail::AudioInstanceTrackRuntime* const runtime =
        GetAudioInstanceTrackRuntime(handle);
    return runtime != nullptr;
}

void Audio::SetListenerWorldPos(const FVec2& world_pos) {
    listener_world_pos = world_pos;
}

void Audio::SetPanHalfWidthPx(float half_width_px) {
    this->pan_half_width_px = ClampPanHalfWidthPx(half_width_px);
}

audio_detail::AudioInstanceTrackRuntime* Audio::GetAudioInstanceTrackRuntime(
    VID handle
) {
    if (!IsValidAudioInstanceVID(handle) ||
        handle.id >= audio_instance_track_runtimes.size()) {
        return nullptr;
    }

    audio_detail::AudioInstanceTrackRuntime* const runtime =
        audio_instance_track_runtimes[handle.id].get();
    if (runtime == nullptr ||
        !runtime->active.load(std::memory_order_relaxed) ||
        runtime->generation.load(std::memory_order_relaxed) != handle.version ||
        !MIX_TrackPlaying(runtime->track)) {
        return nullptr;
    }

    return runtime;
}

const audio_detail::AudioInstanceTrackRuntime* Audio::GetAudioInstanceTrackRuntime(
    VID handle
) const {
    if (!IsValidAudioInstanceVID(handle) ||
        handle.id >= audio_instance_track_runtimes.size()) {
        return nullptr;
    }

    const audio_detail::AudioInstanceTrackRuntime* const runtime =
        audio_instance_track_runtimes[handle.id].get();
    if (runtime == nullptr ||
        !runtime->active.load(std::memory_order_relaxed) ||
        runtime->generation.load(std::memory_order_relaxed) != handle.version ||
        !MIX_TrackPlaying(runtime->track)) {
        return nullptr;
    }

    return runtime;
}

void SDLCALL Audio::OnAudioInstanceTrackStopped(void* userdata, MIX_Track* track) {
    (void)track;

    auto* const runtime =
        static_cast<audio_detail::AudioInstanceTrackRuntime*>(userdata);
    if (runtime == nullptr) {
        return;
    }

    runtime->active.store(false, std::memory_order_relaxed);
    runtime->asset_default_volume = 1.0F;
    runtime->stereo_target_left.store(1.0F, std::memory_order_relaxed);
    runtime->stereo_target_right.store(1.0F, std::memory_order_relaxed);
    runtime->direct_gain.store(1.0F, std::memory_order_relaxed);
    runtime->low_pass_enabled.store(false, std::memory_order_relaxed);
    runtime->low_pass_target_cutoff_hz.store(
        audio_filter::kMaxLowPassCutoffHz,
        std::memory_order_relaxed
    );
    runtime->low_pass_target_wet.store(0.0F, std::memory_order_relaxed);
    runtime->reverb_enabled.store(false, std::memory_order_relaxed);
    runtime->reverb_target_wet.store(0.0F, std::memory_order_relaxed);
    runtime->reverb_target_feedback.store(0.0F, std::memory_order_relaxed);
    runtime->reverb_target_delay_ms.store(80.0F, std::memory_order_relaxed);
    runtime->reverb_target_cutoff_hz.store(
        audio_filter::kMaxLowPassCutoffHz,
        std::memory_order_relaxed
    );
}

void SDLCALL Audio::OnAudioInstanceTrackCooked(
    void* userdata,
    MIX_Track* track,
    const SDL_AudioSpec* spec,
    float* pcm,
    int samples
) {
    (void)track;

    auto* const runtime =
        static_cast<audio_detail::AudioInstanceTrackRuntime*>(userdata);
    if (runtime == nullptr || spec == nullptr || pcm == nullptr || samples <= 0) {
        return;
    }

    const std::uint32_t generation =
        runtime->generation.load(std::memory_order_relaxed);
    const float target_left =
        runtime->stereo_target_left.load(std::memory_order_relaxed);
    const float target_right =
        runtime->stereo_target_right.load(std::memory_order_relaxed);
    const float target_direct_gain =
        runtime->direct_gain.load(std::memory_order_relaxed);
    const bool low_pass_enabled =
        runtime->low_pass_enabled.load(std::memory_order_relaxed);
    const float target_cutoff_hz =
        runtime->low_pass_target_cutoff_hz.load(std::memory_order_relaxed);
    const float target_wet =
        runtime->low_pass_target_wet.load(std::memory_order_relaxed);
    const bool reverb_enabled =
        runtime->reverb_enabled.load(std::memory_order_relaxed);
    const float reverb_target_wet =
        runtime->reverb_target_wet.load(std::memory_order_relaxed);
    const float reverb_target_feedback =
        runtime->reverb_target_feedback.load(std::memory_order_relaxed);
    const float reverb_target_delay_ms =
        runtime->reverb_target_delay_ms.load(std::memory_order_relaxed);
    const float reverb_target_cutoff_hz =
        runtime->reverb_target_cutoff_hz.load(std::memory_order_relaxed);

    if (runtime->stereo_pan_processor.callback_generation != generation) {
        runtime->stereo_pan_processor.Reset(generation, target_left, target_right);
    }
    if (runtime->gain_processor.callback_generation != generation) {
        runtime->gain_processor.Reset(generation, target_direct_gain);
    }
    if (runtime->low_pass_processor.callback_generation != generation) {
        runtime->low_pass_processor.Reset(
            generation,
            target_cutoff_hz,
            low_pass_enabled ? target_wet : 0.0F
        );
    }
    if (runtime->delay_reverb_processor.callback_generation != generation) {
        runtime->delay_reverb_processor.Reset(
            generation,
            reverb_enabled ? reverb_target_wet : 0.0F,
            reverb_target_feedback,
            reverb_target_delay_ms,
            reverb_target_cutoff_hz
        );
    }

    runtime->cooked_scratch.resize(static_cast<std::size_t>(samples));
    std::copy(pcm, pcm + samples, runtime->cooked_scratch.begin());

    runtime->low_pass_processor.ProcessBlock(
        *spec,
        pcm,
        samples,
        low_pass_enabled,
        target_cutoff_hz,
        target_wet
    );
    runtime->gain_processor.ProcessBlock(
        *spec,
        pcm,
        samples,
        target_direct_gain
    );
    runtime->delay_reverb_processor.ProcessBlock(
        *spec,
        runtime->cooked_scratch.data(),
        pcm,
        samples,
        reverb_enabled,
        reverb_target_wet,
        reverb_target_feedback,
        reverb_target_delay_ms,
        reverb_target_cutoff_hz
    );
    runtime->stereo_pan_processor.ProcessBlock(
        *spec,
        pcm,
        samples,
        target_left,
        target_right
    );
}

} // namespace splonks
