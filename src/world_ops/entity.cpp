#include "world_ops.hpp"

#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "state.hpp"

namespace splonks::world_ops {

Entity* SpawnConfiguredEntity(
    State& state,
    const EntitySpawnSetup& setup,
    std::optional<VID> held_by_vid
) {
    (void)held_by_vid;

    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return nullptr;
    }

    if (setup) {
        setup(*entity);
    }
    return entity;
}

Entity* SpawnEntity(
    State& state,
    EntityType type_,
    const EntitySpawnSetup& setup,
    std::optional<VID> held_by_vid
) {
    return SpawnConfiguredEntity(
        state,
        [&](Entity& entity) {
            SetEntityAs(entity, type_);
            if (setup) {
                setup(entity);
            }
        },
        held_by_vid
    );
}

bool DeactivateEntity(State& state, VID entity_vid) {
    Entity* const entity = state.entity_manager.GetEntityMut(entity_vid);
    if (entity == nullptr || !entity->active) {
        return false;
    }

    entities::common::ReleaseEntityFromHolder(*entity, state);

    state.entity_manager.SetInactive(entity->vid.id);
    return true;
}

void PatchEntityState(State& state, const Entity& source, const Entity& entity) {
    (void)state;
    (void)source;
    (void)entity;
}

void PatchPlayerState(State& state, const Entity& player) {
    (void)state;
    (void)player;
}

void PatchRunState(State& state, bool include_snapshot_fingerprint) {
    (void)state;
    (void)include_snapshot_fingerprint;
}

void MarkEntityHeld(
    State& state,
    const Entity& holder,
    const Entity& held,
    AttachmentMode attachment_mode
) {
    (void)state;
    (void)holder;
    (void)held;
    (void)attachment_mode;
}

void MarkEntityDropped(
    State& state,
    const Entity& entity,
    std::optional<VID> dropped_by_vid
) {
    (void)state;
    (void)entity;
    (void)dropped_by_vid;
}

void MarkEntityThrown(State& state, const Entity& thrower, const Entity& thrown, Vec2 throw_velocity) {
    (void)state;
    (void)thrower;
    (void)thrown;
    (void)throw_velocity;
}

void CommitEntityDamaged(
    State& state,
    const Entity& entity,
    DamageType damage_type,
    unsigned int amount,
    std::optional<VID> source_vid
) {
    (void)state;
    (void)entity;
    (void)damage_type;
    (void)amount;
    (void)source_vid;
}

} // namespace splonks::world_ops
