#include "entities/common/common.hpp"

#include "entity/archetype.hpp"
#include "entities/box.hpp"
#include "entities/pot.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"

#include <cmath>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kTileTouchSoundCooldownFrames = 8;
constexpr float kTileTouchSoundMinImpactVelocity = 1.5F;
constexpr float kTileTouchSoundVolumeScale = 0.10F;

struct TileEntityContactDispatch {
    ContactResolution resolution;
    bool write_cooldown = false;
    std::uint32_t cooldown_duration = 0;
};

std::optional<TileEntityContactDispatch> GetEntityTileContactCooldownSpec(
    const Entity& entity,
    const ContactContext& context
) {
    (void)entity;
    (void)context;
    return std::nullopt;
}

ContactResolution TryDispatchEntityTileContactByEntityType(
    std::size_t entity_idx,
    const ContactContext& context,
    State& state
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return ContactResolution{};
    }

    const Entity& entity = state.entity_manager.entities[entity_idx];
    switch (entity.type_) {
    case EntityType::Pot:
        if (!pot::TryApplyPotImpact(entity_idx, context, state)) {
            return ContactResolution{};
        }
        return ContactResolution{
            .blocks_movement = false,
            .stop_sweep = true,
        };
    case EntityType::Box:
        if (!box::TryApplyBoxImpact(entity_idx, context, state)) {
            return ContactResolution{};
        }
        return ContactResolution{
            .blocks_movement = false,
            .stop_sweep = true,
        };
    default:
        return ContactResolution{};
    }
}

void PlayBlockingCollisionSounds(
    Entity& entity,
    Audio& audio,
    const std::optional<SoundEffect>& entity_sound,
    const std::optional<SoundEffect>& tile_sound,
    bool& played_collision_sound
) {
    bool played_any_sound = false;

    if (entity_sound.has_value()) {
        audio.PlaySoundEffect(*entity_sound);
        played_any_sound = true;
    }

    if (tile_sound.has_value() && (!entity_sound.has_value() || *tile_sound != *entity_sound)) {
        audio.PlaySoundEffect(*tile_sound, kTileTouchSoundVolumeScale);
        played_any_sound = true;
    }

    if (!played_any_sound) {
        return;
    }

    entity.contact_sound_cooldown = kTileTouchSoundCooldownFrames;
    played_collision_sound = true;
}

void MaybePlayTileCollisionSounds(
    std::size_t entity_idx,
    const TileContact& tile_contact,
    const ContactContext& context,
    State& state,
    Audio* audio,
    bool& played_collision_sound
) {
    if (played_collision_sound || audio == nullptr || tile_contact.tile == nullptr) {
        return;
    }
    if (context.phase != ContactPhase::AttemptedBlocked || !context.has_impact) {
        return;
    }
    if (std::abs(context.impact_velocity) < kTileTouchSoundMinImpactVelocity) {
        return;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.contact_sound_cooldown > 0) {
        return;
    }

    const EntityArchetype& entity_archetype = GetEntityArchetype(entity.type_);
    const TileArchetype& tile_archetype = GetTileArchetype(*tile_contact.tile);
    PlayBlockingCollisionSounds(
        entity,
        *audio,
        entity_archetype.collide_sound,
        tile_archetype.collide_sound,
        played_collision_sound
    );
}

void MaybePlayStageBoundsCollisionSounds(
    std::size_t entity_idx,
    const ContactContext& context,
    const BlockingContactSet& contacts,
    State& state,
    Audio* audio,
    bool& played_collision_sound
) {
    if (played_collision_sound || audio == nullptr || !contacts.touches_stage_bounds) {
        return;
    }
    if (context.phase != ContactPhase::AttemptedBlocked || !context.has_impact) {
        return;
    }
    if (context.impact_surface != BlockingImpactSurface::StageBounds) {
        return;
    }
    if (std::abs(context.impact_velocity) < kTileTouchSoundMinImpactVelocity) {
        return;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.contact_sound_cooldown > 0) {
        return;
    }

    const EntityArchetype& entity_archetype = GetEntityArchetype(entity.type_);
    const TileArchetype& border_archetype = GetTileArchetype(state.stage.stage_border_tile);
    PlayBlockingCollisionSounds(
        entity,
        *audio,
        entity_archetype.collide_sound,
        border_archetype.collide_sound,
        played_collision_sound
    );
}

TileEntityContactDispatch TryDispatchEntityTileContactForEntityType(
    std::size_t entity_idx,
    const ContactContext& context,
    State& state
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return TileEntityContactDispatch{};
    }

    const Entity& entity = state.entity_manager.entities[entity_idx];
    const std::optional<TileEntityContactDispatch> cooldown_spec =
        GetEntityTileContactCooldownSpec(entity, context);
    if (cooldown_spec.has_value()) {
        // Tile contacts do not currently have a stable directional cooldown identity.
        return TileEntityContactDispatch{};
    }

    return TileEntityContactDispatch{
        .resolution = TryDispatchEntityTileContactByEntityType(entity_idx, context, state),
    };
}

ContactResolution TryDispatchEntityTileContactForTileType(
    std::size_t entity_idx,
    const TileContact& tile_contact,
    const ContactContext& context,
    State& state,
    Audio* audio
) {
    (void)entity_idx;
    (void)context;
    (void)state;
    (void)audio;

    if (tile_contact.tile == nullptr) {
        return ContactResolution{};
    }

    switch (*tile_contact.tile) {
    default:
        return ContactResolution{};
    }
}

} // namespace

ContactResolution TryDispatchEntityTileContacts(
    std::size_t entity_idx,
    const BlockingContactSet& contacts,
    const ContactContext& context,
    State& state,
    Audio* audio
) {
    ContactResolution aggregate{};

    if (contacts.touches_stage_bounds) {
        aggregate.blocks_movement = true;
    }

    bool touched_blocking_tile = false;
    bool played_collision_sound = false;
    for (const TileContact& tile_contact : contacts.tile_contacts) {
        if (tile_contact.tile == nullptr) {
            continue;
        }
        if (IsTileCollidable(*tile_contact.tile)) {
            touched_blocking_tile = true;
            aggregate.blocks_movement = true;
        }

        MaybePlayTileCollisionSounds(
            entity_idx, tile_contact, context, state, audio, played_collision_sound);

        const ContactResolution tile_resolution = TryDispatchEntityTileContactForTileType(
            entity_idx, tile_contact, context, state, audio);
        aggregate.blocks_movement |= tile_resolution.blocks_movement;
        aggregate.stop_sweep |= tile_resolution.stop_sweep;
    }

    MaybePlayStageBoundsCollisionSounds(
        entity_idx, context, contacts, state, audio, played_collision_sound);

    if (contacts.touches_stage_bounds || touched_blocking_tile) {
        const TileEntityContactDispatch entity_dispatch =
            TryDispatchEntityTileContactForEntityType(entity_idx, context, state);
        aggregate.blocks_movement |= entity_dispatch.resolution.blocks_movement;
        aggregate.stop_sweep |= entity_dispatch.resolution.stop_sweep;
    }

    return aggregate;
}

} // namespace splonks::entities::common
