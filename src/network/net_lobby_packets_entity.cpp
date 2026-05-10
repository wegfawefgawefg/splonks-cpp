#include "network/net_lobby_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <variant>

namespace splonks::network {

bool IsReplicatedEntitySpawnedMessage(const NetMessage& message) {
    return message.type == NetMessageType::EntitySpawned &&
           std::holds_alternative<EntitySpawnedMessage>(message.payload);
}

bool IsReplicatedEntityDamageMessage(const NetMessage& message) {
    return message.type == NetMessageType::EntityDamaged &&
           std::holds_alternative<EntityDamagedMessage>(message.payload);
}

bool IsReplicatedEntityCarryMessage(const NetMessage& message) {
    return (message.type == NetMessageType::EntityHeld &&
               std::holds_alternative<EntityHeldMessage>(message.payload)) ||
           (message.type == NetMessageType::EntityDropped &&
               std::holds_alternative<EntityDroppedMessage>(message.payload)) ||
           (message.type == NetMessageType::EntityThrown &&
               std::holds_alternative<EntityThrownMessage>(message.payload));
}

bool IsReplicatedEntityLifecycleMessage(const NetMessage& message) {
    return message.type == NetMessageType::EntityDeactivated &&
           std::holds_alternative<EntityIdMessage>(message.payload);
}


EntitySpawnedMessageEntry MakeEntitySpawnedMessageEntry(const NetMessage& message) {
    const EntitySpawnedMessage* const payload = std::get_if<EntitySpawnedMessage>(&message.payload);
    EntitySpawnedMessageEntry entry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .entity_id = payload != nullptr ? payload->entity_id : kInvalidNetEntityId,
        .held_by_id = payload != nullptr ? payload->held_by_id : kInvalidNetEntityId,
        .entity_type = payload != nullptr ? static_cast<std::uint32_t>(payload->entity_type) : 0U,
        .pos_x = payload != nullptr ? payload->pos.x : 0.0F,
        .pos_y = payload != nullptr ? payload->pos.y : 0.0F,
        .vel_x = payload != nullptr ? payload->vel.x : 0.0F,
        .vel_y = payload != nullptr ? payload->vel.y : 0.0F,
        .acc_x = payload != nullptr ? payload->acc.x : 0.0F,
        .acc_y = payload != nullptr ? payload->acc.y : 0.0F,
        .size_x = payload != nullptr ? payload->size.x : 0.0F,
        .size_y = payload != nullptr ? payload->size.y : 0.0F,
        .counter_a = payload != nullptr ? payload->counter_a : 0.0F,
        .counter_b = payload != nullptr ? payload->counter_b : 0.0F,
        .movement_flags = payload != nullptr ? payload->movement_flags : 0U,
        .use_pressed = static_cast<std::uint8_t>(
            payload != nullptr && payload->use_pressed ? 1 : 0
        ),
        .animate = payload != nullptr ? payload->animate : static_cast<std::uint8_t>(0),
        .animation_loop = payload != nullptr ? payload->animation_loop : static_cast<std::uint8_t>(1),
        .animation_finished =
            payload != nullptr ? payload->animation_finished : static_cast<std::uint8_t>(0),
        .animation_id = payload != nullptr ? payload->animation_id : kInvalidFrameDataId,
        .animation_frame = payload != nullptr ? payload->animation_frame : static_cast<std::uint16_t>(0),
        .animation_time = payload != nullptr ? payload->animation_time : 0.0F,
        .animation_speed = payload != nullptr ? payload->animation_speed : 1.0F,
    };
    if (payload != nullptr) {
        entry.effect_count = static_cast<std::uint8_t>(
            std::min<std::size_t>(payload->effect_count, entry.effects.size())
        );
        for (std::size_t i = 0; i < entry.effect_count; ++i) {
            const EntityReplicatedEffect& effect = payload->effects[i];
            entry.effects[i] = EntityEffectEntry{
                .id = static_cast<std::uint16_t>(effect.id),
                .count = effect.count,
                .value = effect.value,
                .frames_remaining = effect.frames_remaining,
            };
        }
    }
    return entry;
}

