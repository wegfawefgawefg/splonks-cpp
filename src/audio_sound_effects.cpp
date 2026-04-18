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

std::size_t SoundEffectIndex(SoundEffect sound_effect) {
    return static_cast<std::size_t>(sound_effect);
}

[[noreturn]] void ThrowAudioError(const char* message) {
    throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

float ClampPanHalfWidthPx(float half_width_px) {
    return std::max(half_width_px, kMinPanHalfWidthPx);
}

MIX_StereoGains BuildTrackStereoGains(
    const Vec2& listener_world_pos,
    const Vec2& sound_world_pos,
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

SDL_PropertiesID MakeSoundEffectPlayProperties(int loops) {
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
    audio_detail::SoundEffectTrackRuntime& runtime,
    const SoundEffectPlaybackParams& params,
    float sound_effects_volume,
    const Vec2& listener_world_pos,
    float pan_half_width_px
) {
    const float gain =
        std::clamp(sound_effects_volume * params.volume_scale, 0.0F, 1.0F);
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
}

} // namespace

void Audio::InitializeSoundEffectTrack(MIX_Track* track) {
    if (!MIX_SetTrackStereo(track, &kUnityStereoGains)) {
        ThrowAudioError("MIX_SetTrackStereo for sound effect track failed");
    }

    auto runtime = std::make_unique<audio_detail::SoundEffectTrackRuntime>();
    runtime->track = track;
    runtime->slot_index =
        static_cast<std::uint32_t>(sound_effect_track_runtimes.size());

    if (!MIX_SetTrackCookedCallback(
            track,
            &Audio::OnSoundEffectTrackCooked,
            runtime.get())) {
        ThrowAudioError("MIX_SetTrackCookedCallback for sound effect track failed");
    }
    if (!MIX_SetTrackStoppedCallback(
            track,
            &Audio::OnSoundEffectTrackStopped,
            runtime.get())) {
        ThrowAudioError("MIX_SetTrackStoppedCallback for sound effect track failed");
    }

    sound_effect_tracks.push_back(track);
    sound_effect_track_runtimes.push_back(std::move(runtime));
}

void Audio::PlaySoundEffect(SoundEffect sound_effect, float volume_scale) {
    SoundEffectPlaybackParams params;
    params.volume_scale = volume_scale;
    (void)PlaySoundEffectInstance(sound_effect, params);
}

SoundEffectInstanceHandle Audio::PlaySoundEffectInstance(
    SoundEffect sound_effect,
    const SoundEffectPlaybackParams& params
) {
    if (!initialized || sound_effect_track_runtimes.empty()) {
        return {};
    }

    LoadedSound& loaded_sound = sounds[SoundEffectIndex(sound_effect)];
    if (loaded_sound.audio == nullptr) {
        return {};
    }

    audio_detail::SoundEffectTrackRuntime* runtime = nullptr;
    for (std::size_t offset = 0; offset < sound_effect_track_runtimes.size(); ++offset) {
        const std::size_t index =
            (next_sound_effect_track + offset) % sound_effect_track_runtimes.size();
        audio_detail::SoundEffectTrackRuntime& candidate =
            *sound_effect_track_runtimes[index];
        if (!candidate.active.load(std::memory_order_relaxed) ||
            !MIX_TrackPlaying(candidate.track)) {
            runtime = &candidate;
            next_sound_effect_track =
                (index + 1) % sound_effect_track_runtimes.size();
            break;
        }
    }

    if (runtime == nullptr) {
        runtime = sound_effect_track_runtimes[next_sound_effect_track].get();
        next_sound_effect_track =
            (next_sound_effect_track + 1) % sound_effect_track_runtimes.size();
    }

    if (!MIX_SetTrackAudio(runtime->track, loaded_sound.audio)) {
        return {};
    }

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

    SDL_PropertiesID properties = MakeSoundEffectPlayProperties(params.loops);
    const bool play_ok = MIX_PlayTrack(runtime->track, properties);
    if (properties != 0) {
        SDL_DestroyProperties(properties);
    }
    if (!play_ok) {
        runtime->active.store(false, std::memory_order_relaxed);
        return {};
    }

    return SoundEffectInstanceHandle{
        .slot_index = runtime->slot_index,
        .generation = generation,
    };
}

bool Audio::UpdateSoundEffectInstance(
    SoundEffectInstanceHandle handle,
    const SoundEffectPlaybackParams& params
) {
    audio_detail::SoundEffectTrackRuntime* const runtime =
        GetSoundEffectTrackRuntime(handle);
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

bool Audio::StopSoundEffectInstance(SoundEffectInstanceHandle handle) {
    audio_detail::SoundEffectTrackRuntime* const runtime =
        GetSoundEffectTrackRuntime(handle);
    if (runtime == nullptr) {
        return false;
    }

    MIX_StopTrack(runtime->track, 0);
    return true;
}

bool Audio::IsSoundEffectInstancePlaying(SoundEffectInstanceHandle handle) const {
    const audio_detail::SoundEffectTrackRuntime* const runtime =
        GetSoundEffectTrackRuntime(handle);
    return runtime != nullptr;
}

void Audio::SetListenerWorldPos(const Vec2& world_pos) {
    listener_world_pos = world_pos;
}

void Audio::SetPanHalfWidthPx(float half_width_px) {
    this->pan_half_width_px = ClampPanHalfWidthPx(half_width_px);
}

audio_detail::SoundEffectTrackRuntime* Audio::GetSoundEffectTrackRuntime(
    SoundEffectInstanceHandle handle
) {
    if (!handle.IsValid() ||
        handle.slot_index >= sound_effect_track_runtimes.size()) {
        return nullptr;
    }

    audio_detail::SoundEffectTrackRuntime* const runtime =
        sound_effect_track_runtimes[handle.slot_index].get();
    if (runtime == nullptr ||
        !runtime->active.load(std::memory_order_relaxed) ||
        runtime->generation.load(std::memory_order_relaxed) != handle.generation ||
        !MIX_TrackPlaying(runtime->track)) {
        return nullptr;
    }

    return runtime;
}

const audio_detail::SoundEffectTrackRuntime* Audio::GetSoundEffectTrackRuntime(
    SoundEffectInstanceHandle handle
) const {
    if (!handle.IsValid() ||
        handle.slot_index >= sound_effect_track_runtimes.size()) {
        return nullptr;
    }

    const audio_detail::SoundEffectTrackRuntime* const runtime =
        sound_effect_track_runtimes[handle.slot_index].get();
    if (runtime == nullptr ||
        !runtime->active.load(std::memory_order_relaxed) ||
        runtime->generation.load(std::memory_order_relaxed) != handle.generation ||
        !MIX_TrackPlaying(runtime->track)) {
        return nullptr;
    }

    return runtime;
}

void SDLCALL Audio::OnSoundEffectTrackStopped(void* userdata, MIX_Track* track) {
    (void)track;

    auto* const runtime =
        static_cast<audio_detail::SoundEffectTrackRuntime*>(userdata);
    if (runtime == nullptr) {
        return;
    }

    runtime->active.store(false, std::memory_order_relaxed);
    runtime->stereo_target_left.store(1.0F, std::memory_order_relaxed);
    runtime->stereo_target_right.store(1.0F, std::memory_order_relaxed);
    runtime->low_pass_enabled.store(false, std::memory_order_relaxed);
    runtime->low_pass_target_cutoff_hz.store(
        audio_filter::kMaxLowPassCutoffHz,
        std::memory_order_relaxed
    );
    runtime->low_pass_target_wet.store(0.0F, std::memory_order_relaxed);
}

void SDLCALL Audio::OnSoundEffectTrackCooked(
    void* userdata,
    MIX_Track* track,
    const SDL_AudioSpec* spec,
    float* pcm,
    int samples
) {
    (void)track;

    auto* const runtime =
        static_cast<audio_detail::SoundEffectTrackRuntime*>(userdata);
    if (runtime == nullptr || spec == nullptr || pcm == nullptr || samples <= 0) {
        return;
    }

    const std::uint32_t generation =
        runtime->generation.load(std::memory_order_relaxed);
    const float target_left =
        runtime->stereo_target_left.load(std::memory_order_relaxed);
    const float target_right =
        runtime->stereo_target_right.load(std::memory_order_relaxed);
    const bool low_pass_enabled =
        runtime->low_pass_enabled.load(std::memory_order_relaxed);
    const float target_cutoff_hz =
        runtime->low_pass_target_cutoff_hz.load(std::memory_order_relaxed);
    const float target_wet =
        runtime->low_pass_target_wet.load(std::memory_order_relaxed);

    if (runtime->stereo_pan_processor.callback_generation != generation) {
        runtime->stereo_pan_processor.Reset(generation, target_left, target_right);
    }
    if (runtime->low_pass_processor.callback_generation != generation) {
        runtime->low_pass_processor.Reset(
            generation,
            target_cutoff_hz,
            low_pass_enabled ? target_wet : 0.0F
        );
    }

    runtime->low_pass_processor.ProcessBlock(
        *spec,
        pcm,
        samples,
        low_pass_enabled,
        target_cutoff_hz,
        target_wet
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
