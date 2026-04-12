#include "step_entities.hpp"

#include "entities/common.hpp"
#include "entity.hpp"
#include "entity_archetype.hpp"
#include "entity_core_types.hpp"
#include "entity_manager.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace splonks {

namespace {

bool HasUseActivity(const Entity& entity) {
    return entity.use_state.down || entity.use_state.pressed || entity.use_state.released;
}

void ClearUseEdgesAfterFrame(Entity& entity) {
    entity.use_state.pressed = false;
    if (!entity.use_state.down) {
        entity.use_state.released = false;
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
void StepEntities(State& state, Audio& audio, Graphics& graphics, float dt) {
    // extract and step the player first, this is so held items and stuff are updated with the latest player pos.
    // also makes sense to do this first, since the player is the most important entity.

    if (state.player_vid) {
        if (const Entity* const player = state.entity_manager.GetEntity(*state.player_vid)) {
            if (player->active) {
                ClearTransientMovementFlags(state.entity_manager.entities[state.player_vid->id]);
                entities::common::CommonStep(state.player_vid->id, state, graphics, audio, dt);
                if (state.entity_manager.entities[state.player_vid->id].active) {
                    const EntityArchetype& archetype =
                        GetEntityArchetype(state.entity_manager.entities[state.player_vid->id].type_);
                    if (HasUseActivity(state.entity_manager.entities[state.player_vid->id]) &&
                        archetype.on_use != nullptr) {
                        archetype.on_use(state.player_vid->id, state, graphics, audio);
                    }
                    if (state.entity_manager.entities[state.player_vid->id].active &&
                        archetype.step_logic != nullptr) {
                        archetype.step_logic(state.player_vid->id, state, graphics, audio, dt);
                    }
                }
                if (state.entity_manager.entities[state.player_vid->id].active) {
                    entities::common::CommonPostStep(
                        state.player_vid->id,
                        state,
                        graphics,
                        audio,
                        dt
                    );
                }
            }
            if (state.entity_manager.entities[state.player_vid->id].active &&
                state.entity_manager.entities[state.player_vid->id].has_physics) {
                const EntityArchetype& archetype =
                    GetEntityArchetype(state.entity_manager.entities[state.player_vid->id].type_);
                if (archetype.step_physics != nullptr) {
                    archetype.step_physics(state.player_vid->id, state, graphics, audio, dt);
                } else {
                    entities::common::StepStandardPhysics(
                        state.player_vid->id,
                        state,
                        graphics,
                        audio,
                        dt
                    );
                }
            }
            entities::common::ApplyDeactivateConditions(state.player_vid->id, state);
            state.UpdateSidForEntity(state.player_vid->id, graphics);
            if (Entity* const mutable_entity = state.entity_manager.GetEntityMut(*state.player_vid)) {
                ClearUseEdgesAfterFrame(*mutable_entity);
                mutable_entity->last_condition = mutable_entity->condition;
                mutable_entity->last_ai_state = mutable_entity->ai_state;
            }
        }
    }

    for (std::size_t entity_idx = 0; entity_idx < state.entity_manager.entities.size(); ++entity_idx) {
        const Entity& entity = state.entity_manager.GetEntityById(entity_idx);
        const EntityType type_ = entity.type_;

        if (entity.active) {
            if (type_ == EntityType::Player) {
                continue;
            }
            ClearTransientMovementFlags(state.entity_manager.entities[entity_idx]);
            entities::common::CommonStep(entity_idx, state, graphics, audio, dt);
            if (!state.entity_manager.entities[entity_idx].active) {
                continue;
            }
            const EntityArchetype& archetype = GetEntityArchetype(type_);
            if (HasUseActivity(state.entity_manager.entities[entity_idx]) &&
                archetype.on_use != nullptr) {
                archetype.on_use(entity_idx, state, graphics, audio);
            }
            if (state.entity_manager.entities[entity_idx].active &&
                archetype.step_logic != nullptr) {
                archetype.step_logic(entity_idx, state, graphics, audio, dt);
            }
            if (!state.entity_manager.entities[entity_idx].active) {
                continue;
            }
            entities::common::CommonPostStep(entity_idx, state, graphics, audio, dt);
            if (!state.entity_manager.entities[entity_idx].active) {
                continue;
            }

            if (state.entity_manager.entities[entity_idx].has_physics) {
                if (archetype.step_physics != nullptr) {
                    archetype.step_physics(entity_idx, state, graphics, audio, dt);
                } else {
                    entities::common::StepStandardPhysics(
                        entity_idx,
                        state,
                        graphics,
                        audio,
                        dt
                    );
                }
            }
            entities::common::ApplyDeactivateConditions(entity_idx, state);
            state.UpdateSidForEntity(entity_idx, graphics);
            Entity& mutable_entity = state.entity_manager.entities[entity_idx];
            ClearUseEdgesAfterFrame(mutable_entity);
            mutable_entity.last_condition = mutable_entity.condition;
            mutable_entity.last_ai_state = mutable_entity.ai_state;
        }
    }
}

} // namespace splonks
