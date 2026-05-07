#include "network/net_gameplay_replication.hpp"

#include "entity.hpp"
#include "effects.hpp"
#include "entity_tool_inventory.hpp"
#include "gameplay_authority.hpp"
#include "network/net_entity_links.hpp"
#include "network/net_event.hpp"
#include "network/net_session.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstddef>

namespace splonks::network {

namespace {

bool ShouldReplicateEntityEvent(const State& state, VID entity_vid) {
    if (state.net_session.role == NetRole::Offline) {
        return false;
    }
    const PlayerSlot* const player_slot = state.players.FindByEntityVid(entity_vid);
    return player_slot == nullptr ||
           player_slot->connection_kind != PlayerConnectionKind::Remote;
}

bool ShouldReplicateInteractionSourceEvent(const State& state, VID source_vid) {
    return state.net_session.role != NetRole::Offline &&
           HasLocalGameplayAuthorityForInteractionSource(state, source_vid);
}

NetEntityId GetReplicatedEntityLinkId(State& state, const std::optional<VID>& vid) {
    if (!vid.has_value()) {
        return kInvalidNetEntityId;
    }
    const Entity* const linked_entity = state.entity_manager.GetEntity(*vid);
    if (linked_entity == nullptr || !linked_entity->active) {
        return kInvalidNetEntityId;
    }
    return GetOrAssignReplicatedEntityId(state, *vid);
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
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetEntityId net_id = state.net_session.FindNetEntityId(gameplay_event.entity_vid)
        .value_or(kInvalidNetEntityId);
    if (net_id == kInvalidNetEntityId) {
        net_id = state.net_session.AllocateLocalEntityId();
        state.net_session.LinkEntity(net_id, gameplay_event.entity_vid);
    }
    state.net_session.SetEntityOwner(net_id, std::nullopt);

    NetEntityId held_by_id = kInvalidNetEntityId;
    if (gameplay_event.held_by_vid.has_value()) {
        held_by_id = GetOrAssignReplicatedEntityId(state, *gameplay_event.held_by_vid);
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::EntitySpawned;
    event.payload = EntitySpawnedEvent{
        .entity_id = net_id,
        .entity_type = gameplay_event.entity_type,
        .held_by_id = held_by_id,
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
        .acc = gameplay_event.acc,
        .owner = NetEntityOwner::Coordinator(),
        .counter_a = gameplay_event.counter_a,
        .counter_b = gameplay_event.counter_b,
        .use_pressed = gameplay_event.use_pressed,
        .animate = gameplay_event.animate,
        .animation_id = gameplay_event.animation_id,
        .animation_frame = gameplay_event.animation_frame,
        .animation_time = gameplay_event.animation_time,
        .animation_speed = gameplay_event.animation_speed,
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueEntityDeactivatedReplicationEvent(
    State& state,
    const GameplayEntityDeactivated& gameplay_event
) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const std::optional<NetEntityId> entity_id =
        state.net_session.FindNetEntityId(gameplay_event.entity_vid);
    if (!entity_id.has_value()) {
        return;
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::EntityDeactivated;
    event.payload = EntityIdEvent{
        .entity_id = *entity_id,
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueEntityHeldReplicationEvent(State& state, const GameplayEntityHeld& gameplay_event) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const NetEntityId held_id = GetOrAssignReplicatedEntityId(state, gameplay_event.held_vid);

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::EntityHeld;
    event.payload = EntityHeldEvent{
        .holder_id = GetOrAssignReplicatedEntityId(state, gameplay_event.holder_vid),
        .held_id = held_id,
        .attachment_mode = gameplay_event.attachment_mode,
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueEntityDroppedReplicationEvent(State& state, const GameplayEntityDropped& gameplay_event) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::EntityDropped;
    event.payload = EntityDroppedEvent{
        .entity_id = GetOrAssignReplicatedEntityId(state, gameplay_event.entity_vid),
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueEntityThrownReplicationEvent(State& state, const GameplayEntityThrown& gameplay_event) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const NetEntityId entity_id = GetOrAssignReplicatedEntityId(state, gameplay_event.entity_vid);
    if (state.players.FindByEntityVid(gameplay_event.entity_vid) == nullptr) {
        state.net_session.SetEntityOwner(entity_id, std::nullopt);
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::EntityThrown;
    event.payload = EntityThrownEvent{
        .entity_id = entity_id,
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
        .thrower_id = GetOrAssignReplicatedEntityId(state, gameplay_event.thrower_vid),
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueEntityDamagedReplicationEvent(State& state, const GameplayEntityDamaged& gameplay_event) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

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

    const NetEntityId entity_id = GetOrAssignReplicatedEntityId(state, gameplay_event.entity_vid);

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::EntityDamaged;
    event.payload = EntityDamagedEvent{
        .entity_id = entity_id,
        .source_entity_id = gameplay_event.source_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *gameplay_event.source_vid)
            : kInvalidNetEntityId,
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
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueEntityStatePatchedReplicationEvent(
    State& state,
    const GameplayEntityStatePatched& gameplay_event
) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    if (!ShouldReplicateInteractionSourceEvent(state, gameplay_event.source_vid)) {
        return;
    }

    const NetEntityId entity_id = GetOrAssignReplicatedEntityId(state, gameplay_event.entity_vid);

    NetEvent event;
    event.header = state.net_session.MakeLocalTransientEventHeader(state.frame);
    event.type = NetEventType::EntityStatePatched;
    event.payload = EntityStatePatchedEvent{
        .entity_id = entity_id,
        .source_entity_id = GetOrAssignReplicatedEntityId(state, gameplay_event.source_vid),
        .entity_a_id = GetReplicatedEntityLinkId(state, gameplay_event.entity_a_vid),
        .holding_id = GetReplicatedEntityLinkId(state, gameplay_event.holding_vid),
        .held_by_id = GetReplicatedEntityLinkId(state, gameplay_event.held_by_vid),
        .back_id = GetReplicatedEntityLinkId(state, gameplay_event.back_vid),
        .pos = gameplay_event.pos,
        .vel = gameplay_event.vel,
        .acc = gameplay_event.acc,
        .counter_a = gameplay_event.counter_a,
        .counter_b = gameplay_event.counter_b,
        .point_a = gameplay_event.point_a,
        .health = gameplay_event.health,
        .stun_timer = gameplay_event.stun_timer,
        .projectile_contact_timer = gameplay_event.projectile_contact_timer,
        .rotation = gameplay_event.rotation,
        .condition = gameplay_event.condition,
        .grounded = gameplay_event.grounded,
        .active = gameplay_event.active,
        .has_physics = gameplay_event.has_physics,
        .can_collide = gameplay_event.can_collide,
        .can_apply_projectile_contact = gameplay_event.can_apply_projectile_contact,
        .facing = gameplay_event.facing,
        .ai_state = gameplay_event.ai_state,
        .wanted = gameplay_event.wanted,
        .attachment_mode = gameplay_event.attachment_mode,
        .buyable_active = gameplay_event.buyable_active,
        .buyable_display_quantity = gameplay_event.buyable_display_quantity,
        .buyable_display_icon_animation_id = gameplay_event.buyable_display_icon_animation_id,
        .buyable_shop_owner_id = GetReplicatedEntityLinkId(state, gameplay_event.buyable_shop_owner_vid),
        .animate = gameplay_event.animate,
        .animation_id = gameplay_event.animation_id,
        .animation_frame = gameplay_event.animation_frame,
        .animation_time = gameplay_event.animation_time,
        .animation_speed = gameplay_event.animation_speed,
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueuePlayerStatePatchedReplicationEvent(
    State& state,
    const GameplayPlayerStatePatched& gameplay_event
) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    const Entity* const player = state.entity_manager.GetEntity(gameplay_event.player_vid);
    if (player == nullptr || !player->active) {
        return;
    }
    if (state.players.FindByEntityVid(player->vid) == nullptr) {
        return;
    }

    PlayerStatePatchedEvent payload{
        .player_entity_id = GetOrAssignReplicatedEntityId(state, player->vid),
        .health = player->health,
        .money = player->money,
        .wanted = static_cast<std::uint8_t>(player->wanted ? 1 : 0),
    };

    if (const EntityToolState* const tools = state.entity_tools.FindEntityToolState(player->vid)) {
        for (std::size_t i = 0; i < payload.tool_slots.size() && i < tools->slots.size(); ++i) {
            const ToolSlot& slot = tools->slots[i];
            payload.tool_slots[i] = PlayerStatePatchedToolSlot{
                .kind = slot.kind,
                .count = slot.count,
                .cooldown = slot.cooldown,
                .active = static_cast<std::uint8_t>(slot.active ? 1 : 0),
            };
        }
    }

    if (const EntityEffects* const effects = player->effects.get()) {
        payload.effect_count = static_cast<std::uint8_t>(
            std::min<std::size_t>(effects->count, payload.effects.size())
        );
        for (std::size_t i = 0; i < payload.effect_count; ++i) {
            const EffectInstance& effect = effects->effects[i];
            payload.effects[i] = PlayerStatePatchedEffect{
                .id = effect.id,
                .count = effect.count,
                .value = effect.value,
                .frames_remaining = effect.frames_remaining,
            };
        }
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::PlayerStatePatched;
    event.payload = payload;
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueRunStatePatchedReplicationEvent(State& state) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::RunStatePatched;
    event.payload = RunStatePatchedEvent{
        .sac_altar_favor = state.sac_altar_favor,
        .sac_altar_reward_tier = state.sac_altar_reward_tier,
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueRopeTilePlacedReplicationEvent(State& state, const GameplayRopeTilePlaced& gameplay_event) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::RopeTilePlaced;
    event.payload = RopeTilePlacedEvent{
        .tile_pos = gameplay_event.tile_pos,
        .source_entity_id = state.net_session.FindNetEntityId(gameplay_event.source_vid)
                                .value_or(kInvalidNetEntityId),
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueTileBrokenReplicationEvent(State& state, const GameplayTileBroken& gameplay_event) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::TileBroken;
    event.payload = TileBrokenEvent{
        .tile_pos = gameplay_event.tile_pos,
        .source_entity_id = kInvalidNetEntityId,
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueueActionRequestReplicationEvent(State& state, const GameplayActionRequested& gameplay_event) {
    if (state.net_session.role != NetRole::Peer) {
        return;
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalTransientEventHeader(state.frame);
    event.type = NetEventType::ActionRequest;
    event.payload = ActionRequestEvent{
        .kind = static_cast<NetActionKind>(gameplay_event.kind),
        .source_entity_id = gameplay_event.source_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *gameplay_event.source_vid)
            : kInvalidNetEntityId,
        .target_entity_id = gameplay_event.target_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *gameplay_event.target_vid)
            : kInvalidNetEntityId,
        .tile_pos = gameplay_event.tile_pos,
        .direction = gameplay_event.direction,
        .world_pos = gameplay_event.world_pos,
        .velocity = gameplay_event.velocity,
        .damage_type = gameplay_event.damage_type,
        .projectile_contact_damage_type = gameplay_event.projectile_contact_damage_type,
        .amount = gameplay_event.amount,
        .projectile_contact_damage_amount = gameplay_event.projectile_contact_damage_amount,
        .thrown_immunity_timer = gameplay_event.thrown_immunity_timer,
        .projectile_contact_duration = gameplay_event.projectile_contact_duration,
        .clear_velocity = gameplay_event.clear_velocity,
        .clear_acceleration = gameplay_event.clear_acceleration,
        .param_a = gameplay_event.param_a,
        .param_b = gameplay_event.param_b,
    };
    state.net_session.EnqueueNetEvent(event);
}

void EnqueuePresentationCommandReplicationEvent(State& state, const PresentationCommand& command) {
    if (state.net_session.role == NetRole::Offline) {
        return;
    }
    if (command.source_vid.has_value() &&
        !ShouldReplicateInteractionSourceEvent(state, *command.source_vid)) {
        return;
    }

    NetEvent event;
    event.header = state.net_session.MakeLocalEventHeader(state.frame);
    event.type = NetEventType::PresentationCommand;
    event.payload = PresentationCommandEvent{
        .kind = static_cast<std::uint16_t>(command.kind),
        .effect_id = static_cast<std::uint16_t>(command.effect_id),
        .audio_asset_id = command.audio_asset_id,
        .source_entity_id = command.source_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *command.source_vid)
            : kInvalidNetEntityId,
        .target_entity_id = command.target_vid.has_value()
            ? GetOrAssignReplicatedEntityId(state, *command.target_vid)
            : kInvalidNetEntityId,
        .source_pos = command.source_pos,
        .target_pos = command.target_pos,
        .direction_x = command.direction.x,
        .direction_y = command.direction.y,
        .param_a = command.param_a,
        .param_b = command.param_b,
        .param_c = command.param_c,
        .param_d = command.param_d,
    };
    state.net_session.EnqueueNetEvent(event);
}

} // namespace

void ReplicateGameplayEvent(State& state, const GameplayEvent& event) {
    switch (event.type) {
    case GameplayEventType::ActionRequested:
        EnqueueActionRequestReplicationEvent(state, event.action_requested);
        break;
    case GameplayEventType::EntitySpawned:
        EnqueueEntitySpawnedReplicationEvent(state, event.entity_spawned);
        break;
    case GameplayEventType::EntityDeactivated:
        EnqueueEntityDeactivatedReplicationEvent(state, event.entity_deactivated);
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
    case GameplayEventType::PlayerStatePatched:
        EnqueuePlayerStatePatchedReplicationEvent(state, event.player_state_patched);
        break;
    case GameplayEventType::RunStatePatched:
        EnqueueRunStatePatchedReplicationEvent(state);
        break;
    case GameplayEventType::TileBroken:
        EnqueueTileBrokenReplicationEvent(state, event.tile_broken);
        break;
    case GameplayEventType::RopeTilePlaced:
        EnqueueRopeTilePlacedReplicationEvent(state, event.rope_tile_placed);
        break;
    case GameplayEventType::PresentationCommand:
        EnqueuePresentationCommandReplicationEvent(state, event.presentation_command);
        break;
    case GameplayEventType::StageExitRequested:
    case GameplayEventType::StageTransitionRequested:
        break;
    }
}

} // namespace splonks::network
