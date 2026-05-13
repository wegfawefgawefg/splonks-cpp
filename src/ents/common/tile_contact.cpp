#include "ents/common/common.hpp"

#include "ent/spec.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"

#include <cmath>

namespace splonks::ents::common {

namespace {

std::optional<StageBorderSideKind> GetBorderSideForContactContext(const ContactContext& context) {
    if (!context.has_impact) {
        return std::nullopt;
    }

    if (context.impact_axis == BlockingImpactAxis::Horizontal) {
        return context.direction < 0 ? StageBorderSideKind::Left : StageBorderSideKind::Right;
    }
    return context.direction < 0 ? StageBorderSideKind::Top : StageBorderSideKind::Bottom;
}

constexpr std::uint32_t kTileTouchSoundCooldownFrames = 8;
constexpr float kTileTouchSoundMinImpactVelocity = 1.5F;
constexpr float kTileTouchSoundMinPriorTravelDistance = 0.0F;
constexpr float kTileTouchSoundVolumeScale = 0.10F;

ContactResult TryDispatchEntTileContactBySpec(
    std::size_t ent_idx,
    const ContactContext& context,
    State& state
) {
    if (ent_idx >= state.ents.ents.size()) {
        return ContactResult{};
    }

    const Ent& ent = state.ents.ents[ent_idx];
    const EntSpec& spec = GetEntSpec(ent.type_);
    if (spec.on_tile_contact == nullptr) {
        return ContactResult{};
    }
    return spec.on_tile_contact(ent_idx, context, state);
}

void PlayBlockingCollisionSounds(
    Ent& ent,
    State& state,
    const std::optional<AudioAssetId>& ent_sound,
    const std::optional<AudioAssetId>& tile_sound,
    bool& played_collision_sound
) {
    bool played_any_sound = false;

    if (ent_sound.has_value()) {
        (void)PlayEntCenterSoundEmitter(state, ent, *ent_sound);
        played_any_sound = true;
    }

    if (tile_sound.has_value() && (!ent_sound.has_value() || *tile_sound != *ent_sound)) {
        AudioEmitterPlayParams params;
        params.volume_scale = kTileTouchSoundVolumeScale;
        (void)PlayEntCenterSoundEmitter(state, ent, *tile_sound, params);
        played_any_sound = true;
    }

    if (!played_any_sound) {
        return;
    }

    ent.contact_sound_cooldown = kTileTouchSoundCooldownFrames;
    played_collision_sound = true;
}

bool HadRecentMovementForCollisionSound(const Ent& ent) {
    return ent.dist_traveled_this_frame > kTileTouchSoundMinPriorTravelDistance;
}

void MaybePlayTileCollisionSounds(
    std::size_t ent_idx,
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

    Ent& ent = state.ents.ents[ent_idx];
    if (!HadRecentMovementForCollisionSound(ent)) {
        return;
    }
    if (ent.contact_sound_cooldown > 0) {
        return;
    }

    const TileSpec& tile_spec = GetTileSpec(*tile_contact.tile);
    PlayBlockingCollisionSounds(
        ent,
        state,
        ent.collide_sound,
        tile_spec.collide_sound,
        played_collision_sound
    );
}

void MaybePlayStageBoundsCollisionSounds(
    std::size_t ent_idx,
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

    Ent& ent = state.ents.ents[ent_idx];
    if (!HadRecentMovementForCollisionSound(ent)) {
        return;
    }
    if (ent.contact_sound_cooldown > 0) {
        return;
    }

    const std::optional<StageBorderSideKind> side = GetBorderSideForContactContext(context);
    if (!side.has_value()) {
        return;
    }

    const Tile border_tile = state.stage.GetBorderTile(*side);
    if (border_tile == Tile::Air) {
        return;
    }

    const TileSpec& border_spec = GetTileSpec(border_tile);
    PlayBlockingCollisionSounds(
        ent,
        state,
        ent.collide_sound,
        border_spec.collide_sound,
        played_collision_sound
    );
}

} // namespace

ContactResult TryDispatchEntTileContacts(
    std::size_t ent_idx,
    const BlockingContactSet& contacts,
    const ContactContext& context,
    State& state,
    Audio* audio
) {
    ContactResult aggregate{};

    if (contacts.touches_stage_bounds) {
        aggregate.blocks_movement = true;
    }

    bool touched_blocking_tile = false;
    bool played_collision_sound = false;
    for (const TileContact& tile_contact : contacts.tile_contacts) {
        if (tile_contact.tile == nullptr) {
            continue;
        }
        if (tile_contact.blocks_movement) {
            touched_blocking_tile = true;
            aggregate.blocks_movement = true;
        }

        MaybePlayTileCollisionSounds(
            ent_idx, tile_contact, context, state, audio, played_collision_sound);

        const ContactResult ent_tile_resolution =
            TryDispatchEntTileContactBySpec(ent_idx, context, state);
        aggregate.blocks_movement |= ent_tile_resolution.blocks_movement;
        aggregate.stop_sweep |= ent_tile_resolution.stop_sweep;
    }

    MaybePlayStageBoundsCollisionSounds(
        ent_idx, context, contacts, state, audio, played_collision_sound);

    if (contacts.touches_stage_bounds || touched_blocking_tile) {
        const ContactResult ent_tile_resolution =
            TryDispatchEntTileContactBySpec(ent_idx, context, state);
        aggregate.blocks_movement |= ent_tile_resolution.blocks_movement;
        aggregate.stop_sweep |= ent_tile_resolution.stop_sweep;
    }

    return aggregate;
}

} // namespace splonks::ents::common
