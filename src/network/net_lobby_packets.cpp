#include "network/net_lobby_internal.hpp"

#include <variant>

namespace splonks::network {

namespace {

constexpr std::uint16_t kActionRequestFlagClearVelocity = 1U << 0U;
constexpr std::uint16_t kActionRequestFlagClearAcceleration = 1U << 1U;

std::uint16_t BuildActionRequestFlags(const ActionRequestEvent& payload) {
    std::uint16_t flags = 0;
    if (payload.clear_velocity) {
        flags |= kActionRequestFlagClearVelocity;
    }
    if (payload.clear_acceleration) {
        flags |= kActionRequestFlagClearAcceleration;
    }
    return flags;
}

bool IsReplicatedTileEvent(const NetEvent& event) {
    return (event.type == NetEventType::TileBroken &&
               std::holds_alternative<TileBrokenEvent>(event.payload)) ||
           (event.type == NetEventType::RopeTilePlaced &&
               std::holds_alternative<RopeTilePlacedEvent>(event.payload)) ||
           (event.type == NetEventType::TileChanged &&
               std::holds_alternative<TileChangedEvent>(event.payload));
}

bool IsReplicatedEntitySpawnedEvent(const NetEvent& event) {
    return event.type == NetEventType::EntitySpawned &&
           std::holds_alternative<EntitySpawnedEvent>(event.payload);
}

bool IsReplicatedEntityDamageEvent(const NetEvent& event) {
    return event.type == NetEventType::EntityDamaged &&
           std::holds_alternative<EntityDamagedEvent>(event.payload);
}

bool IsReplicatedActionRequestEvent(const NetEvent& event) {
    return event.type == NetEventType::ActionRequest &&
           std::holds_alternative<ActionRequestEvent>(event.payload);
}

bool IsReplicatedEntityCarryEvent(const NetEvent& event) {
    return (event.type == NetEventType::EntityHeld &&
               std::holds_alternative<EntityHeldEvent>(event.payload)) ||
           (event.type == NetEventType::EntityDropped &&
               std::holds_alternative<EntityDroppedEvent>(event.payload)) ||
           (event.type == NetEventType::EntityThrown &&
               std::holds_alternative<EntityThrownEvent>(event.payload));
}

bool IsReplicatedEntityLifecycleEvent(const NetEvent& event) {
    return event.type == NetEventType::EntityDeactivated &&
           std::holds_alternative<EntityIdEvent>(event.payload);
}

bool IsReplicatedPresentationCommandEvent(const NetEvent& event) {
    return event.type == NetEventType::PresentationCommand &&
           std::holds_alternative<PresentationCommandEvent>(event.payload);
}

IVec2 GetTileEventPos(const NetEvent& event) {
    if (const TileBrokenEvent* const payload = std::get_if<TileBrokenEvent>(&event.payload)) {
        return payload->tile_pos;
    }
    if (const RopeTilePlacedEvent* const payload = std::get_if<RopeTilePlacedEvent>(&event.payload)) {
        return payload->tile_pos;
    }
    if (const TileChangedEvent* const payload = std::get_if<TileChangedEvent>(&event.payload)) {
        return payload->tile_pos;
    }
    return IVec2::New(0, 0);
}

Tile GetTileEventTile(const NetEvent& event) {
    if (const TileChangedEvent* const payload = std::get_if<TileChangedEvent>(&event.payload)) {
        return payload->tile;
    }
    if (event.type == NetEventType::RopeTilePlaced) {
        return Tile::Rope;
    }
    return Tile::Air;
}

TileEventEntry MakeTileEventEntry(const NetEvent& event) {
    const IVec2 tile_pos = GetTileEventPos(event);
    return TileEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .event_type = static_cast<std::uint16_t>(event.type),
        .tile = static_cast<std::uint16_t>(GetTileEventTile(event)),
        .tile_x = static_cast<std::int32_t>(tile_pos.x),
        .tile_y = static_cast<std::int32_t>(tile_pos.y),
    };
}

EntitySpawnedEventEntry MakeEntitySpawnedEventEntry(const NetEvent& event) {
    const EntitySpawnedEvent* const payload = std::get_if<EntitySpawnedEvent>(&event.payload);
    return EntitySpawnedEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .entity_id = payload != nullptr ? payload->entity_id : kInvalidNetEntityId,
        .held_by_id = payload != nullptr ? payload->held_by_id : kInvalidNetEntityId,
        .entity_type = payload != nullptr ? static_cast<std::uint32_t>(payload->entity_type) : 0U,
        .pos_x = payload != nullptr ? payload->pos.x : 0.0F,
        .pos_y = payload != nullptr ? payload->pos.y : 0.0F,
        .vel_x = payload != nullptr ? payload->vel.x : 0.0F,
        .vel_y = payload != nullptr ? payload->vel.y : 0.0F,
        .acc_x = payload != nullptr ? payload->acc.x : 0.0F,
        .acc_y = payload != nullptr ? payload->acc.y : 0.0F,
        .counter_a = payload != nullptr ? payload->counter_a : 0.0F,
        .counter_b = payload != nullptr ? payload->counter_b : 0.0F,
        .use_pressed = static_cast<std::uint8_t>(
            payload != nullptr && payload->use_pressed ? 1 : 0
        ),
        .animate = payload != nullptr ? payload->animate : static_cast<std::uint8_t>(0),
        .animation_id = payload != nullptr ? payload->animation_id : kInvalidFrameDataId,
        .animation_frame = payload != nullptr ? payload->animation_frame : static_cast<std::uint16_t>(0),
        .animation_time = payload != nullptr ? payload->animation_time : 0.0F,
        .animation_speed = payload != nullptr ? payload->animation_speed : 1.0F,
    };
}

