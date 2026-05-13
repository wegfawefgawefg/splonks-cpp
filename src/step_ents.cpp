#include "step_entities.hpp"

#include "entities/common/common.hpp"
#include "controls.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "entity/manager.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace splonks {

namespace {

bool HasUseActivity(const Entity& entity) {
    return entity.use_state.down || entity.use_state.pressed || entity.use_state.released;
}

bool HasAreaCallbacks(const Entity& entity) {
    return entity.on_area_enter != nullptr || entity.on_area_exit != nullptr;
}

std::vector<VID> GetAreaOverlapVids(std::size_t area_idx, const State& state) {
    const Entity& area_entity = state.entity_manager.entities[area_idx];
    const AABB area = area_entity.GetAABB();

    std::vector<VID> overlaps;
    for (const VID& vid : QueryEntitiesInAabb(state, area, area_entity.vid)) {
        const Entity* const other = state.entity_manager.GetEntity(vid);
        if (other == nullptr || !other->active) {
            continue;
        }
        if (!WorldAabbsIntersect(state.stage, area, other->GetAABB())) {
            continue;
        }
        overlaps.push_back(vid);
    }
    return overlaps;
}

bool ContainsVid(const std::vector<VID>& vids, VID vid) {
    return std::find(vids.begin(), vids.end(), vid) != vids.end();
}

void StepAreaEntityOverlaps(State& state, Graphics& graphics, Audio& audio) {
    for (const VID& area_vid : state.area_listener_vids) {
        const Entity* const area_entity_ptr = state.entity_manager.GetEntity(area_vid);
        if (area_entity_ptr == nullptr) {
            continue;
        }

        const std::size_t area_idx = area_vid.id;
        const Entity& area_entity = *area_entity_ptr;
        if (!area_entity.active || !HasAreaCallbacks(area_entity)) {
            continue;
        }
        const std::vector<VID> previous_overlaps = area_entity.inside_vids.value_or(std::vector<VID>{});
        const std::vector<VID> current_overlaps = GetAreaOverlapVids(area_idx, state);

        for (const VID& vid : previous_overlaps) {
            if (ContainsVid(current_overlaps, vid)) {
                continue;
            }
            Entity* const area_mut = state.entity_manager.GetEntityMut(area_entity.vid);
            if (area_mut == nullptr || !area_mut->active || area_mut->on_area_exit == nullptr) {
                continue;
            }
            if (state.entity_manager.GetEntity(vid) == nullptr) {
                continue;
            }
            area_mut->on_area_exit(area_idx, vid.id, state, graphics, audio);
        }

        for (const VID& vid : current_overlaps) {
            if (ContainsVid(previous_overlaps, vid)) {
                continue;
            }
            Entity* const area_mut = state.entity_manager.GetEntityMut(area_entity.vid);
            if (area_mut == nullptr || !area_mut->active || area_mut->on_area_enter == nullptr) {
                continue;
            }
            if (state.entity_manager.GetEntity(vid) == nullptr) {
                continue;
            }
            area_mut->on_area_enter(area_idx, vid.id, state, graphics, audio);
        }

        Entity* const area_mut = state.entity_manager.GetEntityMut(area_entity.vid);
        if (area_mut == nullptr || !area_mut->active) {
            continue;
        }
        area_mut->inside_vids = current_overlaps;
    }
}

void ApplyStageWrapAndVoidDeath(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    state.stage.NormalizeEntityPositionForWrap(entity);

    if (!state.stage.HasVoidDeathY()) {
        return;
    }
    if (entity.health == 0 || entity.condition == EntityCondition::Dead) {
        return;
    }

    const auto [_tl, br] = entity.GetBounds();
    if (br.y <= state.stage.GetVoidDeathY()) {
        return;
    }

    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.health = 0;
    entities::common::DieIfDead(entity_idx, state, audio);
}

void ClearUseEdgesAfterFrame(Entity& entity) {
    entity.use_state.pressed = false;
    if (!entity.use_state.down) {
        entity.use_state.released = false;
        entity.use_state.frames = 0;
        entity.use_state.user_vid.reset();
        entity.use_state.source = AttachmentMode::None;
    }
}

} // namespace

/** Step the logic of entities, followed by their physics.  */
/*  Stepping Entities:
        Your entity must implement the following:
            step_logic_<entity_name>(game, map):
                - for logic
                - state machine stuff
                - other entity following
                - actions
            step_physics_<entity_name>(base_entity, game, map):
                - for motion
            step_animation_<entity_name>(base entity, game, map):
                - for stepping the animation state machine basically (can have common impls too)
        If several of your entities end up internally sharing a mutual implementation of some feature,
        you dont need a match out here or in step, just use the function which allows for that feature internally.
        compiler-san will make sure you have the necessary fields if you pass yourself into say,
            flying_entities_physics_step(*self)). because if you dont, compiler: reee
        You may also need to check if an entity has some trait, and that can go in entity, and instead of having a table you can just
        force every entity to implement is_burnable() for example. and the dyn table will take care of that for you.
*/
namespace {

void StepOneEntity(std::size_t entity_idx, State& state, Audio& audio, Graphics& graphics, float dt) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    if (!state.entity_manager.entities[entity_idx].active) {
        return;
    }

