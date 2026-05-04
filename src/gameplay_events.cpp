#include "gameplay_events.hpp"

#include "audio.hpp"
#include "entity.hpp"
#include "entity/display_states.hpp"
#include "gameplay_authority.hpp"
#include "graphics.hpp"
#include "network/net_entity_links.hpp"
#include "network/net_event.hpp"
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

bool ShouldReplicateEntityEvent(const State& state, VID entity_vid) {
    if (state.net_session.role == network::NetRole::Offline) {
        return false;
    }
    const PlayerSlot* const player_slot = state.players.FindByEntityVid(entity_vid);
    return player_slot == nullptr ||
           player_slot->connection_kind != PlayerConnectionKind::Remote;
}

bool ShouldReplicateInteractionSourceEvent(const State& state, VID source_vid) {
    return state.net_session.role != network::NetRole::Offline &&
           HasLocalGameplayAuthorityForInteractionSource(state, source_vid);
}

bool IsPlayerBodyDamageBetweenPlayers(const State& state, const GameplayEntityDamaged& gameplay_event) {
    if (!gameplay_event.source_vid.has_value() ||
        (gameplay_event.damage_type != DamageType::Attack &&
         gameplay_event.damage_type != DamageType::JumpOn)) {
        return false;
    }

    const Entity* const target = state.entity_manager.GetEntity(gameplay_event.entity_vid);
    const Entity* const source = state.entity_manager.GetEntity(*gameplay_event.source_vid);
    return target != nullptr && source != nullptr &&
           IsPlayerLikeEntityType(target->type_) &&
           IsPlayerLikeEntityType(source->type_);
}

void EnqueueEntitySpawnedReplicationEvent(State& state, const GameplayEntitySpawned& gameplay_event) {
    if (state.net_session.role == network::NetRole::Offline) {
        return;
    }

    network::NetEntityId net_id = state.net_session.FindNetEntityId(gameplay_event.entity_vid)
        .value_or(network::kInvalidNetEntityId);
    if (net_id == network::kInvalidNetEntityId) {
        net_id = state.net_session.AllocateLocalEntityId();
        state.net_session.LinkEntity(net_id, gameplay_event.entity_vid);
    }
    state.net_session.SetEntityOwner(net_id, state.net_session.local_player_id);

    network::NetEntityId held_by_id = network::kInvalidNetEntityId;
    if (gameplay_event.held_by_vid.has_value()) {
        held_by_id = network::GetOrAssignReplicatedEntityId(
            state,
            *gameplay_event.held_by_vid
        );
    }

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntitySpawned;
    event.payload = network::EntitySpawnedEvent{
        .entity_id = net_id,
        .entity_type = gameplay_event.entity_type,
        .held_by_id = held_by_id,
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
        .owner = network::NetEntityOwner::Player(state.net_session.local_player_id),
        .counter_a = gameplay_event.counter_a,
        .counter_b = gameplay_event.counter_b,
        .use_pressed = gameplay_event.use_pressed,
    };
    state.net_session.EnqueueLocalEvent(event);
}

void EnqueueEntityHeldReplicationEvent(State& state, const GameplayEntityHeld& gameplay_event) {
    if (!ShouldReplicateInteractionSourceEvent(state, gameplay_event.holder_vid)) {
        return;
    }

    const network::NetEntityId held_id =
        network::GetOrAssignReplicatedEntityId(state, gameplay_event.held_vid);
    state.net_session.SetEntityOwner(held_id, state.net_session.local_player_id);

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntityHeld;
    event.payload = network::EntityHeldEvent{
        .holder_id = network::GetOrAssignReplicatedEntityId(state, gameplay_event.holder_vid),
        .held_id = held_id,
    };
    state.net_session.EnqueueLocalEvent(event);
}

void EnqueueEntityDroppedReplicationEvent(State& state, const GameplayEntityDropped& gameplay_event) {
    const bool should_replicate =
        gameplay_event.dropped_by_vid.has_value()
            ? ShouldReplicateInteractionSourceEvent(state, *gameplay_event.dropped_by_vid)
            : ShouldReplicateEntityEvent(state, gameplay_event.entity_vid);
    if (!should_replicate) {
        return;
    }

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntityDropped;
    event.payload = network::EntityDroppedEvent{
        .entity_id = network::GetOrAssignReplicatedEntityId(state, gameplay_event.entity_vid),
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
    };
    state.net_session.EnqueueLocalEvent(event);
}

void EnqueueEntityThrownReplicationEvent(State& state, const GameplayEntityThrown& gameplay_event) {
    if (!ShouldReplicateInteractionSourceEvent(state, gameplay_event.thrower_vid)) {
        return;
    }

    const network::NetEntityId entity_id =
        network::GetOrAssignReplicatedEntityId(state, gameplay_event.entity_vid);
    state.net_session.SetEntityOwner(entity_id, state.net_session.local_player_id);

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntityThrown;
    event.payload = network::EntityThrownEvent{
        .entity_id = entity_id,
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
        .thrower_id = network::GetOrAssignReplicatedEntityId(state, gameplay_event.thrower_vid),
    };
    state.net_session.EnqueueLocalEvent(event);
}