EntityDamageMessageEntry MakeEntityDamageMessageEntry(const NetMessage& message) {
    const EntityDamagedMessage* const payload = std::get_if<EntityDamagedMessage>(&message.payload);
    return EntityDamageMessageEntry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
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
        .fall_timer = payload != nullptr ? payload->fall_timer : 0U,
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

EntityStateMessageEntry MakeEntityStateMessageEntry(const NetMessage& message) {
    const EntityStatePatchedMessage* const payload = std::get_if<EntityStatePatchedMessage>(&message.payload);
    EntityStateMessageEntry entry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .entity_id = payload != nullptr ? payload->entity_id : kInvalidNetEntityId,
        .source_entity_id = payload != nullptr ? payload->source_entity_id : kInvalidNetEntityId,
        .entity_a_id = payload != nullptr ? payload->entity_a_id : kInvalidNetEntityId,
        .entity_b_id = payload != nullptr ? payload->entity_b_id : kInvalidNetEntityId,
        .entity_c_id = payload != nullptr ? payload->entity_c_id : kInvalidNetEntityId,
        .entity_d_id = payload != nullptr ? payload->entity_d_id : kInvalidNetEntityId,
        .holding_id = payload != nullptr ? payload->holding_id : kInvalidNetEntityId,
        .held_by_id = payload != nullptr ? payload->held_by_id : kInvalidNetEntityId,
        .back_id = payload != nullptr ? payload->back_id : kInvalidNetEntityId,
        .pos_x = payload != nullptr ? payload->pos.x : 0.0F,
        .pos_y = payload != nullptr ? payload->pos.y : 0.0F,
        .vel_x = payload != nullptr ? payload->vel.x : 0.0F,
        .vel_y = payload != nullptr ? payload->vel.y : 0.0F,
        .acc_x = payload != nullptr ? payload->acc.x : 0.0F,
        .acc_y = payload != nullptr ? payload->acc.y : 0.0F,
        .size_x = payload != nullptr ? payload->size.x : 0.0F,
        .size_y = payload != nullptr ? payload->size.y : 0.0F,
        .counter_a = payload != nullptr ? payload->counter_a : 0.0F,
        .counter_b = payload != nullptr ? payload->counter_b : 0.0F,
        .counter_c = payload != nullptr ? payload->counter_c : 0.0F,
        .counter_d = payload != nullptr ? payload->counter_d : 0.0F,
        .threshold_a = payload != nullptr ? payload->threshold_a : 0.0F,
        .threshold_b = payload != nullptr ? payload->threshold_b : 0.0F,
        .point_a_x = payload != nullptr ? payload->point_a.x : 0,
        .point_a_y = payload != nullptr ? payload->point_a.y : 0,
        .point_b_x = payload != nullptr ? payload->point_b.x : 0,
        .point_b_y = payload != nullptr ? payload->point_b.y : 0,
        .point_c_x = payload != nullptr ? payload->point_c.x : 0,
        .point_c_y = payload != nullptr ? payload->point_c.y : 0,
        .point_d_x = payload != nullptr ? payload->point_d.x : 0,
        .point_d_y = payload != nullptr ? payload->point_d.y : 0,
        .health = payload != nullptr ? payload->health : 0U,
        .coyote_time = payload != nullptr ? payload->coyote_time : 0U,
        .fall_timer = payload != nullptr ? payload->fall_timer : 0U,
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
        .damage_vulnerability =
            payload != nullptr ? payload->damage_vulnerability : static_cast<std::uint8_t>(0),
        .facing = payload != nullptr ? payload->facing : static_cast<std::uint8_t>(0),
        .ai_state = payload != nullptr ? payload->ai_state : static_cast<std::uint8_t>(0),
        .wanted = payload != nullptr ? payload->wanted : static_cast<std::uint8_t>(0),
        .holding = payload != nullptr ? payload->holding : static_cast<std::uint8_t>(0),
        .render_enabled =
            payload != nullptr ? payload->render_enabled : static_cast<std::uint8_t>(1),
        .attachment_mode = payload != nullptr ? payload->attachment_mode : static_cast<std::uint8_t>(0),
        .draw_layer = payload != nullptr ? payload->draw_layer : static_cast<std::uint8_t>(0),
        .movement_flags = payload != nullptr ? payload->movement_flags : 0U,
        .money = payload != nullptr ? payload->money : 0U,
        .stage_exit_id = payload != nullptr ? payload->stage_exit_id : -1,
        .runtime_flags = payload != nullptr ? payload->runtime_flags : 0U,
        .buyable_active = payload != nullptr ? payload->buyable_active : static_cast<std::uint8_t>(0),
        .buyable_display_quantity = payload != nullptr ? payload->buyable_display_quantity : 0U,
        .buyable_display_icon_animation_id =
            payload != nullptr ? payload->buyable_display_icon_animation_id : kInvalidFrameDataId,
        .buyable_shop_owner_id =
            payload != nullptr ? payload->buyable_shop_owner_id : kInvalidNetEntityId,
        .animate = payload != nullptr ? payload->animate : static_cast<std::uint8_t>(0),
        .animation_loop = payload != nullptr ? payload->animation_loop : static_cast<std::uint8_t>(1),
        .animation_finished =
            payload != nullptr ? payload->animation_finished : static_cast<std::uint8_t>(0),
        .animation_id = payload != nullptr ? payload->animation_id : kInvalidFrameDataId,
        .animation_frame = payload != nullptr ? payload->animation_frame : static_cast<std::uint16_t>(0),
        .animation_time = payload != nullptr ? payload->animation_time : 0.0F,
        .animation_speed = payload != nullptr ? payload->animation_speed : 1.0F,
    };
    if (payload != nullptr) {
        entry.effect_count = static_cast<std::uint8_t>(
            std::min<std::size_t>(payload->effect_count, entry.effects.size())
        );
        for (std::size_t i = 0; i < entry.effect_count; ++i) {
            const EntityReplicatedEffect& effect = payload->effects[i];
            entry.effects[i] = EntityEffectEntry{
                .id = static_cast<std::uint16_t>(effect.id),
                .count = effect.count,
                .value = effect.value,
                .frames_remaining = effect.frames_remaining,
            };
        }
    }
    return entry;
}