    ClearTransientMovementFlags(state.entity_manager.entities[entity_idx]);
    entities::common::CommonStep(entity_idx, state, graphics, audio, dt);
    if (!state.entity_manager.entities[entity_idx].active) {
        return;
    }

    const Entity& current_entity = state.entity_manager.entities[entity_idx];
    if (HasUseActivity(current_entity) && current_entity.on_use != nullptr) {
        current_entity.on_use(entity_idx, state, graphics, audio);
    }
    if (state.entity_manager.entities[entity_idx].active && current_entity.step_logic != nullptr) {
        current_entity.step_logic(entity_idx, state, graphics, audio, dt);
    }
    if (!state.entity_manager.entities[entity_idx].active) {
        return;
    }

    entities::common::CommonPostStep(entity_idx, state, graphics, audio, dt);
    if (!state.entity_manager.entities[entity_idx].active) {
        return;
    }

    if (state.entity_manager.entities[entity_idx].has_physics) {
        if (current_entity.step_physics != nullptr) {
            current_entity.step_physics(entity_idx, state, graphics, audio, dt);
        } else {
            entities::common::StepStandardPhysics(entity_idx, state, graphics, audio, dt);
        }
        if (state.entity_manager.entities[entity_idx].active) {
            ApplyStageWrapAndVoidDeath(entity_idx, state, audio);
        }
    }

    entities::common::ApplyDeactivateConditions(entity_idx, state);
    state.UpdateSidForEntity(entity_idx, graphics);
    Entity& mutable_entity = state.entity_manager.entities[entity_idx];
    ClearUseEdgesAfterFrame(mutable_entity);
    mutable_entity.last_condition = mutable_entity.condition;
    mutable_entity.last_ai_state = mutable_entity.ai_state;
}

bool IsPlayerSlotEntity(const State& state, const Entity& entity) {
    return state.players.FindPlayerIdForEntity(entity.vid).has_value();
}

bool ShouldRunFullLocalStepForNonPlayerEntity(const State& state, const Entity& entity) {
    (void)state;
    (void)entity;
    return true;
}

bool ShouldRunFullPlayerSlotStep(const State& state, const PlayerSlot& slot) {
    (void)state;
    if (!slot.entity_vid.has_value()) {
        return false;
    }
    return slot.connected;
}

void TryReleaseHeldPlayerSlotFromJump(const PlayerSlot& slot, State& state) {
    if (!slot.entity_vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*slot.entity_vid);
    if (entity == nullptr ||
        !entity->active ||
        !entity->held_by_vid.has_value() ||
        entity->attachment_mode != AttachmentMode::Held ||
        entity->condition != EntityCondition::Normal) {
        return;
    }

    const controls::ControlIntent control = controls::GetControlIntentForEntity(*entity, state);
    if (!control.jump_pressed) {
        return;
    }

    entities::common::ReleaseEntityFromHolderIfAttached(*entity, state);
    entity->grounded = false;
    entity->coyote_time = 2;
}

} // namespace

void StepEntities(State& state, Audio& audio, Graphics& graphics, float dt) {
    // Step all player-controlled entities first so held items and attached
    // visuals follow the latest player positions.
    for (const PlayerSlot& slot : state.players.slots) {
        if (ShouldRunFullPlayerSlotStep(state, slot)) {
            TryReleaseHeldPlayerSlotFromJump(slot, state);
            StepOneEntity(slot.entity_vid->id, state, audio, graphics, dt);
        }
    }

    for (std::size_t entity_idx = 0; entity_idx < state.entity_manager.entities.size(); ++entity_idx) {
        const Entity& entity = state.entity_manager.GetEntityById(entity_idx);

        if (entity.active) {
            if (IsPlayerSlotEntity(state, entity)) {
                continue;
            }
            if (ShouldRunFullLocalStepForNonPlayerEntity(state, entity)) {
                StepOneEntity(entity_idx, state, audio, graphics, dt);
            }
        }
    }

    constexpr int kAttachmentSyncPasses = 8;
    for (int pass = 0; pass < kAttachmentSyncPasses; ++pass) {
        for (std::size_t entity_idx = 0; entity_idx < state.entity_manager.entities.size(); ++entity_idx) {
            entities::common::SyncEntityAttachments(entity_idx, state, graphics);
        }
    }

    StepAreaEntityOverlaps(state, graphics, audio);
}

} // namespace splonks
