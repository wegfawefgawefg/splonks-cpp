#include "world_ops.hpp"

#include "audio.hpp"
#include "buying.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "world_query.hpp"

namespace splonks::world_ops {

namespace {

bool AreEntitiesOverlappingForInteract(
    const Entity& source,
    const Entity& target,
    const State& state,
    const Graphics& graphics
) {
    const AABB source_aabb = entities::common::GetContactAabbForEntity(source, graphics);
    const Vec2 source_center = (source_aabb.tl + source_aabb.br) / 2.0F;
    const AABB target_aabb = GetNearestWorldAabb(
        state.stage,
        source_center,
        entities::common::GetContactAabbForEntity(target, graphics)
    );
    return AabbsIntersect(source_aabb, target_aabb);
}

} // namespace

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

bool TryApplyInteractEntity(
    VID source_vid,
    VID target_vid,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    Entity* const source = state.entity_manager.GetEntityMut(source_vid);
    Entity* const target = state.entity_manager.GetEntityMut(target_vid);
    if (source == nullptr || target == nullptr ||
        !source->active || !target->active ||
        source->condition == EntityCondition::Dead) {
        return false;
    }

    if (!AreEntitiesOverlappingForInteract(*source, *target, state, graphics)) {
        return false;
    }

    if (target->buyable.active) {
        return TryBuyEntity(target->vid.id, source->vid.id, state, graphics, audio);
    }

    const EntityArchetype& archetype = GetEntityArchetype(target->type_);
    if (archetype.on_interact == nullptr) {
        return false;
    }
    return archetype.on_interact(target->vid.id, source->vid.id, state, graphics, audio);
}

} // namespace splonks::world_ops
