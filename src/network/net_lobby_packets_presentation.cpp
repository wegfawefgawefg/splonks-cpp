#include "network/net_lobby_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <variant>

namespace splonks::network {

bool IsReplicatedPresentationCommandMessage(const NetMessage& message) {
    return message.type == NetMessageType::PresentationCommand &&
           std::holds_alternative<PresentationCommandMessage>(message.payload);
}


PresentationCommandMessageEntry MakePresentationCommandMessageEntry(const NetMessage& message) {
    const PresentationCommandMessage* const payload = std::get_if<PresentationCommandMessage>(&message.payload);
    return PresentationCommandMessageEntry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
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
        .effect_count = payload != nullptr ? payload->effect_count : 0,
        .effect_scale = payload != nullptr ? payload->effect_scale : 1.0F,
        .entity_shake_amount = payload != nullptr ? payload->entity_shake_amount : 0.0F,
        .foreground_shake_amount = payload != nullptr ? payload->foreground_shake_amount : 0.0F,
        .background_shake_amount = payload != nullptr ? payload->background_shake_amount : 0.0F,
        .area_entity_shake_amount = payload != nullptr ? payload->area_entity_shake_amount : 0.0F,
        .shake_radius_tiles = payload != nullptr ? payload->shake_radius_tiles : 0.0F,
        .light_strength = payload != nullptr ? payload->light_strength : 0.0F,
        .light_color = payload != nullptr ? payload->light_color : Color3::White(),
        .light_radius = payload != nullptr ? payload->light_radius : 0,
        .light_lifetime_frames = payload != nullptr ? payload->light_lifetime_frames : 0,
    };
}

NetMessage MakePresentationCommandMessage(const PresentationCommandMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = NetMessageType::PresentationCommand;
    message.payload = PresentationCommandMessage{
        .kind = entry.kind,
        .effect_id = entry.effect_id,
        .audio_asset_id = entry.audio_asset_id,
        .source_entity_id = entry.source_entity_id,
        .target_entity_id = entry.target_entity_id,
        .source_pos = Vec2::New(entry.source_x, entry.source_y),
        .target_pos = Vec2::New(entry.target_x, entry.target_y),
        .direction_x = entry.direction_x,
        .direction_y = entry.direction_y,
        .effect_count = entry.effect_count,
        .effect_scale = entry.effect_scale,
        .entity_shake_amount = entry.entity_shake_amount,
        .foreground_shake_amount = entry.foreground_shake_amount,
        .background_shake_amount = entry.background_shake_amount,
        .area_entity_shake_amount = entry.area_entity_shake_amount,
        .shake_radius_tiles = entry.shake_radius_tiles,
        .light_strength = entry.light_strength,
        .light_color = entry.light_color,
        .light_radius = entry.light_radius,
        .light_lifetime_frames = entry.light_lifetime_frames,
    };
    return message;
}


} // namespace splonks::network
