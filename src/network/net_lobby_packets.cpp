#include "network/net_lobby_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <variant>

namespace splonks::network {

bool IsReplicatedEntitySpawnedEvent(const NetEvent& event) {
    return event.type == NetEventType::EntitySpawned &&
           std::holds_alternative<EntitySpawnedEvent>(event.payload);
}

bool IsReplicatedEntityDamageEvent(const NetEvent& event) {
    return event.type == NetEventType::EntityDamaged &&
           std::holds_alternative<EntityDamagedEvent>(event.payload);
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

bool IsReplicatedPlayerStateEvent(const NetEvent& event) {
    return event.type == NetEventType::PlayerStatePatched &&
           std::holds_alternative<PlayerStatePatchedEvent>(event.payload);
}

bool IsReplicatedRunStateEvent(const NetEvent& event) {
    return event.type == NetEventType::RunStatePatched &&
           std::holds_alternative<RunStatePatchedEvent>(event.payload);
}

bool IsReplicatedPresentationCommandEvent(const NetEvent& event) {
    return event.type == NetEventType::PresentationCommand &&
           std::holds_alternative<PresentationCommandEvent>(event.payload);
}

EntitySpawnedEventEntry MakeEntitySpawnedEventEntry(const NetEvent& event) {
    const EntitySpawnedEvent* const payload = std::get_if<EntitySpawnedEvent>(&event.payload);
    EntitySpawnedEventEntry entry{
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

EntityStateEventEntry MakeEntityStateEventEntry(const NetEvent& event) {
    const EntityStatePatchedEvent* const payload = std::get_if<EntityStatePatchedEvent>(&event.payload);
    EntityStateEventEntry entry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
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
        entry.attachment_mode = static_cast<std::uint16_t>(held_payload->attachment_mode);
    } else if (const EntityDroppedEvent* const dropped_payload = std::get_if<EntityDroppedEvent>(&event.payload)) {
        entry.entity_id = dropped_payload->entity_id;
        entry.holder_id = dropped_payload->dropped_by_id;
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

PlayerStateEventEntry MakePlayerStateEventEntry(const NetEvent& event) {
    const PlayerStatePatchedEvent* const payload = std::get_if<PlayerStatePatchedEvent>(&event.payload);
    PlayerStateEventEntry entry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .player_entity_id = payload != nullptr ? payload->player_entity_id : kInvalidNetEntityId,
        .player_id = payload != nullptr ? payload->player_id : kInvalidPlayerId,
        .health = payload != nullptr ? payload->health : 0U,
        .money = payload != nullptr ? payload->money : 0U,
        .wanted = payload != nullptr ? payload->wanted : static_cast<std::uint8_t>(0),
        .connected = payload != nullptr ? payload->connected : static_cast<std::uint8_t>(1),
        .effect_count = payload != nullptr ? payload->effect_count : static_cast<std::uint8_t>(0),
    };
    if (payload == nullptr) {
        return entry;
    }
    for (std::size_t i = 0; i < entry.tool_slots.size() && i < payload->tool_slots.size(); ++i) {
        const PlayerStatePatchedToolSlot& slot = payload->tool_slots[i];
        entry.tool_slots[i] = PlayerStateToolSlotEntry{
            .kind = static_cast<std::uint16_t>(slot.kind),
            .count = slot.count,
            .cooldown = slot.cooldown,
            .active = slot.active,
        };
    }
    entry.effect_count = static_cast<std::uint8_t>(
        std::min<std::size_t>(entry.effect_count, entry.effects.size())
    );
    for (std::size_t i = 0; i < entry.effect_count; ++i) {
        const PlayerStatePatchedEffect& effect = payload->effects[i];
        entry.effects[i] = PlayerStateEffectEntry{
            .id = static_cast<std::uint16_t>(effect.id),
            .count = effect.count,
            .value = effect.value,
            .frames_remaining = effect.frames_remaining,
        };
    }
    return entry;
}

RunStateEventEntry MakeRunStateEventEntry(const NetEvent& event) {
    const RunStatePatchedEvent* const payload = std::get_if<RunStatePatchedEvent>(&event.payload);
    return RunStateEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .quest_id = payload != nullptr
            ? static_cast<std::uint16_t>(payload->quest_id)
            : static_cast<std::uint16_t>(QuestId::None),
        .stage_type = payload != nullptr
            ? static_cast<std::uint16_t>(payload->stage_type)
            : static_cast<std::uint16_t>(0),
        .frame = payload != nullptr ? payload->frame : 0U,
        .stage_frame = payload != nullptr ? payload->stage_frame : 0U,
        .depth = payload != nullptr ? payload->depth : 0U,
        .points = payload != nullptr ? payload->points : 0U,
        .deaths = payload != nullptr ? payload->deaths : 0U,
        .quest_level_number = payload != nullptr ? payload->quest_level_number : 0,
        .generation_seed = payload != nullptr ? payload->generation_seed : 0U,
        .tile_change_generation = payload != nullptr ? payload->tile_change_generation : 0U,
        .stage_gravity = payload != nullptr ? payload->stage_gravity : 0.0F,
        .border_left_tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->border_left_tile)
            : static_cast<std::uint16_t>(0),
        .border_right_tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->border_right_tile)
            : static_cast<std::uint16_t>(0),
        .border_top_tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->border_top_tile)
            : static_cast<std::uint16_t>(0),
        .border_bottom_tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->border_bottom_tile)
            : static_cast<std::uint16_t>(0),
        .void_death_y = payload != nullptr ? payload->void_death_y : 0,
        .wrap_core_origin_x = payload != nullptr ? payload->wrap_core_origin_x : 0U,
        .wrap_core_origin_y = payload != nullptr ? payload->wrap_core_origin_y : 0U,
        .wrap_core_size_x = payload != nullptr ? payload->wrap_core_size_x : 0U,
        .wrap_core_size_y = payload != nullptr ? payload->wrap_core_size_y : 0U,
        .classic_made_black_market =
            payload != nullptr ? payload->classic_made_black_market : static_cast<std::uint8_t>(0),
        .classic_made_udjat_eye =
            payload != nullptr ? payload->classic_made_udjat_eye : static_cast<std::uint8_t>(0),
        .classic_has_udjat_eye =
            payload != nullptr ? payload->classic_has_udjat_eye : static_cast<std::uint8_t>(0),
        .classic_made_moai =
            payload != nullptr ? payload->classic_made_moai : static_cast<std::uint8_t>(0),
        .classic_has_hedjet =
            payload != nullptr ? payload->classic_has_hedjet : static_cast<std::uint8_t>(0),
        .classic_has_sceptre =
            payload != nullptr ? payload->classic_has_sceptre : static_cast<std::uint8_t>(0),
        .classic_has_book_of_dead =
            payload != nullptr ? payload->classic_has_book_of_dead : static_cast<std::uint8_t>(0),
        .has_generation_seed =
            payload != nullptr ? payload->has_generation_seed : static_cast<std::uint8_t>(0),
        .border_wrap_x =
            payload != nullptr ? payload->border_wrap_x : static_cast<std::uint8_t>(0),
        .border_wrap_y =
            payload != nullptr ? payload->border_wrap_y : static_cast<std::uint8_t>(0),
        .has_void_death_y =
            payload != nullptr ? payload->has_void_death_y : static_cast<std::uint8_t>(0),
        .camera_clamp_enabled =
            payload != nullptr ? payload->camera_clamp_enabled : static_cast<std::uint8_t>(1),
        .wrap_transform_active =
            payload != nullptr ? payload->wrap_transform_active : static_cast<std::uint8_t>(0),
        .game_over = payload != nullptr ? payload->game_over : static_cast<std::uint8_t>(0),
        .win = payload != nullptr ? payload->win : static_cast<std::uint8_t>(0),
        .has_snapshot_fingerprint =
            payload != nullptr ? payload->has_snapshot_fingerprint : static_cast<std::uint8_t>(0),
        .wrap_padding_tiles =
            payload != nullptr ? payload->wrap_padding_tiles : static_cast<std::uint16_t>(0),
        .sac_altar_favor = payload != nullptr ? payload->sac_altar_favor : 0,
        .sac_altar_reward_tier = payload != nullptr ? payload->sac_altar_reward_tier : 0U,
        .snapshot_fingerprint = payload != nullptr ? payload->snapshot_fingerprint : 0U,
    };
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
        .entity_shake_amount = payload != nullptr ? payload->entity_shake_amount : 0.0F,
        .foreground_shake_amount = payload != nullptr ? payload->foreground_shake_amount : 0.0F,
        .background_shake_amount = payload != nullptr ? payload->background_shake_amount : 0.0F,
        .area_entity_shake_amount = payload != nullptr ? payload->area_entity_shake_amount : 0.0F,
        .shake_radius_tiles = payload != nullptr ? payload->shake_radius_tiles : 0.0F,
    };
}

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
) {
    if (transport.capture_outgoing_packets) {
        UdpPacket packet;
        packet.endpoint = endpoint;
        packet.size = std::min(encoded.size, packet.bytes.size());
        std::copy_n(encoded.bytes.begin(), packet.size, packet.bytes.begin());
        transport.captured_packets.push_back(packet);
        return;
    }

    std::string error;
    if (!transport.socket.Send(endpoint, encoded.bytes.data(), encoded.size, &error)) {
        transport.last_error = error;
    }
}