EntityDamageEventEntry MakeEntityDamageEventEntry(const NetEvent& event) {
    const EntityDamagedEvent* const payload = std::get_if<EntityDamagedEvent>(&event.payload);
    return EntityDamageEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .entity_id = payload != nullptr ? payload->entity_id : kInvalidNetEntityId,
        .source_entity_id = payload != nullptr ? payload->source_entity_id : kInvalidNetEntityId,
        .amount = payload != nullptr ? payload->amount : 0U,
        .remaining_health = payload != nullptr ? payload->remaining_health : 0U,
        .pos_x = payload != nullptr ? payload->pos.x : 0.0F,
        .pos_y = payload != nullptr ? payload->pos.y : 0.0F,
        .vel_x = payload != nullptr ? payload->vel.x : 0.0F,
        .vel_y = payload != nullptr ? payload->vel.y : 0.0F,
        .acc_x = payload != nullptr ? payload->acc.x : 0.0F,
        .acc_y = payload != nullptr ? payload->acc.y : 0.0F,
        .stun_timer = payload != nullptr ? payload->stun_timer : 0U,
        .projectile_contact_timer = payload != nullptr ? payload->projectile_contact_timer : 0U,
        .damage_type = payload != nullptr
            ? static_cast<std::uint16_t>(payload->damage_type)
            : static_cast<std::uint16_t>(0),
        .condition = payload != nullptr ? payload->condition : static_cast<std::uint8_t>(0),
        .grounded = payload != nullptr ? payload->grounded : static_cast<std::uint8_t>(0),
        .animate = payload != nullptr ? payload->animate : static_cast<std::uint8_t>(0),
        .animation_id = payload != nullptr ? payload->animation_id : kInvalidFrameDataId,
        .animation_frame = payload != nullptr ? payload->animation_frame : static_cast<std::uint16_t>(0),
        .animation_time = payload != nullptr ? payload->animation_time : 0.0F,
        .animation_speed = payload != nullptr ? payload->animation_speed : 1.0F,
    };
}