EntityCarryMessageEntry MakeEntityCarryMessageEntry(const NetMessage& message) {
    EntityCarryMessageEntry entry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .message_type = static_cast<std::uint16_t>(message.type),
    };

    if (const EntityHeldMessage* const held_payload = std::get_if<EntityHeldMessage>(&message.payload)) {
        entry.entity_id = held_payload->held_id;
        entry.holder_id = held_payload->holder_id;
        entry.attachment_mode = static_cast<std::uint16_t>(held_payload->attachment_mode);
    } else if (const EntityDroppedMessage* const dropped_payload = std::get_if<EntityDroppedMessage>(&message.payload)) {
        entry.entity_id = dropped_payload->entity_id;
        entry.holder_id = dropped_payload->dropped_by_id;
        entry.pos_x = dropped_payload->pos.x;
        entry.pos_y = dropped_payload->pos.y;
        entry.vel_x = dropped_payload->vel.x;
        entry.vel_y = dropped_payload->vel.y;
    } else if (const EntityThrownMessage* const thrown_payload = std::get_if<EntityThrownMessage>(&message.payload)) {
        entry.entity_id = thrown_payload->entity_id;
        entry.thrower_id = thrown_payload->thrower_id;
        entry.pos_x = thrown_payload->pos.x;
        entry.pos_y = thrown_payload->pos.y;
        entry.vel_x = thrown_payload->vel.x;
        entry.vel_y = thrown_payload->vel.y;
    }

    return entry;
}

