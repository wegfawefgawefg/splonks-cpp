#include "particles/particle_specs.hpp"

namespace splonks {

namespace {

constexpr ScriptedParticleSequenceStep kMeatheadPopupSequence[] = {
    {aframe_ids::MeatheadRise, AnimPlaybackMode::Forward, 1},
    {aframe_ids::Meathead, AnimPlaybackMode::Forward, 2},
    {aframe_ids::MeatheadRise, AnimPlaybackMode::Reverse, 1},
};

constexpr ScriptedParticleSequenceStep kMeatTileTopperSequence[] = {
    {aframe_ids::MeatTileTopper, AnimPlaybackMode::Forward, 1},
};

const ScriptedParticleSpec kScriptedParticleSpecs[] = {
    {
        .id = scripted_particle_spec_ids::MeatheadPopup,
        .name = "meathead_popup",
        .draw_layer = DrawLayer::Background,
        .size = FVec2::New(9.0F, 9.0F),
        .sequence = std::span<const ScriptedParticleSequenceStep>(kMeatheadPopupSequence),
    },
    {
        .id = scripted_particle_spec_ids::MeatTileTopper,
        .name = "meat_tile_topper",
        .draw_layer = DrawLayer::Background,
        .size = FVec2::New(16.0F, 7.0F),
        .hold_frames_after_sequence = 20U * 60U,
        .sequence = std::span<const ScriptedParticleSequenceStep>(kMeatTileTopperSequence),
    },
};

const RibbonParticleSpec kRibbonParticleSpecs[] = {
    {
        .id = ribbon_particle_spec_ids::Default,
        .name = "default_ribbon",
        .draw_layer = DrawLayer::Foreground,
        .anim_id = aframe_ids::LittleSmoke,
        .width = 4.0F,
    },
    {
        .id = ribbon_particle_spec_ids::BaseballBatTrail,
        .name = "baseball_bat_trail",
        .draw_layer = DrawLayer::Foreground,
        .lighting_mode = ParticleLightingMode::Emissive,
        .anim_id = aframe_ids::BaseballBatTrail,
        .width = 2.5F,
    },
};

const SegmentedSpriteParticleSpec kSegmentedSpriteParticleSpecs[] = {
    {
        .id = segmented_sprite_particle_spec_ids::Default,
        .name = "default_segmented_sprite",
        .draw_layer = DrawLayer::Foreground,
        .anim_id = aframe_ids::RopeBall,
        .segment_size = FVec2::New(4.0F, 4.0F),
        .spacing = 4.0F,
    },
};

template <typename T, typename Id>
const T* FindSpecById(const T* specs, std::size_t count, Id id) {
    for (std::size_t i = 0; i < count; ++i) {
        if (specs[i].id == id) {
            return &specs[i];
        }
    }
    return nullptr;
}

} // namespace

const ScriptedParticleSpec* GetScriptedParticleSpec(ScriptedParticleSpecId id) {
    return FindSpecById(kScriptedParticleSpecs, std::size(kScriptedParticleSpecs), id);
}

const RibbonParticleSpec* GetRibbonParticleSpec(RibbonParticleSpecId id) {
    return FindSpecById(kRibbonParticleSpecs, std::size(kRibbonParticleSpecs), id);
}

const SegmentedSpriteParticleSpec* GetSegmentedSpriteParticleSpec(
    SegmentedSpriteParticleSpecId id
) {
    return FindSpecById(
        kSegmentedSpriteParticleSpecs,
        std::size(kSegmentedSpriteParticleSpecs),
        id
    );
}

} // namespace splonks
