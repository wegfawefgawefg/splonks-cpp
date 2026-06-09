#pragma once

#include "draw_layer.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "particles/lighting_mode.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace splonks {

using ScriptedParticleSpecId = std::uint32_t;
using RibbonParticleSpecId = std::uint32_t;
using SegmentedSpriteParticleSpecId = std::uint32_t;

constexpr ScriptedParticleSpecId kInvalidScriptedParticleSpecId = 0;
constexpr RibbonParticleSpecId kInvalidRibbonParticleSpecId = 0;
constexpr SegmentedSpriteParticleSpecId kInvalidSegmentedSpriteParticleSpecId = 0;

constexpr std::uint32_t kParticleSpecFnvOffsetBasis32 = 2166136261U;
constexpr std::uint32_t kParticleSpecFnvPrime32 = 16777619U;

constexpr std::uint32_t HashParticleSpecIdConstexpr(std::string_view text) {
    std::uint32_t hash = kParticleSpecFnvOffsetBasis32;
    for (char character : text) {
        hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(character));
        hash *= kParticleSpecFnvPrime32;
    }
    return hash;
}

inline std::uint32_t HashParticleSpecId(const std::string& text) {
    return HashParticleSpecIdConstexpr(text);
}

namespace scripted_particle_spec_ids {
constexpr ScriptedParticleSpecId MeatheadPopup =
    HashParticleSpecIdConstexpr("meathead_popup");
constexpr ScriptedParticleSpecId MeatTileTopper =
    HashParticleSpecIdConstexpr("meat_tile_topper");
} // namespace scripted_particle_spec_ids

namespace ribbon_particle_spec_ids {
constexpr RibbonParticleSpecId Default =
    HashParticleSpecIdConstexpr("default_ribbon");
constexpr RibbonParticleSpecId BaseballBatTrail =
    HashParticleSpecIdConstexpr("baseball_bat_trail");
} // namespace ribbon_particle_spec_ids

namespace segmented_sprite_particle_spec_ids {
constexpr SegmentedSpriteParticleSpecId Default =
    HashParticleSpecIdConstexpr("default_segmented_sprite");
} // namespace segmented_sprite_particle_spec_ids

struct ScriptedParticleSequenceStep {
    AFrameId anim_id = kInvalidAFrameId;
    AnimPlaybackMode playback_mode = AnimPlaybackMode::Forward;
    std::uint32_t play_count = 1;
};

struct ScriptedParticleSpec {
    ScriptedParticleSpecId id = kInvalidScriptedParticleSpecId;
    std::string_view name;
    DrawLayer draw_layer = DrawLayer::Middle;
    ParticleLightingMode lighting_mode = ParticleLightingMode::SceneLit;
    FVec2 size = FVec2::New(0.0F, 0.0F);
    std::uint32_t hold_frames_after_sequence = 0;
    std::span<const ScriptedParticleSequenceStep> sequence;
};

struct RibbonParticleSpec {
    RibbonParticleSpecId id = kInvalidRibbonParticleSpecId;
    std::string_view name;
    DrawLayer draw_layer = DrawLayer::Middle;
    ParticleLightingMode lighting_mode = ParticleLightingMode::SceneLit;
    AFrameId anim_id = kInvalidAFrameId;
    float width = 1.0F;
};

struct SegmentedSpriteParticleSpec {
    SegmentedSpriteParticleSpecId id = kInvalidSegmentedSpriteParticleSpecId;
    std::string_view name;
    DrawLayer draw_layer = DrawLayer::Middle;
    ParticleLightingMode lighting_mode = ParticleLightingMode::SceneLit;
    AFrameId anim_id = kInvalidAFrameId;
    FVec2 segment_size = FVec2::New(0.0F, 0.0F);
    float spacing = 0.0F;
};

const ScriptedParticleSpec* GetScriptedParticleSpec(ScriptedParticleSpecId id);
const RibbonParticleSpec* GetRibbonParticleSpec(RibbonParticleSpecId id);
const SegmentedSpriteParticleSpec* GetSegmentedSpriteParticleSpec(
    SegmentedSpriteParticleSpecId id
);

} // namespace splonks