bool IsReplicatedEntityStateEvent(const NetEvent& event) {
    return event.type == NetEventType::EntityStatePatched &&
           std::holds_alternative<EntityStatePatchedEvent>(event.payload);
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
    EntitySpawnedEvent payload{
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
    event.payload = payload;
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
    EntityStatePatchedEvent payload{
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
    event.payload = payload;
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
            .attachment_mode =
                entry.attachment_mode == static_cast<std::uint16_t>(AttachmentMode::Back)
                    ? AttachmentMode::Back
                    : AttachmentMode::Held,
        };
        break;
    case NetEventType::EntityDropped:
        event.payload = EntityDroppedEvent{
            .entity_id = entry.entity_id,
            .dropped_by_id = entry.holder_id,
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

NetEvent MakePlayerStateEvent(const PlayerStateEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    PlayerStatePatchedEvent payload{
        .player_entity_id = entry.player_entity_id,
        .player_id = entry.player_id,
        .health = entry.health,
        .money = entry.money,
        .wanted = entry.wanted,
        .connected = entry.connected,
        .effect_count = entry.effect_count,
    };
    for (std::size_t i = 0; i < payload.tool_slots.size() && i < entry.tool_slots.size(); ++i) {
        const PlayerStateToolSlotEntry& slot = entry.tool_slots[i];
        payload.tool_slots[i] = PlayerStatePatchedToolSlot{
            .kind = static_cast<ToolKind>(slot.kind),
            .count = slot.count,
            .cooldown = slot.cooldown,
            .active = slot.active,
        };
    }
    payload.effect_count = static_cast<std::uint8_t>(
        std::min<std::size_t>(payload.effect_count, payload.effects.size())
    );
    for (std::size_t i = 0; i < payload.effect_count; ++i) {
        const PlayerStateEffectEntry& effect = entry.effects[i];
        payload.effects[i] = PlayerStatePatchedEffect{
            .id = static_cast<EffectId>(effect.id),
            .count = effect.count,
            .value = effect.value,
            .frames_remaining = effect.frames_remaining,
        };
    }
    event.type = NetEventType::PlayerStatePatched;
    event.payload = payload;
    return event;
}

NetEvent MakeRunStateEvent(const RunStateEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::RunStatePatched;
    event.payload = RunStatePatchedEvent{
        .quest_id = static_cast<QuestId>(entry.quest_id),
        .frame = entry.frame,
        .stage_frame = entry.stage_frame,
        .depth = entry.depth,
        .points = entry.points,
        .deaths = entry.deaths,
        .stage_type = entry.stage_type,
        .quest_level_number = entry.quest_level_number,
        .generation_seed = entry.generation_seed,
        .tile_change_generation = entry.tile_change_generation,
        .stage_gravity = entry.stage_gravity,
        .border_left_tile = static_cast<Tile>(entry.border_left_tile),
        .border_right_tile = static_cast<Tile>(entry.border_right_tile),
        .border_top_tile = static_cast<Tile>(entry.border_top_tile),
        .border_bottom_tile = static_cast<Tile>(entry.border_bottom_tile),
        .void_death_y = entry.void_death_y,
        .has_generation_seed = entry.has_generation_seed,
        .border_wrap_x = entry.border_wrap_x,
        .border_wrap_y = entry.border_wrap_y,
        .has_void_death_y = entry.has_void_death_y,
        .camera_clamp_enabled = entry.camera_clamp_enabled,
        .wrap_transform_active = entry.wrap_transform_active,
        .game_over = entry.game_over,
        .win = entry.win,
        .wrap_padding_tiles = entry.wrap_padding_tiles,
        .wrap_core_origin_x = entry.wrap_core_origin_x,
        .wrap_core_origin_y = entry.wrap_core_origin_y,
        .wrap_core_size_x = entry.wrap_core_size_x,
        .wrap_core_size_y = entry.wrap_core_size_y,
        .classic_made_black_market = entry.classic_made_black_market,
        .classic_made_udjat_eye = entry.classic_made_udjat_eye,
        .classic_has_udjat_eye = entry.classic_has_udjat_eye,
        .classic_made_moai = entry.classic_made_moai,
        .classic_has_hedjet = entry.classic_has_hedjet,
        .classic_has_sceptre = entry.classic_has_sceptre,
        .classic_has_book_of_dead = entry.classic_has_book_of_dead,
        .sac_altar_favor = entry.sac_altar_favor,
        .sac_altar_reward_tier = entry.sac_altar_reward_tier,
        .has_snapshot_fingerprint = entry.has_snapshot_fingerprint,
        .snapshot_fingerprint = entry.snapshot_fingerprint,
    };
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
        .entity_shake_amount = entry.entity_shake_amount,
        .foreground_shake_amount = entry.foreground_shake_amount,
        .background_shake_amount = entry.background_shake_amount,
        .area_entity_shake_amount = entry.area_entity_shake_amount,
        .shake_radius_tiles = entry.shake_radius_tiles,
    };
    return event;
}

} // namespace splonks::network
