#include "network/net_lobby_internal.hpp"

#include <variant>

namespace splonks::network {

namespace {

IVec2 GetTileMessagePos(const NetMessage& message) {
    if (const TileBrokenMessage* const payload = std::get_if<TileBrokenMessage>(&message.payload)) {
        return payload->tile_pos;
    }
    if (const TileChangedMessage* const payload = std::get_if<TileChangedMessage>(&message.payload)) {
        return payload->tile_pos;
    }
    return IVec2::New(0, 0);
}

Tile GetTileMessageTile(const NetMessage& message) {
    if (const TileChangedMessage* const payload = std::get_if<TileChangedMessage>(&message.payload)) {
        return payload->tile;
    }
    return Tile::Air;
}

TileRotation GetTileMessageRotation(const NetMessage& message) {
    if (const TileChangedMessage* const payload = std::get_if<TileChangedMessage>(&message.payload)) {
        return payload->rotation;
    }
    return kTileRotation0;
}

NetTileLayer GetTileMessageLayer(const NetMessage& message) {
    if (const TileChangedMessage* const payload = std::get_if<TileChangedMessage>(&message.payload)) {
        return payload->layer;
    }
    return NetTileLayer::Foreground;
}

} // namespace

bool IsReplicatedTileMessage(const NetMessage& message) {
    return (message.type == NetMessageType::TileBroken &&
               std::holds_alternative<TileBrokenMessage>(message.payload)) ||
           (message.type == NetMessageType::TileChanged &&
               std::holds_alternative<TileChangedMessage>(message.payload));
}

bool IsReplicatedFluidCellMessage(const NetMessage& message) {
    return message.type == NetMessageType::FluidCellPatched &&
           std::holds_alternative<FluidCellPatchedMessage>(message.payload);
}

TileMessageEntry MakeTileMessageEntry(const NetMessage& message) {
    const IVec2 tile_pos = GetTileMessagePos(message);
    return TileMessageEntry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .message_type = static_cast<std::uint16_t>(message.type),
        .tile = static_cast<std::uint16_t>(GetTileMessageTile(message)),
        .rotation = GetTileMessageRotation(message),
        .layer = static_cast<std::uint8_t>(GetTileMessageLayer(message)),
        .tile_x = static_cast<std::int32_t>(tile_pos.x),
        .tile_y = static_cast<std::int32_t>(tile_pos.y),
    };
}

FluidCellMessageEntry MakeFluidCellMessageEntry(const NetMessage& message) {
    const FluidCellPatchedMessage* const payload = std::get_if<FluidCellPatchedMessage>(&message.payload);
    return FluidCellMessageEntry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->tile)
            : static_cast<std::uint16_t>(0),
        .tile_x = payload != nullptr ? payload->tile_pos.x : 0,
        .tile_y = payload != nullptr ? payload->tile_pos.y : 0,
        .amount = payload != nullptr ? payload->amount : 0.0F,
        .velocity_x = payload != nullptr ? payload->velocity.x : 0.0F,
        .velocity_y = payload != nullptr ? payload->velocity.y : 0.0F,
        .gravity_x = payload != nullptr ? payload->gravity.x : 0.0F,
        .gravity_y = payload != nullptr ? payload->gravity.y : 0.0F,
        .temp_gravity_x = payload != nullptr ? payload->temp_gravity.x : 0.0F,
        .temp_gravity_y = payload != nullptr ? payload->temp_gravity.y : 0.0F,
        .gravity_strength = payload != nullptr ? payload->gravity_strength : 0.0F,
    };
}

NetMessage MakeTileMessage(const TileMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = static_cast<NetMessageType>(entry.message_type);
    const IVec2 tile_pos = IVec2::New(entry.tile_x, entry.tile_y);
    switch (message.type) {
    case NetMessageType::TileBroken:
        message.payload = TileBrokenMessage{
            .tile_pos = tile_pos,
            .source_entity_id = kInvalidNetEntityId,
        };
        break;
    case NetMessageType::TileChanged:
        message.payload = TileChangedMessage{
            .tile_pos = tile_pos,
            .tile = static_cast<Tile>(entry.tile),
            .rotation = NormalizeTileRotation(entry.rotation),
            .layer = static_cast<NetTileLayer>(entry.layer),
        };
        break;
    default:
        message.type = NetMessageType::None;
        message.payload = std::monostate{};
        break;
    }
    return message;
}

NetMessage MakeFluidCellMessage(const FluidCellMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = NetMessageType::FluidCellPatched;
    message.payload = FluidCellPatchedMessage{
        .tile_pos = IVec2::New(entry.tile_x, entry.tile_y),
        .tile = static_cast<Tile>(entry.tile),
        .amount = entry.amount,
        .velocity = Vec2::New(entry.velocity_x, entry.velocity_y),
        .gravity = Vec2::New(entry.gravity_x, entry.gravity_y),
        .temp_gravity = Vec2::New(entry.temp_gravity_x, entry.temp_gravity_y),
        .gravity_strength = entry.gravity_strength,
    };
    return message;
}

} // namespace splonks::network
