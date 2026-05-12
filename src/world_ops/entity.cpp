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

} // namespace splonks::world_ops