EntityStateEventEntry MakeEntityStateEventEntry(const NetEvent& event) {
    const EntityStatePatchedEvent* const payload = std::get_if<EntityStatePatchedEvent>(&event.payload);
    return EntityStateEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .entity_id = payload != nullptr ? payload->entity_id : kInvalidNetEntityId,
        .source_entity_id = payload != nullptr ? payload->source_entity_id : kInvalidNetEntityId,
        .entity_a_id = payload != nullptr ? payload->entity_a_id : kInvalidNetEntityId,
        .pos_x = payload != nullptr ? payload->pos.x : 0.0F,
        .pos_y = payload != nullptr ? payload->pos.y : 0.0F,
        .vel_x = payload != nullptr ? payload->vel.x : 0.0F,
        .vel_y = payload != nullptr ? payload->vel.y : 0.0F,
        .acc_x = payload != nullptr ? payload->acc.x : 0.0F,
        .acc_y = payload != nullptr ? payload->acc.y : 0.0F,
        .point_a_x = payload != nullptr ? payload->point_a.x : 0,
        .point_a_y = payload != nullptr ? payload->point_a.y : 0,
        .health = payload != nullptr ? payload->health : 0U,
        .stun_timer = payload != nullptr ? payload->stun_timer : 0U,
        .projectile_contact_timer = payload != nullptr ? payload->projectile_contact_timer : 0U,
        .rotation = payload != nullptr ? payload->rotation : 0.0F,
        .condition = payload != nullptr ? payload->condition : static_cast<std::uint8_t>(0),
        .grounded = payload != nullptr ? payload->grounded : static_cast<std::uint8_t>(0),
        .active = payload != nullptr ? payload->active : static_cast<std::uint8_t>(0),
        .has_physics = payload != nullptr ? payload->has_physics : static_cast<std::uint8_t>(1),
        .can_collide = payload != nullptr ? payload->can_collide : static_cast<std::uint8_t>(1),
        .can_apply_projectile_contact =
            payload != nullptr ? payload->can_apply_projectile_contact : static_cast<std::uint8_t>(1),
        .facing = payload != nullptr ? payload->facing : static_cast<std::uint8_t>(0),
        .animate = payload != nullptr ? payload->animate : static_cast<std::uint8_t>(0),
        .animation_id = payload != nullptr ? payload->animation_id : kInvalidFrameDataId,
        .animation_frame = payload != nullptr ? payload->animation_frame : static_cast<std::uint16_t>(0),
        .animation_time = payload != nullptr ? payload->animation_time : 0.0F,
        .animation_speed = payload != nullptr ? payload->animation_speed : 1.0F,
    };
}

EntityCarryEventEntry MakeEntityCarryEventEntry(const NetEvent& event) {
    EntityCarryEventEntry entry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .event_type = static_cast<std::uint16_t>(event.type),
    };

    if (const EntityHeldEvent* const held_payload = std::get_if<EntityHeldEvent>(&event.payload)) {
        entry.entity_id = held_payload->held_id;
        entry.holder_id = held_payload->holder_id;
    } else if (const EntityDroppedEvent* const dropped_payload = std::get_if<EntityDroppedEvent>(&event.payload)) {
        entry.entity_id = dropped_payload->entity_id;
        entry.pos_x = dropped_payload->pos.x;
        entry.pos_y = dropped_payload->pos.y;
        entry.vel_x = dropped_payload->vel.x;
        entry.vel_y = dropped_payload->vel.y;
    } else if (const EntityThrownEvent* const thrown_payload = std::get_if<EntityThrownEvent>(&event.payload)) {
        entry.entity_id = thrown_payload->entity_id;
        entry.thrower_id = thrown_payload->thrower_id;
        entry.pos_x = thrown_payload->pos.x;
        entry.pos_y = thrown_payload->pos.y;
        entry.vel_x = thrown_payload->vel.x;
        entry.vel_y = thrown_payload->vel.y;
    }

    return entry;
}

EntityLifecycleEventEntry MakeEntityLifecycleEventEntry(const NetEvent& event) {
    EntityLifecycleEventEntry entry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .event_type = static_cast<std::uint16_t>(event.type),
    };

    if (const EntityIdEvent* const id_payload = std::get_if<EntityIdEvent>(&event.payload)) {
        entry.entity_id = id_payload->entity_id;
    }

    return entry;
}

PresentationCommandEventEntry MakePresentationCommandEventEntry(const NetEvent& event) {
    const PresentationCommandEvent* const payload = std::get_if<PresentationCommandEvent>(&event.payload);
    return PresentationCommandEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .kind = payload != nullptr ? payload->kind : static_cast<std::uint16_t>(0),
        .effect_id = payload != nullptr ? payload->effect_id : static_cast<std::uint16_t>(0),
        .audio_asset_id = payload != nullptr ? payload->audio_asset_id : 0,
        .source_entity_id = payload != nullptr ? payload->source_entity_id : kInvalidNetEntityId,
        .target_entity_id = payload != nullptr ? payload->target_entity_id : kInvalidNetEntityId,
        .source_x = payload != nullptr ? payload->source_pos.x : 0.0F,
        .source_y = payload != nullptr ? payload->source_pos.y : 0.0F,
        .target_x = payload != nullptr ? payload->target_pos.x : 0.0F,
        .target_y = payload != nullptr ? payload->target_pos.y : 0.0F,
        .direction_x = payload != nullptr ? payload->direction_x : 1,
        .direction_y = payload != nullptr ? payload->direction_y : 0,
        .param_a = payload != nullptr ? payload->param_a : 0.0F,
        .param_b = payload != nullptr ? payload->param_b : 0.0F,
        .param_c = payload != nullptr ? payload->param_c : 0.0F,
        .param_d = payload != nullptr ? payload->param_d : 0.0F,
    };
}

