#include "network/net_lobby_internal.hpp"

#include <variant>

namespace splonks::network {

bool IsReplicatedStageLightMessage(const NetMessage& message) {
    return (message.type == NetMessageType::StageLightAdded &&
               std::holds_alternative<StageLightAddedMessage>(message.payload)) ||
           (message.type == NetMessageType::StageLightRemoved &&
               std::holds_alternative<StageLightRemovedMessage>(message.payload));
}

StageLightMessageEntry MakeStageLightMessageEntry(const NetMessage& message) {
    StageLightMessageEntry entry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .message_type = static_cast<std::uint16_t>(message.type),
    };
    if (const auto* const added = std::get_if<StageLightAddedMessage>(&message.payload)) {
        entry.light_id = added->light_id;
        entry.tile_x = added->tile_pos.x;
        entry.tile_y = added->tile_pos.y;
        entry.radius = added->radius;
    } else if (const auto* const removed = std::get_if<StageLightRemovedMessage>(&message.payload)) {
        entry.light_id = removed->light_id;
    }
    return entry;
}

NetMessage MakeStageLightMessage(const StageLightMessageEntry& entry) {
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
    case NetMessageType::StageLightAdded:
        message.payload = StageLightAddedMessage{
            .light_id = entry.light_id,
            .tile_pos = IVec2::New(entry.tile_x, entry.tile_y),
            .radius = entry.radius,
        };
        break;
    case NetMessageType::StageLightRemoved:
        message.payload = StageLightRemovedMessage{
            .light_id = entry.light_id,
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
