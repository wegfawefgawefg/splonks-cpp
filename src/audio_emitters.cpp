#include "audio_emitters.hpp"

#include "audio_acoustics.hpp"
#include "entity.hpp"
#include "entity/manager.hpp"
#include "graphics.hpp"
#include "state.hpp"

#include <cstdio>

namespace splonks {

namespace {

void ClearEmitterRuntime(AudioEmitter& emitter) {
    emitter.started = false;
    emitter.sound_instance_vid = kInvalidSoundEffectInstanceVID;
}

void DeactivateEmitter(AudioEmitterManager& manager, AudioEmitter& emitter) {
    if (!emitter.active) {
        return;
    }

    emitter.active = false;
    ClearEmitterRuntime(emitter);
    emitter.owner_entity_vid.reset();
    emitter.attached_entity_vid.reset();
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
        emitter.attached_entity_vid.reset();
        emitter.attached_offset = Vec2::New(0.0F, 0.0F);
        if (owner_missing) {
            emitter.owner_entity_vid.reset();
        }
        return true;
    case AudioEmitterTargetLossPolicy::KeepPlayingDetached:
        emitter.source_mode = AudioEmitterSourceMode::FixedWorldPos;
        emitter.attached_entity_vid.reset();
        emitter.attached_offset = Vec2::New(0.0F, 0.0F);
        if (owner_missing) {
            emitter.owner_entity_vid.reset();
        }
        return true;
    }
    return false;
}

bool ResolveEmitterWorldPos(State& state, AudioEmitter& emitter) {
    if (emitter.owner_entity_vid.has_value() &&
        state.entity_manager.GetEntity(*emitter.owner_entity_vid) == nullptr) {
        return HandleMissingTarget(emitter, true);
    }

    if (emitter.source_mode != AudioEmitterSourceMode::AttachedEntity ||
        !emitter.attached_entity_vid.has_value()) {
        return true;
    }

    const Entity* const attached = state.entity_manager.GetEntity(*emitter.attached_entity_vid);
    if (attached != nullptr) {
        emitter.world_pos = attached->GetCenter() + emitter.attached_offset;
        return true;
    }

    return HandleMissingTarget(emitter, false);
}

SoundEffectPlaybackParams BuildEmitterPlaybackParams(
    State& state,
    const Graphics& graphics,
    const AudioEmitter& emitter
) {
    SoundEffectPlaybackParams params;
    params.volume_scale = emitter.volume_scale;
    params.positional = true;
    params.loops = emitter.playback_mode == AudioEmitterPlaybackMode::Looping ? -1 : 0;

    const PositionalAudioAcoustics acoustics = ComputePositionalAudioAcoustics(
        state,
        graphics.camera.target,
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
    const SoundEffectPlaybackParams params = BuildEmitterPlaybackParams(state, graphics, emitter);
    emitter.sound_instance_vid = audio.PlaySoundEffectInstance(emitter.sound_effect, params);
    emitter.started = IsValidSoundEffectInstanceVID(emitter.sound_instance_vid);
    return emitter.started;
}

} // namespace

AudioEmitterManager AudioEmitterManager::New() {
    AudioEmitterManager manager;
    manager.emitters.reserve(kMaxNumAudioEmitters);
    manager.available_ids.reserve(kMaxNumAudioEmitters);
    for (std::size_t i = 0; i < kMaxNumAudioEmitters; ++i) {
        AudioEmitter emitter;
        emitter.vid = VID{i, 0};
        manager.emitters.push_back(emitter);
        manager.available_ids.insert(manager.available_ids.begin(), i);
    }
    return manager;
}

std::optional<VID> AudioEmitterManager::NewEmitter() {
    if (available_ids.empty()) {
        std::printf("Audio emitter budget bounce!\n");
        return std::nullopt;
    }

    const std::size_t id = available_ids.back();
    available_ids.pop_back();
    AudioEmitter& emitter = emitters[id];
    emitter.active = true;
    emitter.vid.version += 1;
    emitter.started = false;
    emitter.sound_instance_vid = kInvalidSoundEffectInstanceVID;
    emitter.owner_entity_vid.reset();
    emitter.attached_entity_vid.reset();
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
    if (!IsValidAudioEmitterVID(vid) || vid.id >= emitters.size()) {
        return nullptr;
    }

    AudioEmitter& emitter = emitters[vid.id];
    if (!emitter.active || emitter.vid.version != vid.version) {
        return nullptr;
    }
    return &emitter;
}

const AudioEmitter* AudioEmitterManager::GetEmitter(const VID& vid) const {
    if (!IsValidAudioEmitterVID(vid) || vid.id >= emitters.size()) {
        return nullptr;
    }

    const AudioEmitter& emitter = emitters[vid.id];
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
        emitters[i].owner_entity_vid.reset();
        emitters[i].attached_entity_vid.reset();
        available_ids.insert(available_ids.begin(), i);
    }
}

const AudioEmitter* GetSoundEmitter(const State& state, VID emitter_vid) {
    return state.audio_emitters.GetEmitter(emitter_vid);
}

AudioEmitter* GetSoundEmitterMut(State& state, VID emitter_vid) {
    return state.audio_emitters.GetEmitterMut(emitter_vid);
}