ActionRequestEventEntry MakeActionRequestEventEntry(const NetEvent& event) {
    const ActionRequestEvent* const payload = std::get_if<ActionRequestEvent>(&event.payload);
    return ActionRequestEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .action_kind = payload != nullptr
            ? static_cast<std::uint16_t>(payload->kind)
            : static_cast<std::uint16_t>(NetActionKind::None),
        .damage_type = payload != nullptr
            ? static_cast<std::uint16_t>(payload->damage_type)
            : static_cast<std::uint16_t>(DamageType::Attack),
        .projectile_contact_damage_type = payload != nullptr
            ? static_cast<std::uint16_t>(payload->projectile_contact_damage_type)
            : static_cast<std::uint16_t>(DamageType::Attack),
        .flags = payload != nullptr ? BuildActionRequestFlags(*payload) : static_cast<std::uint16_t>(0),
        .source_entity_id = payload != nullptr ? payload->source_entity_id : kInvalidNetEntityId,
        .target_entity_id = payload != nullptr ? payload->target_entity_id : kInvalidNetEntityId,
        .tile_x = payload != nullptr ? payload->tile_pos.x : 0,
        .tile_y = payload != nullptr ? payload->tile_pos.y : 0,
        .direction_x = payload != nullptr ? payload->direction.x : 0,
        .direction_y = payload != nullptr ? payload->direction.y : 0,
        .world_x = payload != nullptr ? payload->world_pos.x : 0.0F,
        .world_y = payload != nullptr ? payload->world_pos.y : 0.0F,
        .velocity_x = payload != nullptr ? payload->velocity.x : 0.0F,
        .velocity_y = payload != nullptr ? payload->velocity.y : 0.0F,
        .amount = payload != nullptr ? payload->amount : 0U,
        .projectile_contact_damage_amount =
            payload != nullptr ? payload->projectile_contact_damage_amount : 0U,
        .thrown_immunity_timer = payload != nullptr ? payload->thrown_immunity_timer : 0U,
        .projectile_contact_duration =
            payload != nullptr ? payload->projectile_contact_duration : 0U,
        .param_a = payload != nullptr ? payload->param_a : 0U,
        .param_b = payload != nullptr ? payload->param_b : 0U,
    };
}

} // namespace

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
) {
    std::string error;
    if (!transport.socket.Send(endpoint, encoded.bytes.data(), encoded.size, &error)) {
        transport.last_error = error;
    }
}

bool IsReplicatedEntityStateEvent(const NetEvent& event) {
    return event.type == NetEventType::EntityStatePatched &&
           std::holds_alternative<EntityStatePatchedEvent>(event.payload);
}

bool IsOneShotActionRequestEvent(const NetEvent& event) {
    if (event.header.coordinator_order != 0) {
        return false;
    }
    const ActionRequestEvent* const action = std::get_if<ActionRequestEvent>(&event.payload);
    return action != nullptr &&
           (action->kind == NetActionKind::UseTool ||
            action->kind == NetActionKind::PickupEntity ||
            action->kind == NetActionKind::DropEntity ||
            action->kind == NetActionKind::ThrowEntity ||
            action->kind == NetActionKind::UseHeldEntity ||
            action->kind == NetActionKind::UseBackEntity ||
            action->kind == NetActionKind::InteractEntity ||
            action->kind == NetActionKind::PushEntity);
}