EntityLifecycleMessageEntry MakeEntityLifecycleMessageEntry(const NetMessage& message) {
    EntityLifecycleMessageEntry entry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .message_type = static_cast<std::uint16_t>(message.type),
    };

    if (const EntityIdMessage* const id_payload = std::get_if<EntityIdMessage>(&message.payload)) {
        entry.entity_id = id_payload->entity_id;
    }

    return entry;
}


bool IsReplicatedEntityStateMessage(const NetMessage& message) {
    return message.type == NetMessageType::EntityStatePatched &&
           std::holds_alternative<EntityStatePatchedMessage>(message.payload);
}

NetMessage MakeEntitySpawnedMessage(const EntitySpawnedMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = NetMessageType::EntitySpawned;
    EntitySpawnedMessage payload{
        .entity_id = entry.entity_id,
        .entity_type = static_cast<EntityType>(entry.entity_type),
        .held_by_id = entry.held_by_id,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .acc = Vec2::New(entry.acc_x, entry.acc_y),
        .size = Vec2::New(entry.size_x, entry.size_y),
        .owner = NetEntityOwner::Coordinator(),
        .counter_a = entry.counter_a,
        .counter_b = entry.counter_b,
        .movement_flags = entry.movement_flags,
        .use_pressed = entry.use_pressed != 0,
        .animate = entry.animate,
        .animation_loop = entry.animation_loop,
        .animation_finished = entry.animation_finished,
        .animation_id = entry.animation_id,
        .animation_frame = entry.animation_frame,
        .animation_time = entry.animation_time,
        .animation_speed = entry.animation_speed,
    };
    payload.effect_count = static_cast<std::uint8_t>(
        std::min<std::size_t>(entry.effect_count, payload.effects.size())
    );
    for (std::size_t i = 0; i < payload.effect_count; ++i) {
        const EntityEffectEntry& effect = entry.effects[i];
        payload.effects[i] = EntityReplicatedEffect{
            .id = static_cast<EffectId>(effect.id),
            .count = effect.count,
            .value = effect.value,
            .frames_remaining = effect.frames_remaining,
        };
    }
    message.payload = payload;
    return message;
}

