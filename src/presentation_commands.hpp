#pragma once

#include "audio_asset_id.hpp"
#include "math_types.hpp"
#include "vid.hpp"

#include <cstdint>
#include <optional>

namespace splonks {

struct Graphics;
struct State;

enum class PresentationCommandKind : std::uint16_t {
    None,
    PlaySoundAt,
    ShakeEntity,
    ShakeArea,
    SpawnScriptedEffect,
};

enum class ScriptedPresentationEffectId : std::uint16_t {
    None,
    TeleportSplit,
    TeleportMerge,
    JetpackSmoke,
};

struct PresentationCommand {
    PresentationCommandKind kind = PresentationCommandKind::None;
    ScriptedPresentationEffectId effect_id = ScriptedPresentationEffectId::None;
    AudioAssetId audio_asset_id = kInvalidAudioAssetId;
    std::optional<VID> source_vid = std::nullopt;
    std::optional<VID> target_vid = std::nullopt;
    Vec2 source_pos = Vec2::New(0.0F, 0.0F);
    Vec2 target_pos = Vec2::New(0.0F, 0.0F);
    IVec2 direction = IVec2::New(1, 0);
    float param_a = 0.0F;
    float param_b = 0.0F;
    float param_c = 0.0F;
    float param_d = 0.0F;
};

void PlayPresentationCommand(State& state, Graphics& graphics, const PresentationCommand& command);

} // namespace splonks