NetEvent MakeTileEvent(const TileEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = static_cast<NetEventType>(entry.event_type);
    const IVec2 tile_pos = IVec2::New(entry.tile_x, entry.tile_y);
    switch (event.type) {
    case NetEventType::TileBroken:
        event.payload = TileBrokenEvent{
            .tile_pos = tile_pos,
            .source_entity_id = kInvalidNetEntityId,
        };
        break;
    case NetEventType::RopeTilePlaced:
        event.payload = RopeTilePlacedEvent{
            .tile_pos = tile_pos,
            .source_entity_id = kInvalidNetEntityId,
        };
        break;
    case NetEventType::TileChanged:
        event.payload = TileChangedEvent{
            .tile_pos = tile_pos,
            .tile = static_cast<Tile>(entry.tile),
            .rotation = kTileRotation0,
        };
        break;
    default:
        event.type = NetEventType::None;
        event.payload = std::monostate{};
        break;
    }
    return event;
}

NetEvent MakeEntitySpawnedEvent(const EntitySpawnedEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::EntitySpawned;
    event.payload = EntitySpawnedEvent{
        .entity_id = entry.entity_id,
        .entity_type = static_cast<EntityType>(entry.entity_type),
        .held_by_id = entry.held_by_id,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .acc = Vec2::New(entry.acc_x, entry.acc_y),
        .owner = NetEntityOwner::Coordinator(),
        .counter_a = entry.counter_a,
        .counter_b = entry.counter_b,
        .use_pressed = entry.use_pressed != 0,
        .animate = entry.animate,
        .animation_id = entry.animation_id,
        .animation_frame = entry.animation_frame,
        .animation_time = entry.animation_time,
        .animation_speed = entry.animation_speed,
    };
    return event;
}

NetEvent MakeEntityDamageEvent(const EntityDamageEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::EntityDamaged;
    event.payload = EntityDamagedEvent{
        .entity_id = entry.entity_id,
        .source_entity_id = entry.source_entity_id,
        .amount = entry.amount,
        .remaining_health = entry.remaining_health,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .acc = Vec2::New(entry.acc_x, entry.acc_y),
        .stun_timer = entry.stun_timer,
        .projectile_contact_timer = entry.projectile_contact_timer,
        .condition = entry.condition,
        .grounded = entry.grounded,
        .animate = entry.animate,
        .animation_id = entry.animation_id,
        .animation_frame = entry.animation_frame,
        .animation_time = entry.animation_time,
        .animation_speed = entry.animation_speed,
        .damage_type = static_cast<DamageType>(entry.damage_type),
    };
    return event;
}

NetEvent MakeEntityStateEvent(const EntityStateEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::EntityStatePatched;
    event.payload = EntityStatePatchedEvent{
        .entity_id = entry.entity_id,
        .source_entity_id = entry.source_entity_id,
        .entity_a_id = entry.entity_a_id,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .acc = Vec2::New(entry.acc_x, entry.acc_y),
        .point_a = IVec2::New(entry.point_a_x, entry.point_a_y),
        .health = entry.health,
        .stun_timer = entry.stun_timer,
        .projectile_contact_timer = entry.projectile_contact_timer,
        .rotation = entry.rotation,
        .condition = entry.condition,
        .grounded = entry.grounded,
        .active = entry.active,
        .has_physics = entry.has_physics,
        .can_collide = entry.can_collide,
        .can_apply_projectile_contact = entry.can_apply_projectile_contact,
        .facing = entry.facing,
        .animate = entry.animate,
        .animation_id = entry.animation_id,
        .animation_frame = entry.animation_frame,
        .animation_time = entry.animation_time,
        .animation_speed = entry.animation_speed,
    };
    return event;
}

NetEvent MakeEntityCarryEvent(const EntityCarryEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = static_cast<NetEventType>(entry.event_type);
    switch (event.type) {
    case NetEventType::EntityHeld:
        event.payload = EntityHeldEvent{
            .holder_id = entry.holder_id,
            .held_id = entry.entity_id,
        };
        break;
    case NetEventType::EntityDropped:
        event.payload = EntityDroppedEvent{
            .entity_id = entry.entity_id,
            .pos = Vec2::New(entry.pos_x, entry.pos_y),
            .vel = Vec2::New(entry.vel_x, entry.vel_y),
        };
        break;
    case NetEventType::EntityThrown:
        event.payload = EntityThrownEvent{
            .entity_id = entry.entity_id,
            .pos = Vec2::New(entry.pos_x, entry.pos_y),
            .vel = Vec2::New(entry.vel_x, entry.vel_y),
            .thrower_id = entry.thrower_id,
        };
        break;
    default:
        event.type = NetEventType::None;
        event.payload = std::monostate{};
        break;
    }
    return event;
}

