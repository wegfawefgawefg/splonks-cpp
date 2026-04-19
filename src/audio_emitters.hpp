#pragma once

#include "audio.hpp"
#include "math_types.hpp"
#include "vid.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace splonks {

struct Audio;
struct Entity;
struct Graphics;
struct State;

constexpr std::size_t kInvalidAudioEmitterId = std::numeric_limits<std::size_t>::max();
inline constexpr VID kInvalidAudioEmitterVID{kInvalidAudioEmitterId, 0};

inline bool IsValidAudioEmitterVID(const VID& vid) {
    return vid.id != kInvalidAudioEmitterId;
}

/// Whether an emitter should play once or keep restarting the same sound until stopped.
enum class AudioEmitterPlaybackMode : std::uint8_t {
    OneShot,
    Looping,
};

/// Where the emitter resolves its world position from.
enum class AudioEmitterSourceMode : std::uint8_t {
    FixedWorldPos,
    AttachedEntity,
};

/// What to do if the attached/owning entity disappears while the emitter is active.
enum class AudioEmitterTargetLossPolicy : std::uint8_t {
    StopImmediately,
    FinishCurrentPlay,
    KeepPlayingDetached,
};


/// Shared play-time knobs for emitter creation helpers.
/// owner_entity_vid is optional ownership metadata used for lookup, dedupe, and cleanup.
/// It does not have to match the attached entity.
struct AudioEmitterPlayParams {
    float volume_scale = 1.0F;
    AudioEmitterPlaybackMode playback_mode = AudioEmitterPlaybackMode::OneShot;
    AudioEmitterTargetLossPolicy target_loss_policy =
        AudioEmitterTargetLossPolicy::FinishCurrentPlay;
    std::optional<VID> owner_entity_vid = std::nullopt;
};

struct AudioEmitter {
    bool active = false;
    VID vid = kInvalidAudioEmitterVID;
    bool started = false;
    VID sound_instance_vid = kInvalidAudioInstanceVID;
    AudioAssetId audio_asset_id = audio_asset_ids::Jump;
    float volume_scale = 1.0F;
    AudioEmitterPlaybackMode playback_mode = AudioEmitterPlaybackMode::OneShot;
    AudioEmitterSourceMode source_mode = AudioEmitterSourceMode::FixedWorldPos;
    AudioEmitterTargetLossPolicy target_loss_policy =
        AudioEmitterTargetLossPolicy::FinishCurrentPlay;
    std::optional<VID> owner_entity_vid = std::nullopt;
    std::optional<VID> attached_entity_vid = std::nullopt;
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
    Vec2 attached_offset = Vec2::New(0.0F, 0.0F);
};

struct AudioEmitterManager {
    std::vector<AudioEmitter> emitters;
    std::vector<std::size_t> available_ids;

    static constexpr std::size_t kMaxNumAudioEmitters = 256;

    static AudioEmitterManager New();
    std::optional<VID> NewEmitter();
    void SetInactiveVid(const VID& vid);
    AudioEmitter* GetEmitterMut(const VID& vid);
    const AudioEmitter* GetEmitter(const VID& vid) const;
    void ClearAll();
};

const AudioEmitter* GetSoundEmitter(const State& state, VID emitter_vid);
AudioEmitter* GetSoundEmitterMut(State& state, VID emitter_vid);
Vec2 GetAudioListenerWorldPos(const State& state);
void SetAudioListenerWorldPos(State& state, const Vec2& world_pos);
std::optional<VID> FindOwnedSoundEmitter(
    const State& state,
    VID owner_entity_vid,
    AudioAssetId audio_asset_id,
    AudioEmitterPlaybackMode playback_mode
);
/// Spawns an emitter at a fixed world position.
/// Use this for one-shots or detached ambience that should stay at a point in the world
/// while still updating positional mix as the listener/camera moves.
std::optional<VID> PlayWorldSoundEmitter(
    State& state,
    const Vec2& world_pos,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params = {}
);
/// Spawns an emitter that follows an entity every frame at entity center + attached_offset.
/// Use this when the sound source should move with an entity without the caller manually
/// updating emitter.world_pos each step.
std::optional<VID> PlayAttachedSoundEmitter(
    State& state,
    VID attached_entity_vid,
    const Vec2& attached_offset,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params = {}
);
/// Spawns a one-shot or loop emitter attached to an entity and default-owned by it.
/// Use this for sounds that should follow a living entity while they play.
std::optional<VID> PlayEntitySoundEmitter(
    State& state,
    const Entity& entity,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params = {},
    const Vec2& attached_offset = Vec2::New(0.0F, 0.0F)
);
/// Spawns a fixed world emitter at an entity's current center plus world_offset.
/// Use this for transient impacts or pickups where the source may disappear immediately.
std::optional<VID> PlayEntityCenterSoundEmitter(
    State& state,
    const Entity& entity,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params = {},
    const Vec2& world_offset = Vec2::New(0.0F, 0.0F)
);
/// Finds or creates one looping attached emitter for (owner_entity_vid, audio_asset_id).
/// Intended for per-step maintenance of persistent loops, e.g. a rolling boulder hum.
/// Re-calling it refreshes follow target, offset, volume, and loss policy instead of
/// spawning duplicate loop instances.
std::optional<VID> EnsureAttachedLoopingSoundEmitter(
    State& state,
    VID owner_entity_vid,
    VID attached_entity_vid,
    const Vec2& attached_offset,
    AudioAssetId audio_asset_id,
    float volume_scale = 1.0F,
    AudioEmitterTargetLossPolicy target_loss_policy =
        AudioEmitterTargetLossPolicy::StopImmediately
);
/// Stops a specific emitter immediately and releases its slot.
bool StopSoundEmitter(State& state, Audio& audio, VID emitter_vid);
/// Stops the owned emitter matching this owner/sound/playback tuple, if any.
bool StopOwnedSoundEmitter(
    State& state,
    Audio& audio,
    VID owner_entity_vid,
    AudioAssetId audio_asset_id,
    AudioEmitterPlaybackMode playback_mode
);
/// Hard stop for all active emitters. Useful on scene/state teardown.
void StopAllSoundEmitters(State& state, Audio& audio);
/// Resolves world positions, starts queued emitters, updates positional mix, and reaps
/// finished emitters. Call once per frame while the owning game state is active.
void UpdateAudioEmitters(State& state, Audio& audio, const Graphics& graphics);
const char* AudioEmitterPlaybackModeToString(AudioEmitterPlaybackMode mode);
const char* AudioEmitterSourceModeToString(AudioEmitterSourceMode mode);
const char* AudioEmitterTargetLossPolicyToString(
    AudioEmitterTargetLossPolicy behavior
);

} // namespace splonks
