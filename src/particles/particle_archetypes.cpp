#include "particles/particle_archetypes.hpp"

namespace splonks {

namespace {

constexpr ScriptedParticleSequenceStep kMeatheadPopupSequence[] = {
    {frame_data_ids::MeatheadRise, AnimationPlaybackMode::Forward, 1},
    {frame_data_ids::Meathead, AnimationPlaybackMode::Forward, 2},
    {frame_data_ids::MeatheadRise, AnimationPlaybackMode::Reverse, 1},
};

constexpr ScriptedParticleSequenceStep kMeatTileTopperSequence[] = {
    {frame_data_ids::MeatTileTopper, AnimationPlaybackMode::Forward, 1},
};

const ScriptedParticleArchetype kScriptedParticleArchetypes[] = {
    {
        .id = scripted_particle_archetype_ids::MeatheadPopup,
        .name = "meathead_popup",
        .draw_layer = DrawLayer::Background,
        .size = Vec2::New(9.0F, 9.0F),
        .sequence = std::span<const ScriptedParticleSequenceStep>(kMeatheadPopupSequence),
    },
    {
        .id = scripted_particle_archetype_ids::MeatTileTopper,
        .name = "meat_tile_topper",
        .draw_layer = DrawLayer::Background,
        .size = Vec2::New(16.0F, 7.0F),
        .hold_frames_after_sequence = 20U * 60U,
        .sequence = std::span<const ScriptedParticleSequenceStep>(kMeatTileTopperSequence),
    },
};

const RibbonParticleArchetype kRibbonParticleArchetypes[] = {
    {
        .id = ribbon_particle_archetype_ids::Default,
        .name = "default_ribbon",
        .draw_layer = DrawLayer::Foreground,
        .animation_id = frame_data_ids::LittleSmoke,
        .width = 4.0F,
    },
    {
        .id = ribbon_particle_archetype_ids::BaseballBatTrail,
        .name = "baseball_bat_trail",
        .draw_layer = DrawLayer::Foreground,
        .animation_id = frame_data_ids::BaseballBatTrail,
        .width = 2.5F,
    },
};

const SegmentedSpriteParticleArchetype kSegmentedSpriteParticleArchetypes[] = {
    {
        .id = segmented_sprite_particle_archetype_ids::Default,
        .name = "default_segmented_sprite",
        .draw_layer = DrawLayer::Foreground,
        .animation_id = frame_data_ids::RopeBall,
        .segment_size = Vec2::New(4.0F, 4.0F),
        .spacing = 4.0F,
    },
};

template <typename T, typename Id>
const T* FindArchetypeById(const T* archetypes, std::size_t count, Id id) {
    for (std::size_t i = 0; i < count; ++i) {
        if (archetypes[i].id == id) {
            return &archetypes[i];
        }
    }
    return nullptr;
}

} // namespace

const ScriptedParticleArchetype* GetScriptedParticleArchetype(ScriptedParticleArchetypeId id) {
    return FindArchetypeById(kScriptedParticleArchetypes, std::size(kScriptedParticleArchetypes), id);
}

const RibbonParticleArchetype* GetRibbonParticleArchetype(RibbonParticleArchetypeId id) {
    return FindArchetypeById(kRibbonParticleArchetypes, std::size(kRibbonParticleArchetypes), id);
}

const SegmentedSpriteParticleArchetype* GetSegmentedSpriteParticleArchetype(
    SegmentedSpriteParticleArchetypeId id
) {
    return FindArchetypeById(
        kSegmentedSpriteParticleArchetypes,
        std::size(kSegmentedSpriteParticleArchetypes),
        id
    );
}

} // namespace splonks
