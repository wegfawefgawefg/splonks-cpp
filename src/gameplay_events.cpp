#include "gameplay_events.hpp"

#include "audio.hpp"
#include "entity.hpp"
#include "entity/display_states.hpp"
#include "graphics.hpp"
#include "network/net_gameplay_replication.hpp"
#include "network/net_progression.hpp"
#include "network/net_session.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace splonks {

namespace {

struct AnimationPresentationSnapshot {
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

AnimationPresentationSnapshot CaptureDamagePresentation(const Entity& entity) {
    AnimationPresentationSnapshot presentation{
        .animate = static_cast<std::uint8_t>(entity.frame_data_animator.animate ? 1 : 0),
        .animation_id = entity.frame_data_animator.animation_id,
        .animation_frame = static_cast<std::uint16_t>(std::min<std::size_t>(
            entity.frame_data_animator.current_frame,
            std::numeric_limits<std::uint16_t>::max()
        )),
        .animation_time = entity.frame_data_animator.current_time,
        .animation_speed = entity.frame_data_animator.speed,
    };

    std::optional<EntityDisplayState> canonical_display_state;
    if (entity.health == 0 || entity.condition == EntityCondition::Dead) {
        canonical_display_state = EntityDisplayState::Dead;
    } else if (entity.condition == EntityCondition::Stunned) {
        canonical_display_state = EntityDisplayState::Stunned;
    }

    if (canonical_display_state.has_value()) {
        const auto selection = GetFrameDataSelectionForDisplayState(EntityDisplayInput{
            .type_ = entity.type_,
            .display_state = *canonical_display_state,
        });
        if (selection.has_value()) {
            presentation.animate = static_cast<std::uint8_t>(selection->animate ? 1 : 0);
            presentation.animation_id = selection->animation_id;
            presentation.animation_frame = static_cast<std::uint16_t>(
                selection->has_forced_frame
                    ? std::min<std::size_t>(
                          selection->forced_frame,
                          std::numeric_limits<std::uint16_t>::max()
                      )
                    : 0
            );
            presentation.animation_time = 0.0F;
            presentation.animation_speed = 1.0F;
        }
    }

    return presentation;
}

} // namespace

void EmitStageExitRequested(State& state, StageExitId exit_id, PlayerId player_id) {
    GameplayEvent event;
    event.type = GameplayEventType::StageExitRequested;
    event.stage_exit = GameplayStageExitRequested{
        .exit_id = exit_id,
        .player_id = player_id,
    };
    state.gameplay_events.push_back(event);
}

void EmitStageTransitionRequested(State& state, const StageTransitionTarget& target, PlayerId player_id) {
    GameplayEvent event;
    event.type = GameplayEventType::StageTransitionRequested;
    event.stage_transition = GameplayStageTransitionRequested{
        .target = target,
        .player_id = player_id,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntitySpawnedGameplayEvent(
    State& state,
    const Entity& spawned_entity,
    std::optional<VID> held_by_vid
) {
    GameplayEvent event;
    event.type = GameplayEventType::EntitySpawned;
    event.entity_spawned = GameplayEntitySpawned{
        .entity_vid = spawned_entity.vid,
        .held_by_vid = held_by_vid,
        .entity_type = spawned_entity.type_,
        .pos = spawned_entity.pos,
        .vel = spawned_entity.vel + spawned_entity.acc,
        .counter_a = spawned_entity.counter_a,
        .counter_b = spawned_entity.counter_b,
        .use_pressed = spawned_entity.use_state.pressed,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityHeldGameplayEvent(State& state, const Entity& holder, const Entity& held) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityHeld;
    event.entity_held = GameplayEntityHeld{
        .holder_vid = holder.vid,
        .held_vid = held.vid,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityDroppedGameplayEvent(
    State& state,
    const Entity& entity,
    std::optional<VID> dropped_by_vid
) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityDropped;
    event.entity_dropped = GameplayEntityDropped{
        .entity_vid = entity.vid,
        .dropped_by_vid = dropped_by_vid,
        .pos = entity.pos,
        .vel = entity.vel,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityThrownGameplayEvent(
    State& state,
    const Entity& thrower,
    const Entity& thrown,
    Vec2 throw_velocity
) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityThrown;
    event.entity_thrown = GameplayEntityThrown{
        .thrower_vid = thrower.vid,
        .entity_vid = thrown.vid,
        .pos = thrown.pos,
        .vel = throw_velocity,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityDamagedGameplayEvent(
    State& state,
    const Entity& entity,
    DamageType damage_type,
    unsigned int amount,
    std::optional<VID> source_vid
) {
    const AnimationPresentationSnapshot presentation = CaptureDamagePresentation(entity);
    GameplayEvent event;
    event.type = GameplayEventType::EntityDamaged;
    event.entity_damaged = GameplayEntityDamaged{
        .entity_vid = entity.vid,
        .source_vid = source_vid,
        .damage_type = damage_type,
        .amount = amount,
        .remaining_health = entity.health,
        .pos = entity.pos,
        .vel = entity.vel,
        .acc = entity.acc,
        .stun_timer = entity.stun_timer,
        .projectile_contact_timer = entity.projectile_contact_timer,
        .condition = static_cast<std::uint8_t>(entity.condition),
        .grounded = static_cast<std::uint8_t>(entity.grounded ? 1 : 0),
        .animate = presentation.animate,
        .animation_id = presentation.animation_id,
        .animation_frame = presentation.animation_frame,
        .animation_time = presentation.animation_time,
        .animation_speed = presentation.animation_speed,
    };
    state.gameplay_events.push_back(event);
}

void EmitEntityStatePatchedGameplayEvent(State& state, const Entity& source, const Entity& entity) {
    GameplayEvent event;
    event.type = GameplayEventType::EntityStatePatched;
    event.entity_state_patched = GameplayEntityStatePatched{
        .entity_vid = entity.vid,
        .source_vid = source.vid,
        .entity_a_vid = entity.entity_a,
        .pos = entity.pos,
        .vel = entity.vel,
        .acc = entity.acc,
        .point_a = entity.point_a,
        .health = entity.health,
        .stun_timer = entity.stun_timer,
        .projectile_contact_timer = entity.projectile_contact_timer,
        .rotation = entity.rotation,
        .condition = static_cast<std::uint8_t>(entity.condition),
        .grounded = static_cast<std::uint8_t>(entity.grounded ? 1 : 0),
        .active = static_cast<std::uint8_t>(entity.active ? 1 : 0),
        .has_physics = static_cast<std::uint8_t>(entity.has_physics ? 1 : 0),
        .can_collide = static_cast<std::uint8_t>(entity.can_collide ? 1 : 0),
        .can_apply_projectile_contact =
            static_cast<std::uint8_t>(entity.can_apply_projectile_contact ? 1 : 0),
        .facing = static_cast<std::uint8_t>(entity.facing == LeftOrRight::Right ? 1 : 0),
    };
    state.gameplay_events.push_back(event);
}

void EmitTileBrokenGameplayEvent(State& state, const IVec2& tile_pos) {
    GameplayEvent event;
    event.type = GameplayEventType::TileBroken;
    event.tile_broken = GameplayTileBroken{
        .tile_pos = tile_pos,
    };
    state.gameplay_events.push_back(event);
}

void EmitRopeTilePlacedGameplayEvent(State& state, const Entity& source_entity, const IVec2& tile_pos) {
    GameplayEvent event;
    event.type = GameplayEventType::RopeTilePlaced;
    event.rope_tile_placed = GameplayRopeTilePlaced{
        .source_vid = source_entity.vid,
        .tile_pos = tile_pos,
    };
    state.gameplay_events.push_back(event);
}

void EmitPresentationCommandGameplayEvent(State& state, const PresentationCommand& command) {
    GameplayEvent event;
    event.type = GameplayEventType::PresentationCommand;
    event.presentation_command = command;
    state.gameplay_events.push_back(event);
}

void ProcessGameplayEvents(State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;

    const std::vector<GameplayEvent> events = std::move(state.gameplay_events);
    state.gameplay_events.clear();

    for (const GameplayEvent& event : events) {
        switch (event.type) {
        case GameplayEventType::StageExitRequested:
            if (state.pending_stage_transition.has_value()) {
                continue;
            }
            if (!IsStageExitAllowed(state, event.stage_exit.exit_id)) {
                continue;
            }
            if (state.net_session.role == network::NetRole::Peer) {
                network::RequestStageExit(state, event.stage_exit.exit_id);
                QueueStageExitTransition(state, event.stage_exit.exit_id);
                continue;
            }
            QueueStageExitTransition(state, event.stage_exit.exit_id);
            break;
        case GameplayEventType::StageTransitionRequested:
            if (state.pending_stage_transition.has_value() ||
                state.net_session.role == network::NetRole::Peer) {
                continue;
            }
            QueueStageTransition(state, event.stage_transition.target);
            break;
        case GameplayEventType::EntitySpawned:
        case GameplayEventType::EntityHeld:
        case GameplayEventType::EntityDropped:
        case GameplayEventType::EntityThrown:
        case GameplayEventType::EntityDamaged:
        case GameplayEventType::EntityStatePatched:
        case GameplayEventType::TileBroken:
        case GameplayEventType::RopeTilePlaced:
        case GameplayEventType::PresentationCommand:
            network::ReplicateGameplayEvent(state, event);
            break;
        }
    }
}

} // namespace splonks