void EnqueueEntityDamagedReplicationEvent(State& state, const GameplayEntityDamaged& gameplay_event) {
    if (IsPlayerBodyDamageBetweenPlayers(state, gameplay_event)) {
        return;
    }

    const bool should_replicate =
        gameplay_event.source_vid.has_value()
            ? ShouldReplicateInteractionSourceEvent(state, *gameplay_event.source_vid)
            : ShouldReplicateEntityEvent(state, gameplay_event.entity_vid);
    if (!should_replicate) {
        return;
    }

    const network::NetEntityId entity_id =
        network::GetOrAssignReplicatedEntityId(state, gameplay_event.entity_vid);
    if (state.players.FindByEntityVid(gameplay_event.entity_vid) == nullptr) {
        state.net_session.SetEntityOwner(entity_id, state.net_session.local_player_id);
    }

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntityDamaged;
    event.payload = network::EntityDamagedEvent{
        .entity_id = entity_id,
        .source_entity_id = gameplay_event.source_vid.has_value()
            ? network::GetOrAssignReplicatedEntityId(state, *gameplay_event.source_vid)
            : network::kInvalidNetEntityId,
        .amount = gameplay_event.amount,
        .remaining_health = gameplay_event.remaining_health,
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
        .acc = gameplay_event.acc,
        .stun_timer = gameplay_event.stun_timer,
        .projectile_contact_timer = gameplay_event.projectile_contact_timer,
        .condition = gameplay_event.condition,
        .grounded = gameplay_event.grounded,
        .animate = gameplay_event.animate,
        .animation_id = gameplay_event.animation_id,
        .animation_frame = gameplay_event.animation_frame,
        .animation_time = gameplay_event.animation_time,
        .animation_speed = gameplay_event.animation_speed,
        .damage_type = gameplay_event.damage_type,
    };
    state.net_session.EnqueueLocalEvent(event);
}

void EnqueueEntityStatePatchedReplicationEvent(
    State& state,
    const GameplayEntityStatePatched& gameplay_event
) {
    if (!ShouldReplicateInteractionSourceEvent(state, gameplay_event.source_vid)) {
        return;
    }

    const network::NetEntityId entity_id =
        network::GetOrAssignReplicatedEntityId(state, gameplay_event.entity_vid);
    if (state.players.FindByEntityVid(gameplay_event.entity_vid) == nullptr) {
        state.net_session.SetEntityOwner(entity_id, state.net_session.local_player_id);
    }

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::EntityStatePatched;
    event.payload = network::EntityStatePatchedEvent{
        .entity_id = entity_id,
        .source_entity_id =
            network::GetOrAssignReplicatedEntityId(state, gameplay_event.source_vid),
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
        .acc = gameplay_event.acc,
        .health = gameplay_event.health,
        .stun_timer = gameplay_event.stun_timer,
        .condition = gameplay_event.condition,
        .grounded = gameplay_event.grounded,
        .active = gameplay_event.active,
    };
    state.net_session.EnqueueLocalEvent(event);
}

void EnqueueRopeTilePlacedReplicationEvent(State& state, const GameplayRopeTilePlaced& gameplay_event) {
    if (state.net_session.role == network::NetRole::Offline) {
        return;
    }

    network::NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = network::NetEventType::RopeTilePlaced;
    event.payload = network::RopeTilePlacedEvent{
        .tile_pos = gameplay_event.tile_pos,
        .source_entity_id = state.net_session.FindNetEntityId(gameplay_event.source_vid)
                                .value_or(network::kInvalidNetEntityId),
    };
    state.net_session.EnqueueLocalEvent(event);
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
        .pos = entity.pos,
        .vel = entity.vel,
        .acc = entity.acc,
        .health = entity.health,
        .stun_timer = entity.stun_timer,
        .condition = static_cast<std::uint8_t>(entity.condition),
        .grounded = static_cast<std::uint8_t>(entity.grounded ? 1 : 0),
        .active = static_cast<std::uint8_t>(entity.active ? 1 : 0),
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
            }
            QueueStageExitTransition(state, event.stage_exit.exit_id);
            break;
        case GameplayEventType::StageTransitionRequested:
            if (state.pending_stage_transition.has_value()) {
                continue;
            }
            QueueStageTransition(state, event.stage_transition.target);
            break;
        case GameplayEventType::EntitySpawned:
            EnqueueEntitySpawnedReplicationEvent(state, event.entity_spawned);
            break;
        case GameplayEventType::EntityHeld:
            EnqueueEntityHeldReplicationEvent(state, event.entity_held);
            break;
        case GameplayEventType::EntityDropped:
            EnqueueEntityDroppedReplicationEvent(state, event.entity_dropped);
            break;
        case GameplayEventType::EntityThrown:
            EnqueueEntityThrownReplicationEvent(state, event.entity_thrown);
            break;
        case GameplayEventType::EntityDamaged:
            EnqueueEntityDamagedReplicationEvent(state, event.entity_damaged);
            break;
        case GameplayEventType::EntityStatePatched:
            EnqueueEntityStatePatchedReplicationEvent(state, event.entity_state_patched);
            break;
        case GameplayEventType::RopeTilePlaced:
            EnqueueRopeTilePlacedReplicationEvent(state, event.rope_tile_placed);
            break;
        }
    }
}

} // namespace splonks
