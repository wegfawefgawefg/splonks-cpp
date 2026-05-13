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
    AddTransientLight,
};

enum class ScriptedPresentationEffectId : std::uint16_t {
    None,
    TeleportSplit,
    TeleportMerge,
    JetpackSmoke,
    ExplosionBurst,
    PistolMuzzleSmoke,
    PistolImpact,
    TreasurePickupSparkles,
    BaseballBatTrail,
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
    std::uint32_t effect_count = 0;
    float effect_scale = 1.0F;
    float entity_shake_amount = 0.0F;
    float foreground_shake_amount = 0.0F;
    float background_shake_amount = 0.0F;
    float area_entity_shake_amount = 0.0F;
    float shake_radius_tiles = 0.0F;
    float light_strength = 0.0F;
    Color3 light_color = Color3::White();
    int light_radius = 0;
    std::uint32_t light_lifetime_frames = 0;
};

void PlayPresentationCommand(State& state, Graphics& graphics, const PresentationCommand& command);

} // namespace splonks
