#pragma once

#include "audio.hpp"
#include "math_types.hpp"
#include "vid.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace splonks {

struct Audio;
struct Ent;
struct Graphics;
struct State;

constexpr std::uint32_t kInvalidAudioEmitterId = std::numeric_limits<std::uint32_t>::max();
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
    AttachedEnt,
};

/// What to do if the attached/owning ent disappears while the emitter is active.
enum class AudioEmitterTargetLossPolicy : std::uint8_t {
    StopImmediately,
    FinishCurrentPlay,
    KeepPlayingDetached,
};


/// Shared play-time knobs for emitter creation helpers.
/// owner_ent_vid is optional ownership metadata used for lookup, dedupe, and cleanup.
/// It does not have to match the attached ent.
struct AudioEmitterPlayParams {
    float volume_scale = 1.0F;
    AudioEmitterPlaybackMode playback_mode = AudioEmitterPlaybackMode::OneShot;
    AudioEmitterTargetLossPolicy target_loss_policy =
        AudioEmitterTargetLossPolicy::FinishCurrentPlay;
    std::optional<VID> owner_ent_vid = std::nullopt;
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
    std::optional<VID> owner_ent_vid = std::nullopt;
    std::optional<VID> attached_ent_vid = std::nullopt;
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
    VID owner_ent_vid,
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
/// Spawns an emitter that follows an ent every frame at ent center + attached_offset.
/// Use this when the sound source should move with an ent without the caller manually
/// updating emitter.world_pos each step.
std::optional<VID> PlayAttachedSoundEmitter(
    State& state,
    VID attached_ent_vid,
    const Vec2& attached_offset,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params = {}
);
/// Spawns a one-shot or loop emitter attached to an ent and default-owned by it.
/// Use this for sounds that should follow a living ent while they play.
std::optional<VID> PlayEntSoundEmitter(
    State& state,
    const Ent& ent,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params = {},
    const Vec2& attached_offset = Vec2::New(0.0F, 0.0F)
);
/// Spawns a fixed world emitter at an ent's current center plus world_offset.
/// Use this for transient impacts or pickups where the source may disappear immediately.
std::optional<VID> PlayEntCenterSoundEmitter(
    State& state,
    const Ent& ent,
    AudioAssetId audio_asset_id,
    const AudioEmitterPlayParams& params = {},
    const Vec2& world_offset = Vec2::New(0.0F, 0.0F)
);
/// Finds or creates one looping attached emitter for (owner_ent_vid, audio_asset_id).
/// Intended for per-step maintenance of persistent loops, e.g. a rolling boulder hum.
/// Re-calling it refreshes follow target, offset, volume, and loss policy instead of
/// spawning duplicate loop instances.
std::optional<VID> EnsureAttachedLoopingSoundEmitter(
    State& state,
    VID owner_ent_vid,
    VID attached_ent_vid,
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
    VID owner_ent_vid,
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
