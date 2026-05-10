#include "network/net_lobby_internal.hpp"

#include <variant>

namespace splonks::network {

namespace {

IVec2 GetTileEventPos(const NetEvent& event) {
    if (const TileBrokenEvent* const payload = std::get_if<TileBrokenEvent>(&event.payload)) {
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
    return Tile::Air;
}

TileRotation GetTileEventRotation(const NetEvent& event) {
    if (const TileChangedEvent* const payload = std::get_if<TileChangedEvent>(&event.payload)) {
        return payload->rotation;
    }
    return kTileRotation0;
}

NetTileLayer GetTileEventLayer(const NetEvent& event) {
    if (const TileChangedEvent* const payload = std::get_if<TileChangedEvent>(&event.payload)) {
        return payload->layer;
    }
    return NetTileLayer::Foreground;
}

} // namespace

bool IsReplicatedTileEvent(const NetEvent& event) {
    return (event.type == NetEventType::TileBroken &&
               std::holds_alternative<TileBrokenEvent>(event.payload)) ||
           (event.type == NetEventType::TileChanged &&
               std::holds_alternative<TileChangedEvent>(event.payload));
}

bool IsReplicatedFluidCellEvent(const NetEvent& event) {
    return event.type == NetEventType::FluidCellPatched &&
           std::holds_alternative<FluidCellPatchedEvent>(event.payload);
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
        .rotation = GetTileEventRotation(event),
        .layer = static_cast<std::uint8_t>(GetTileEventLayer(event)),
        .tile_x = static_cast<std::int32_t>(tile_pos.x),
        .tile_y = static_cast<std::int32_t>(tile_pos.y),
    };
}

FluidCellEventEntry MakeFluidCellEventEntry(const NetEvent& event) {
    const FluidCellPatchedEvent* const payload = std::get_if<FluidCellPatchedEvent>(&event.payload);
    return FluidCellEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
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
    case NetEventType::TileChanged:
        event.payload = TileChangedEvent{
            .tile_pos = tile_pos,
            .tile = static_cast<Tile>(entry.tile),
            .rotation = NormalizeTileRotation(entry.rotation),
            .layer = static_cast<NetTileLayer>(entry.layer),
        };
        break;
    default:
        event.type = NetEventType::None;
        event.payload = std::monostate{};
        break;
    }
    return event;
}

NetEvent MakeFluidCellEvent(const FluidCellEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::FluidCellPatched;
    event.payload = FluidCellPatchedEvent{
        .tile_pos = IVec2::New(entry.tile_x, entry.tile_y),
        .tile = static_cast<Tile>(entry.tile),
        .amount = entry.amount,
        .velocity = Vec2::New(entry.velocity_x, entry.velocity_y),
        .gravity = Vec2::New(entry.gravity_x, entry.gravity_y),
        .temp_gravity = Vec2::New(entry.temp_gravity_x, entry.temp_gravity_y),
        .gravity_strength = entry.gravity_strength,
    };
    return event;
}

} // namespace splonks::network
