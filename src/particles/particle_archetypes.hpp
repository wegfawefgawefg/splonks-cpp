#pragma once

#include "draw_layer.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace splonks {

using ScriptedParticleArchetypeId = std::uint32_t;
using RibbonParticleArchetypeId = std::uint32_t;
using SegmentedSpriteParticleArchetypeId = std::uint32_t;

constexpr ScriptedParticleArchetypeId kInvalidScriptedParticleArchetypeId = 0;
constexpr RibbonParticleArchetypeId kInvalidRibbonParticleArchetypeId = 0;
constexpr SegmentedSpriteParticleArchetypeId kInvalidSegmentedSpriteParticleArchetypeId = 0;

constexpr std::uint32_t kParticleArchetypeFnvOffsetBasis32 = 2166136261U;
constexpr std::uint32_t kParticleArchetypeFnvPrime32 = 16777619U;

constexpr std::uint32_t HashParticleArchetypeIdConstexpr(std::string_view text) {
    std::uint32_t hash = kParticleArchetypeFnvOffsetBasis32;
    for (char character : text) {
        hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(character));
        hash *= kParticleArchetypeFnvPrime32;
    }
    return hash;
}

inline std::uint32_t HashParticleArchetypeId(const std::string& text) {
    return HashParticleArchetypeIdConstexpr(text);
}

namespace scripted_particle_archetype_ids {
constexpr ScriptedParticleArchetypeId MeatheadPopup =
    HashParticleArchetypeIdConstexpr("meathead_popup");
} // namespace scripted_particle_archetype_ids

namespace ribbon_particle_archetype_ids {
constexpr RibbonParticleArchetypeId Default =
    HashParticleArchetypeIdConstexpr("default_ribbon");
constexpr RibbonParticleArchetypeId BaseballBatTrail =
    HashParticleArchetypeIdConstexpr("baseball_bat_trail");
} // namespace ribbon_particle_archetype_ids

namespace segmented_sprite_particle_archetype_ids {
constexpr SegmentedSpriteParticleArchetypeId Default =
    HashParticleArchetypeIdConstexpr("default_segmented_sprite");
} // namespace segmented_sprite_particle_archetype_ids

struct ScriptedParticleSequenceStep {
    FrameDataId animation_id = kInvalidFrameDataId;
    AnimationPlaybackMode playback_mode = AnimationPlaybackMode::Forward;
    std::uint32_t play_count = 1;
};

struct ScriptedParticleArchetype {
    ScriptedParticleArchetypeId id = kInvalidScriptedParticleArchetypeId;
    std::string_view name;
    DrawLayer draw_layer = DrawLayer::Middle;
    Vec2 size = Vec2::New(0.0F, 0.0F);
    std::span<const ScriptedParticleSequenceStep> sequence;
};

struct RibbonParticleArchetype {
    RibbonParticleArchetypeId id = kInvalidRibbonParticleArchetypeId;
    std::string_view name;
    DrawLayer draw_layer = DrawLayer::Middle;
    FrameDataId animation_id = kInvalidFrameDataId;
    float width = 1.0F;
};

struct SegmentedSpriteParticleArchetype {
    SegmentedSpriteParticleArchetypeId id = kInvalidSegmentedSpriteParticleArchetypeId;
    std::string_view name;
    DrawLayer draw_layer = DrawLayer::Middle;
    FrameDataId animation_id = kInvalidFrameDataId;
    Vec2 segment_size = Vec2::New(0.0F, 0.0F);
    float spacing = 0.0F;
};

const ScriptedParticleArchetype* GetScriptedParticleArchetype(ScriptedParticleArchetypeId id);
const RibbonParticleArchetype* GetRibbonParticleArchetype(RibbonParticleArchetypeId id);
const SegmentedSpriteParticleArchetype* GetSegmentedSpriteParticleArchetype(
    SegmentedSpriteParticleArchetypeId id
);

} // namespace splonks
