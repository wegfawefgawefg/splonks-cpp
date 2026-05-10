#pragma once

#include "network/net_message.hpp"

#include <cstdint>
#include <optional>

namespace splonks {

struct Audio;
struct Graphics;
struct State;
struct VID;

namespace network {

struct NetSessionState;

std::optional<VID> FindEntityVidForMessage(
    NetSessionState& session,
    State& state,
    NetEntityId entity_id
);

void ApplyEntitySpawnedMessage(
    NetSessionState& session,
    State& state,
    const EntitySpawnedMessage& payload,
    Graphics* graphics
);
void ApplyEntityDamagedMessage(
    NetSessionState& session,
    State& state,
    Audio* audio,
    PlayerId source_player_id,
    const EntityDamagedMessage& payload
);
void ApplyEntityDeactivatedMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityIdMessage& payload
);
void ApplyEntityStatePatchedMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    PlayerId source_player_id,
    const EntityStatePatchedMessage& payload
);
void ApplyEntityHeldMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityHeldMessage& payload
);
void ApplyEntityDroppedMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityDroppedMessage& payload
);
void ApplyEntityThrownMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityThrownMessage& payload
);
void ApplyPlayerStatePatchedMessage(
    NetSessionState& session,
    State& state,
    const PlayerStatePatchedMessage& payload
);
void ApplyRunStatePatchedMessage(
    State& state,
    const RunStatePatchedMessage& payload,
    std::optional<std::uint64_t>& pending_snapshot_fingerprint
);
void ApplyPresentationCommandMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const PresentationCommandMessage& payload
);

void ApplyTileBrokenMessage(State& state, Audio* audio, const TileBrokenMessage& payload);
void ApplyTileChangedMessage(State& state, const TileChangedMessage& payload);
void ApplyFluidCellPatchedMessage(State& state, const FluidCellPatchedMessage& payload);

} // namespace network
} // namespace splonks
