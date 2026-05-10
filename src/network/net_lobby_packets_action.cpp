#include "network/net_lobby_internal.hpp"

#include <variant>

namespace splonks::network {

namespace {

std::uint16_t BuildActionRequestFlags(const ActionRequestEvent& payload) {
    std::uint16_t flags = 0;
    if (payload.clear_velocity) {
        flags |= kActionRequestFlagClearVelocity;
    }
    if (payload.clear_acceleration) {
        flags |= kActionRequestFlagClearAcceleration;
    }
    if (payload.knockback_on_no_damage) {
        flags |= kActionRequestFlagKnockbackOnNoDamage;
    }
    return flags;
}

} // namespace

bool IsReplicatedActionRequestEvent(const NetEvent& event) {
    return event.type == NetEventType::ActionRequest &&
           std::holds_alternative<ActionRequestEvent>(event.payload);
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
        .tool_slot = payload != nullptr ? payload->tool_slot : 0U,
        .use_edge = payload != nullptr
            ? static_cast<std::uint16_t>(payload->use_edge)
            : static_cast<std::uint16_t>(NetUseEdge::None),
    };
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
        .knockback_on_no_damage = (entry.flags & kActionRequestFlagKnockbackOnNoDamage) != 0,
        .tool_slot = entry.tool_slot,
        .use_edge = static_cast<NetUseEdge>(entry.use_edge),
    };
    return event;
}

} // namespace splonks::network
