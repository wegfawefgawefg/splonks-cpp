#include "audio_emitters.hpp"

#include "audio_acoustics.hpp"
#include "ent.hpp"
#include "ents/common/common.hpp"
#include "ent/manager.hpp"
#include "graphics.hpp"
#include "state.hpp"

#include <cstdio>

namespace splonks {

namespace {

void ClearEmitterRuntime(AudioEmitter& emitter) {
    emitter.started = false;
    emitter.sound_instance_vid = kInvalidAudioInstanceVID;
}

void DeactivateEmitter(AudioEmitterManager& manager, AudioEmitter& emitter) {
    if (!emitter.active) {
        return;
    }

    emitter.active = false;
    ClearEmitterRuntime(emitter);
    emitter.owner_ent_vid.reset();
    emitter.attached_ent_vid.reset();
    manager.available_ids.insert(manager.available_ids.begin(), emitter.vid.id);
}

bool HandleMissingTarget(AudioEmitter& emitter, bool owner_missing) {
    switch (emitter.target_loss_policy) {
    case AudioEmitterTargetLossPolicy::StopImmediately:
        return false;
    case AudioEmitterTargetLossPolicy::FinishCurrentPlay:
        if (emitter.playback_mode == AudioEmitterPlaybackMode::Looping) {
            return false;
        }
        emitter.source_mode = AudioEmitterSourceMode::FixedWorldPos;
        emitter.attached_ent_vid.reset();
        emitter.attached_offset = Vec2::New(0.0F, 0.0F);
        if (owner_missing) {
            emitter.owner_ent_vid.reset();
        }
        return true;
    case AudioEmitterTargetLossPolicy::KeepPlayingDetached:
        emitter.source_mode = AudioEmitterSourceMode::FixedWorldPos;
        emitter.attached_ent_vid.reset();
        emitter.attached_offset = Vec2::New(0.0F, 0.0F);
        if (owner_missing) {
            emitter.owner_ent_vid.reset();
        }
        return true;
    }
    return false;
}

bool ResolveEmitterWorldPos(State& state, const Graphics& graphics, AudioEmitter& emitter) {
    if (emitter.owner_ent_vid.has_value() &&
        state.ents.GetEnt(*emitter.owner_ent_vid) == nullptr) {
        return HandleMissingTarget(emitter, true);
    }

    if (emitter.source_mode != AudioEmitterSourceMode::AttachedEnt ||
        !emitter.attached_ent_vid.has_value()) {
        return true;
    }

    const Ent* const attached = state.ents.GetEnt(*emitter.attached_ent_vid);
    if (attached != nullptr) {
        emitter.world_pos =
            ents::common::GetVisualCenterForEnt(*attached, graphics, attached->GetCenter()) +
            emitter.attached_offset;
        return true;
    }

    return HandleMissingTarget(emitter, false);
}

AudioPlaybackParams BuildEmitterPlaybackParams(
    State& state,
    const Graphics& graphics,
    const AudioEmitter& emitter
) {
    (void)graphics;
    AudioPlaybackParams params;
    params.volume_scale = emitter.volume_scale;
    params.positional = true;
    params.loops = emitter.playback_mode == AudioEmitterPlaybackMode::Looping ? -1 : 0;

    const PositionalAudioAcoustics acoustics = ComputePositionalAudioAcoustics(
        state,
        GetAudioListenerWorldPos(state),
        emitter.world_pos
    );
    params.world_pos = acoustics.wrapped_source_world_pos;
    params.direct_gain = acoustics.direct_gain;
    params.low_pass_enabled = acoustics.low_pass_enabled;
    params.low_pass_cutoff_hz = acoustics.low_pass_cutoff_hz;
    params.low_pass_wet = acoustics.low_pass_wet;
    params.reverb_enabled = acoustics.reverb_enabled;
    params.reverb_wet = acoustics.reverb_wet;
    params.reverb_feedback = acoustics.reverb_feedback;
    params.reverb_delay_ms = acoustics.reverb_delay_ms;
    params.reverb_low_pass_cutoff_hz = acoustics.reverb_low_pass_cutoff_hz;
    return params;
}

bool StartEmitterInstance(
    AudioEmitter& emitter,
    State& state,
    Audio& audio,
    const Graphics& graphics
) {
    const AudioPlaybackParams params = BuildEmitterPlaybackParams(state, graphics, emitter);
    emitter.sound_instance_vid = audio.PlayAudioAssetInstance(emitter.audio_asset_id, params);
    emitter.started = IsValidAudioInstanceVID(emitter.sound_instance_vid);
    return emitter.started;
}

} // namespace

AudioEmitterManager AudioEmitterManager::New() {
    AudioEmitterManager manager;
    manager.emitters.reserve(kMaxNumAudioEmitters);
    manager.available_ids.reserve(kMaxNumAudioEmitters);
    for (std::size_t i = 0; i < kMaxNumAudioEmitters; ++i) {
        const auto id = static_cast<std::uint32_t>(i);
        AudioEmitter emitter;
        emitter.vid = VID{id, 0};
        manager.emitters.push_back(emitter);
        manager.available_ids.insert(manager.available_ids.begin(), id);
    }
    return manager;
}

std::optional<VID> AudioEmitterManager::NewEmitter() {
    if (available_ids.empty()) {
        std::printf("Audio emitter budget bounce!\n");
        return std::nullopt;
    }

    const std::uint32_t id = available_ids.back();
    available_ids.pop_back();
    AudioEmitter& emitter = emitters[static_cast<std::size_t>(id)];
    emitter.active = true;
    emitter.vid.version += 1;
    emitter.started = false;
    emitter.sound_instance_vid = kInvalidAudioInstanceVID;
    emitter.owner_ent_vid.reset();
    emitter.attached_ent_vid.reset();
    emitter.attached_offset = Vec2::New(0.0F, 0.0F);
    emitter.world_pos = Vec2::New(0.0F, 0.0F);
    return emitter.vid;
}

void AudioEmitterManager::SetInactiveVid(const VID& vid) {
    AudioEmitter* const emitter = GetEmitterMut(vid);
    if (emitter == nullptr) {
        return;
    }
    DeactivateEmitter(*this, *emitter);
}

AudioEmitter* AudioEmitterManager::GetEmitterMut(const VID& vid) {
    if (!IsValidAudioEmitterVID(vid) ||
        static_cast<std::size_t>(vid.id) >= emitters.size()) {
        return nullptr;
    }

    AudioEmitter& emitter = emitters[static_cast<std::size_t>(vid.id)];
    if (!emitter.active || emitter.vid.version != vid.version) {
        return nullptr;
    }
    return &emitter;
}

const AudioEmitter* AudioEmitterManager::GetEmitter(const VID& vid) const {
    if (!IsValidAudioEmitterVID(vid) ||
        static_cast<std::size_t>(vid.id) >= emitters.size()) {
        return nullptr;
    }

    const AudioEmitter& emitter = emitters[static_cast<std::size_t>(vid.id)];
    if (!emitter.active || emitter.vid.version != vid.version) {
        return nullptr;
    }
    return &emitter;
}

void AudioEmitterManager::ClearAll() {
    available_ids.clear();
    for (std::size_t i = 0; i < emitters.size(); ++i) {
        emitters[i].active = false;
        ClearEmitterRuntime(emitters[i]);
        emitters[i].owner_ent_vid.reset();
        emitters[i].attached_ent_vid.reset();
        available_ids.insert(available_ids.begin(), static_cast<std::uint32_t>(i));
    }
}

const AudioEmitter* GetSoundEmitter(const State& state, VID emitter_vid) {
    return state.audio_emitters.GetEmitter(emitter_vid);
}

AudioEmitter* GetSoundEmitterMut(State& state, VID emitter_vid) {
    return state.audio_emitters.GetEmitterMut(emitter_vid);
}

Vec2 GetAudioListenerWorldPos(const State& state) {
    return state.audio_listener_world_pos;
}

void SetAudioListenerWorldPos(State& state, const Vec2& world_pos) {
    state.audio_listener_world_pos = world_pos;
}

std::optional<VID> FindOwnedSoundEmitter(
    const State& state,
    VID owner_ent_vid,
    AudioAssetId audio_asset_id,
    AudioEmitterPlaybackMode playback_mode
) {
    for (const AudioEmitter& emitter : state.audio_emitters.emitters) {
        if (!emitter.active || !emitter.owner_ent_vid.has_value()) {
            continue;
        }
        if (*emitter.owner_ent_vid == owner_ent_vid &&
            emitter.audio_asset_id == audio_asset_id &&
            emitter.playback_mode == playback_mode) {
            return emitter.vid;
        }
    }
    return std::nullopt;
}

/// Creates a fixed-position world emitter.
/// Use this for one-shots or detached ambience that should stay at a world point
/// while positional mix still updates as the listener moves.
std::optional<VID> PlayWorldSoundEmitter(
    State& state,
    const Vec2& world_pos,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params
) {
    const std::optional<VID> emitter_vid = state.audio_emitters.NewEmitter();
    if (!emitter_vid.has_value()) {
        return std::nullopt;
    }

    AudioEmitter* const emitter = state.audio_emitters.GetEmitterMut(*emitter_vid);
    if (emitter == nullptr) {
        return std::nullopt;
    }

    emitter->audio_asset_id = audio_asset_id;
    emitter->volume_scale = params.volume_scale;
    emitter->playback_mode = params.playback_mode;
    emitter->target_loss_policy = params.target_loss_policy;
    emitter->owner_ent_vid = params.owner_ent_vid;
    emitter->source_mode = AudioEmitterSourceMode::FixedWorldPos;
    emitter->world_pos = world_pos;
    return emitter->vid;
}

/// Creates an emitter that follows an ent each frame at ent center + attached_offset.
/// Use this when the sound source should move with an ent without manual position updates.
std::optional<VID> PlayAttachedSoundEmitter(
    State& state,
    VID attached_ent_vid,
    const Vec2& attached_offset,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params
) {
    const std::optional<VID> emitter_vid = state.audio_emitters.NewEmitter();
    if (!emitter_vid.has_value()) {
        return std::nullopt;
    }

    AudioEmitter* const emitter = state.audio_emitters.GetEmitterMut(*emitter_vid);
    if (emitter == nullptr) {
        return std::nullopt;
    }

    emitter->audio_asset_id = audio_asset_id;
    emitter->volume_scale = params.volume_scale;
    emitter->playback_mode = params.playback_mode;
    emitter->target_loss_policy = params.target_loss_policy;
    emitter->owner_ent_vid =
        params.owner_ent_vid.has_value() ? params.owner_ent_vid : std::optional<VID>(attached_ent_vid);
    emitter->source_mode = AudioEmitterSourceMode::AttachedEnt;
    emitter->attached_ent_vid = attached_ent_vid;
    emitter->attached_offset = attached_offset;
    if (const Ent* const attached = state.ents.GetEnt(attached_ent_vid)) {
        emitter->world_pos = attached->GetCenter() + attached_offset;
    }
    return emitter->vid;
}

std::optional<VID> PlayEntSoundEmitter(
    State& state,
    const Ent& ent,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params,
    const Vec2& attached_offset
) {
    AudioEmitterPlayParams ent_params = params;
    if (!ent_params.owner_ent_vid.has_value()) {
        ent_params.owner_ent_vid = ent.vid;
    }
    return PlayAttachedSoundEmitter(
        state,
        ent.vid,
        attached_offset,
        audio_asset_id,
        ent_params
    );
}

std::optional<VID> PlayEntCenterSoundEmitter(
    State& state,
    const Ent& ent,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params,
    const Vec2& world_offset
) {
    AudioEmitterPlayParams ent_params = params;
    if (!ent_params.owner_ent_vid.has_value()) {
        ent_params.owner_ent_vid = ent.vid;
    }
    return PlayWorldSoundEmitter(
        state,
        ent.GetCenter() + world_offset,
        audio_asset_id,
        ent_params
    );
}

/// Finds or creates one looping attached emitter for the given owner and sound.
/// Re-calling this each step is the intended pattern for persistent loops; it refreshes
/// volume, follow target, offset, and loss policy instead of spawning duplicates.
std::optional<VID> EnsureAttachedLoopingSoundEmitter(
    State& state,
    VID owner_ent_vid,
    VID attached_ent_vid,
    const Vec2& attached_offset,
    AudioAssetId audio_asset_id,
    float volume_scale,
    AudioEmitterTargetLossPolicy target_loss_policy
) {
    const std::optional<VID> existing = FindOwnedSoundEmitter(
        state,
        owner_ent_vid,
        audio_asset_id,
        AudioEmitterPlaybackMode::Looping
    );
    if (existing.has_value()) {
        AudioEmitter* const emitter = state.audio_emitters.GetEmitterMut(*existing);
        if (emitter != nullptr) {
            emitter->volume_scale = volume_scale;
            emitter->source_mode = AudioEmitterSourceMode::AttachedEnt;
            emitter->attached_ent_vid = attached_ent_vid;
            emitter->attached_offset = attached_offset;
            emitter->target_loss_policy = target_loss_policy;
        }
        return existing;
    }

    AudioEmitterPlayParams params;
    params.volume_scale = volume_scale;
    params.playback_mode = AudioEmitterPlaybackMode::Looping;
    params.target_loss_policy = target_loss_policy;
    params.owner_ent_vid = owner_ent_vid;
    return PlayAttachedSoundEmitter(
        state,
        attached_ent_vid,
        attached_offset,
        audio_asset_id,
        params
    );
}

/// Stops one emitter immediately and frees its slot.
bool StopSoundEmitter(State& state, Audio& audio, VID emitter_vid) {
    AudioEmitter* const emitter = state.audio_emitters.GetEmitterMut(emitter_vid);
    if (emitter == nullptr) {
        return false;
    }

    if (IsValidAudioInstanceVID(emitter->sound_instance_vid)) {
        (void)audio.StopAudioInstance(emitter->sound_instance_vid);
    }
    state.audio_emitters.SetInactiveVid(emitter->vid);
    return true;
}

/// Stops the owned emitter matching this owner/sound/playback tuple, if one exists.
bool StopOwnedSoundEmitter(
    State& state,
    Audio& audio,
    VID owner_ent_vid,
    AudioAssetId audio_asset_id,
    AudioEmitterPlaybackMode playback_mode
) {
    const std::optional<VID> emitter_vid =
        FindOwnedSoundEmitter(state, owner_ent_vid, audio_asset_id, playback_mode);
    if (!emitter_vid.has_value()) {
        return false;
    }
    return StopSoundEmitter(state, audio, *emitter_vid);
}

/// Hard-stops every active emitter. Use on scene or state teardown.
void StopAllSoundEmitters(State& state, Audio& audio) {
    std::vector<VID> emitter_vids;
    emitter_vids.reserve(state.audio_emitters.emitters.size());
    for (const AudioEmitter& emitter : state.audio_emitters.emitters) {
        if (emitter.active) {
            emitter_vids.push_back(emitter.vid);
        }
    }
    for (const VID& emitter_vid : emitter_vids) {
        (void)StopSoundEmitter(state, audio, emitter_vid);
    }
}

/// Advances the emitter system for one frame: resolve source positions, start pending
/// instances, update positional/acoustic mix, and reap emitters whose playback ended.
void UpdateAudioEmitters(State& state, Audio& audio, const Graphics& graphics) {
    audio.SetListenerWorldPos(GetAudioListenerWorldPos(state));

    for (AudioEmitter& emitter : state.audio_emitters.emitters) {
        if (!emitter.active) {
            continue;
        }

        if (!ResolveEmitterWorldPos(state, graphics, emitter)) {
            if (IsValidAudioInstanceVID(emitter.sound_instance_vid)) {
                (void)audio.StopAudioInstance(emitter.sound_instance_vid);
            }
            DeactivateEmitter(state.audio_emitters, emitter);
            continue;
        }

        if (!emitter.started) {
            (void)StartEmitterInstance(emitter, state, audio, graphics);
            continue;
        }

        const bool playing =
            IsValidAudioInstanceVID(emitter.sound_instance_vid) &&
            audio.IsAudioInstancePlaying(emitter.sound_instance_vid);
        if (!playing) {
            if (emitter.playback_mode == AudioEmitterPlaybackMode::Looping) {
                ClearEmitterRuntime(emitter);
                (void)StartEmitterInstance(emitter, state, audio, graphics);
                continue;
            }

            DeactivateEmitter(state.audio_emitters, emitter);
            continue;
        }

        const AudioPlaybackParams params = BuildEmitterPlaybackParams(state, graphics, emitter);
        if (audio.UpdateAudioInstance(emitter.sound_instance_vid, params)) {
            continue;
        }

        if (emitter.playback_mode == AudioEmitterPlaybackMode::Looping) {
            ClearEmitterRuntime(emitter);
            (void)StartEmitterInstance(emitter, state, audio, graphics);
            continue;
        }

        DeactivateEmitter(state.audio_emitters, emitter);
    }
}

const char* AudioEmitterPlaybackModeToString(AudioEmitterPlaybackMode mode) {
    switch (mode) {
    case AudioEmitterPlaybackMode::OneShot:
        return "oneshot";
    case AudioEmitterPlaybackMode::Looping:
        return "loop";
    }
    return "unknown";
}

const char* AudioEmitterSourceModeToString(AudioEmitterSourceMode mode) {
    switch (mode) {
    case AudioEmitterSourceMode::FixedWorldPos:
        return "world";
    case AudioEmitterSourceMode::AttachedEnt:
        return "attached";
    }
    return "unknown";
}

const char* AudioEmitterTargetLossPolicyToString(
    AudioEmitterTargetLossPolicy behavior
) {
    switch (behavior) {
    case AudioEmitterTargetLossPolicy::StopImmediately:
        return "stop";
    case AudioEmitterTargetLossPolicy::FinishCurrentPlay:
        return "finish";
    case AudioEmitterTargetLossPolicy::KeepPlayingDetached:
        return "detach";
    }
    return "unknown";
}

} // namespace splonks