std::optional<VID> FindOwnedSoundEmitter(
    const State& state,
    VID owner_entity_vid,
    SoundEffect sound_effect,
    AudioEmitterPlaybackMode playback_mode
) {
    for (const AudioEmitter& emitter : state.audio_emitters.emitters) {
        if (!emitter.active || !emitter.owner_entity_vid.has_value()) {
            continue;
        }
        if (*emitter.owner_entity_vid == owner_entity_vid &&
            emitter.sound_effect == sound_effect &&
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
    SoundEffect sound_effect,
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

    emitter->sound_effect = sound_effect;
    emitter->volume_scale = params.volume_scale;
    emitter->playback_mode = params.playback_mode;
    emitter->target_loss_policy = params.target_loss_policy;
    emitter->owner_entity_vid = params.owner_entity_vid;
    emitter->source_mode = AudioEmitterSourceMode::FixedWorldPos;
    emitter->world_pos = world_pos;
    return emitter->vid;
}

/// Creates an emitter that follows an entity each frame at entity center + attached_offset.
/// Use this when the sound source should move with an entity without manual position updates.
std::optional<VID> PlayAttachedSoundEmitter(
    State& state,
    VID attached_entity_vid,
    const Vec2& attached_offset,
    SoundEffect sound_effect,
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

    emitter->sound_effect = sound_effect;
    emitter->volume_scale = params.volume_scale;
    emitter->playback_mode = params.playback_mode;
    emitter->target_loss_policy = params.target_loss_policy;
    emitter->owner_entity_vid =
        params.owner_entity_vid.has_value() ? params.owner_entity_vid : std::optional<VID>(attached_entity_vid);
    emitter->source_mode = AudioEmitterSourceMode::AttachedEntity;
    emitter->attached_entity_vid = attached_entity_vid;
    emitter->attached_offset = attached_offset;
    return emitter->vid;
}

/// Finds or creates one looping attached emitter for the given owner and sound.
/// Re-calling this each step is the intended pattern for persistent loops; it refreshes
/// volume, follow target, offset, and loss policy instead of spawning duplicates.
std::optional<VID> EnsureAttachedLoopingSoundEmitter(
    State& state,
    VID owner_entity_vid,
    VID attached_entity_vid,
    const Vec2& attached_offset,
    SoundEffect sound_effect,
    float volume_scale,
    AudioEmitterTargetLossPolicy target_loss_policy
) {
    const std::optional<VID> existing = FindOwnedSoundEmitter(
        state,
        owner_entity_vid,
        sound_effect,
        AudioEmitterPlaybackMode::Looping
    );
    if (existing.has_value()) {
        AudioEmitter* const emitter = state.audio_emitters.GetEmitterMut(*existing);
        if (emitter != nullptr) {
            emitter->volume_scale = volume_scale;
            emitter->source_mode = AudioEmitterSourceMode::AttachedEntity;
            emitter->attached_entity_vid = attached_entity_vid;
            emitter->attached_offset = attached_offset;
            emitter->target_loss_policy = target_loss_policy;
        }
        return existing;
    }

    AudioEmitterPlayParams params;
    params.volume_scale = volume_scale;
    params.playback_mode = AudioEmitterPlaybackMode::Looping;
    params.target_loss_policy = target_loss_policy;
    params.owner_entity_vid = owner_entity_vid;
    return PlayAttachedSoundEmitter(
        state,
        attached_entity_vid,
        attached_offset,
        sound_effect,
        params
    );
}

/// Stops one emitter immediately and frees its slot.
bool StopSoundEmitter(State& state, Audio& audio, VID emitter_vid) {
    AudioEmitter* const emitter = state.audio_emitters.GetEmitterMut(emitter_vid);
    if (emitter == nullptr) {
        return false;
    }

    if (IsValidSoundEffectInstanceVID(emitter->sound_instance_vid)) {
        (void)audio.StopSoundEffectInstance(emitter->sound_instance_vid);
    }
    state.audio_emitters.SetInactiveVid(emitter->vid);
    return true;
}

/// Stops the owned emitter matching this owner/sound/playback tuple, if one exists.
bool StopOwnedSoundEmitter(
    State& state,
    Audio& audio,
    VID owner_entity_vid,
    SoundEffect sound_effect,
    AudioEmitterPlaybackMode playback_mode
) {
    const std::optional<VID> emitter_vid =
        FindOwnedSoundEmitter(state, owner_entity_vid, sound_effect, playback_mode);
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
    audio.SetListenerWorldPos(graphics.camera.target);

    for (AudioEmitter& emitter : state.audio_emitters.emitters) {
        if (!emitter.active) {
            continue;
        }

        if (!ResolveEmitterWorldPos(state, emitter)) {
            if (IsValidSoundEffectInstanceVID(emitter.sound_instance_vid)) {
                (void)audio.StopSoundEffectInstance(emitter.sound_instance_vid);
            }
            DeactivateEmitter(state.audio_emitters, emitter);
            continue;
        }

        if (!emitter.started) {
            (void)StartEmitterInstance(emitter, state, audio, graphics);
            continue;
        }

        const bool playing =
            IsValidSoundEffectInstanceVID(emitter.sound_instance_vid) &&
            audio.IsSoundEffectInstancePlaying(emitter.sound_instance_vid);
        if (!playing) {
            if (emitter.playback_mode == AudioEmitterPlaybackMode::Looping) {
                ClearEmitterRuntime(emitter);
                (void)StartEmitterInstance(emitter, state, audio, graphics);
                continue;
            }

            DeactivateEmitter(state.audio_emitters, emitter);
            continue;
        }

        const SoundEffectPlaybackParams params = BuildEmitterPlaybackParams(state, graphics, emitter);
        if (audio.UpdateSoundEffectInstance(emitter.sound_instance_vid, params)) {
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
    case AudioEmitterSourceMode::AttachedEntity:
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