NetEvent MakeEntityLifecycleEvent(const EntityLifecycleEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = static_cast<NetEventType>(entry.event_type);
    switch (event.type) {
    case NetEventType::EntityDeactivated:
        event.payload = EntityIdEvent{
            .entity_id = entry.entity_id,
        };
        break;
    default:
        event.type = NetEventType::None;
        event.payload = std::monostate{};
        break;
    }
    return event;
}

NetEvent MakePresentationCommandEvent(const PresentationCommandEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::PresentationCommand;
    event.payload = PresentationCommandEvent{
        .kind = entry.kind,
        .effect_id = entry.effect_id,
        .audio_asset_id = entry.audio_asset_id,
        .source_entity_id = entry.source_entity_id,
        .target_entity_id = entry.target_entity_id,
        .source_pos = Vec2::New(entry.source_x, entry.source_y),
        .target_pos = Vec2::New(entry.target_x, entry.target_y),
        .direction_x = entry.direction_x,
        .direction_y = entry.direction_y,
        .param_a = entry.param_a,
        .param_b = entry.param_b,
        .param_c = entry.param_c,
        .param_d = entry.param_d,
    };
    return event;
}

NetEvent MakeActionRequestEvent(const ActionRequestEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = 0,
    };
    event.type = NetEventType::ActionRequest;
    event.payload = ActionRequestEvent{
        .kind = static_cast<NetActionKind>(entry.action_kind),
        .source_entity_id = entry.source_entity_id,
        .target_entity_id = entry.target_entity_id,
        .tile_pos = IVec2::New(entry.tile_x, entry.tile_y),
        .direction = IVec2::New(entry.direction_x, entry.direction_y),
        .world_pos = Vec2::New(entry.world_x, entry.world_y),
        .velocity = Vec2::New(entry.velocity_x, entry.velocity_y),
        .damage_type = static_cast<DamageType>(entry.damage_type),
        .projectile_contact_damage_type = static_cast<DamageType>(entry.projectile_contact_damage_type),
        .amount = entry.amount,
        .projectile_contact_damage_amount = entry.projectile_contact_damage_amount,
        .thrown_immunity_timer = entry.thrown_immunity_timer,
        .projectile_contact_duration = entry.projectile_contact_duration,
        .clear_velocity = (entry.flags & kActionRequestFlagClearVelocity) != 0,
        .clear_acceleration = (entry.flags & kActionRequestFlagClearAcceleration) != 0,
        .param_a = entry.param_a,
        .param_b = entry.param_b,
    };
    return event;
}

void SendTileEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    TileEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedTileEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeTileEvents(packet));
            packet = TileEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeTileEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeTileEvents(packet));
    }
}

void SendEntitySpawnedEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntitySpawnedEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntitySpawnedEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntitySpawnedEvents(packet));
            packet = EntitySpawnedEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntitySpawnedEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntitySpawnedEvents(packet));
    }
}

void SendEntityDamageEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityDamageEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityDamageEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityDamageEvents(packet));
            packet = EntityDamageEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityDamageEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityDamageEvents(packet));
    }
}

void SendEntityStateEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityStateEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityStateEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityStateEvents(packet));
            packet = EntityStateEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityStateEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityStateEvents(packet));
    }
}

void SendEntityCarryEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityCarryEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityCarryEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityCarryEvents(packet));
            packet = EntityCarryEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityCarryEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityCarryEvents(packet));
    }
}

void SendEntityLifecycleEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityLifecycleEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityLifecycleEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityLifecycleEvents(packet));
            packet = EntityLifecycleEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityLifecycleEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityLifecycleEvents(packet));
    }
}

void SendPresentationCommandEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    PresentationCommandEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedPresentationCommandEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodePresentationCommandEvents(packet));
            packet = PresentationCommandEventsPacket{};
        }
        packet.events[packet.event_count++] = MakePresentationCommandEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodePresentationCommandEvents(packet));
    }
}

void SendActionRequestEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    ActionRequestEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedActionRequestEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeActionRequestEvents(packet));
            packet = ActionRequestEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeActionRequestEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeActionRequestEvents(packet));
    }
}

} // namespace splonks::network
