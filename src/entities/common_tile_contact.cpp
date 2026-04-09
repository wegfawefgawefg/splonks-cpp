#include "entities/common.hpp"

#include "entities/breakaway_container.hpp"
#include "tile.hpp"

namespace splonks::entities::common {

namespace {

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
    case EntityType::Box:
        if (!breakaway_container::TryApplyBreakawayContainerImpact(entity_idx, context, state)) {
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

}

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
    for (const TileContact& tile_contact : contacts.tile_contacts) {
        if (tile_contact.tile == nullptr) {
            continue;
        }
        if (IsTileCollidable(*tile_contact.tile)) {
            touched_blocking_tile = true;
            aggregate.blocks_movement = true;
        }

        const ContactResolution tile_resolution = TryDispatchEntityTileContactForTileType(
            entity_idx, tile_contact, context, state, audio);
        aggregate.blocks_movement |= tile_resolution.blocks_movement;
        aggregate.stop_sweep |= tile_resolution.stop_sweep;
    }

    if (contacts.touches_stage_bounds || touched_blocking_tile) {
        const TileEntityContactDispatch entity_dispatch =
            TryDispatchEntityTileContactForEntityType(entity_idx, context, state);
        aggregate.blocks_movement |= entity_dispatch.resolution.blocks_movement;
        aggregate.stop_sweep |= entity_dispatch.resolution.stop_sweep;
    }

    return aggregate;
}

} // namespace splonks::entities::common