NetMessage MakeEntityDamageMessage(const EntityDamageMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = NetMessageType::EntityDamaged;
    message.payload = EntityDamagedMessage{
        .entity_id = entry.entity_id,
        .source_entity_id = entry.source_entity_id,
        .amount = entry.amount,
        .remaining_health = entry.remaining_health,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .acc = Vec2::New(entry.acc_x, entry.acc_y),
        .fall_timer = entry.fall_timer,
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
    return message;
}

NetMessage MakeEntityStateMessage(const EntityStateMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = NetMessageType::EntityStatePatched;
    EntityStatePatchedMessage payload{
        .entity_id = entry.entity_id,
        .source_entity_id = entry.source_entity_id,
        .entity_a_id = entry.entity_a_id,
        .entity_b_id = entry.entity_b_id,
        .entity_c_id = entry.entity_c_id,
        .entity_d_id = entry.entity_d_id,
        .holding_id = entry.holding_id,
        .held_by_id = entry.held_by_id,
        .back_id = entry.back_id,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .acc = Vec2::New(entry.acc_x, entry.acc_y),
        .size = Vec2::New(entry.size_x, entry.size_y),
        .counter_a = entry.counter_a,
        .counter_b = entry.counter_b,
        .counter_c = entry.counter_c,
        .counter_d = entry.counter_d,
        .threshold_a = entry.threshold_a,
        .threshold_b = entry.threshold_b,
        .point_a = IVec2::New(entry.point_a_x, entry.point_a_y),
        .point_b = IVec2::New(entry.point_b_x, entry.point_b_y),
        .point_c = IVec2::New(entry.point_c_x, entry.point_c_y),
        .point_d = IVec2::New(entry.point_d_x, entry.point_d_y),
        .health = entry.health,
        .coyote_time = entry.coyote_time,
        .fall_timer = entry.fall_timer,
        .stun_timer = entry.stun_timer,
        .projectile_contact_timer = entry.projectile_contact_timer,
        .rotation = entry.rotation,
        .condition = entry.condition,
        .grounded = entry.grounded,
        .active = entry.active,
        .has_physics = entry.has_physics,
        .can_collide = entry.can_collide,
        .can_apply_projectile_contact = entry.can_apply_projectile_contact,
        .damage_vulnerability = entry.damage_vulnerability,
        .facing = entry.facing,
        .ai_state = entry.ai_state,
        .wanted = entry.wanted,
        .holding = entry.holding,
        .render_enabled = entry.render_enabled,
        .attachment_mode = entry.attachment_mode,
        .draw_layer = entry.draw_layer,
        .movement_flags = entry.movement_flags,
        .money = entry.money,
        .stage_exit_id = entry.stage_exit_id,
        .runtime_flags = entry.runtime_flags,
        .buyable_active = entry.buyable_active,
        .buyable_display_quantity = entry.buyable_display_quantity,
        .buyable_display_icon_animation_id = entry.buyable_display_icon_animation_id,
        .buyable_shop_owner_id = entry.buyable_shop_owner_id,
        .animate = entry.animate,
        .animation_loop = entry.animation_loop,
        .animation_finished = entry.animation_finished,
        .animation_id = entry.animation_id,
        .animation_frame = entry.animation_frame,
        .animation_time = entry.animation_time,
        .animation_speed = entry.animation_speed,
    };
    payload.effect_count = static_cast<std::uint8_t>(
        std::min<std::size_t>(entry.effect_count, payload.effects.size())
    );
    for (std::size_t i = 0; i < payload.effect_count; ++i) {
        const EntityEffectEntry& effect = entry.effects[i];
        payload.effects[i] = EntityReplicatedEffect{
            .id = static_cast<EffectId>(effect.id),
            .count = effect.count,
            .value = effect.value,
            .frames_remaining = effect.frames_remaining,
        };
    }
    message.payload = payload;
    return message;
}

NetMessage MakeEntityCarryMessage(const EntityCarryMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = static_cast<NetMessageType>(entry.message_type);
    switch (message.type) {
    case NetMessageType::EntityHeld:
        message.payload = EntityHeldMessage{
            .holder_id = entry.holder_id,
            .held_id = entry.entity_id,
            .attachment_mode =
                entry.attachment_mode == static_cast<std::uint16_t>(AttachmentMode::Back)
                    ? AttachmentMode::Back
                    : AttachmentMode::Held,
        };
        break;
    case NetMessageType::EntityDropped:
        message.payload = EntityDroppedMessage{
            .entity_id = entry.entity_id,
            .dropped_by_id = entry.holder_id,
            .pos = Vec2::New(entry.pos_x, entry.pos_y),
            .vel = Vec2::New(entry.vel_x, entry.vel_y),
        };
        break;
    case NetMessageType::EntityThrown:
        message.payload = EntityThrownMessage{
            .entity_id = entry.entity_id,
            .pos = Vec2::New(entry.pos_x, entry.pos_y),
            .vel = Vec2::New(entry.vel_x, entry.vel_y),
            .thrower_id = entry.thrower_id,
        };
        break;
    default:
        message.type = NetMessageType::None;
        message.payload = std::monostate{};
        break;
    }
    return message;
}

NetMessage MakeEntityLifecycleMessage(const EntityLifecycleMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = static_cast<NetMessageType>(entry.message_type);
    switch (message.type) {
    case NetMessageType::EntityDeactivated:
        message.payload = EntityIdMessage{
            .entity_id = entry.entity_id,
        };
        break;
    default:
        message.type = NetMessageType::None;
        message.payload = std::monostate{};
        break;
    }
    return message;
}


} // namespace splonks::network
