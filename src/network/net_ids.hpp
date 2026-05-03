#pragma once

#include "player_id.hpp"

#include <cstdint>
#include <optional>

namespace splonks::network {

using NetEntityId = std::uint64_t;
using NetEventId = std::uint64_t;
using StageInstanceId = std::uint64_t;

constexpr NetEntityId kInvalidNetEntityId = 0;
constexpr NetEventId kInvalidNetEventId = 0;
constexpr StageInstanceId kInvalidStageInstanceId = 0;
constexpr NetEntityId kPlayerNetEntityIdMask = 0xFFFF000000000000ULL;

enum class NetRole : std::uint8_t {
    Offline,
    Coordinator,
    Peer,
};

struct NetEntityOwner {
    std::optional<PlayerId> player_id = std::nullopt;

    static NetEntityOwner Coordinator() { return NetEntityOwner{}; }
    static NetEntityOwner Player(PlayerId player_id) {
        return NetEntityOwner{.player_id = player_id};
    }

    bool IsCoordinator() const { return !player_id.has_value(); }
};

inline NetEntityId MakePlayerNetEntityId(PlayerId player_id) {
    return kPlayerNetEntityIdMask | static_cast<NetEntityId>(player_id);
}

inline bool IsPlayerNetEntityId(NetEntityId entity_id) {
    return entity_id != kInvalidNetEntityId &&
           (entity_id & kPlayerNetEntityIdMask) == kPlayerNetEntityIdMask;
}

inline PlayerId GetPlayerIdFromNetEntityId(NetEntityId entity_id) {
    return static_cast<PlayerId>(entity_id & ~kPlayerNetEntityIdMask);
}

inline std::optional<PlayerId> GetSpawnedNetEntityOwnerPlayerId(NetEntityId entity_id) {
    if (entity_id == kInvalidNetEntityId || IsPlayerNetEntityId(entity_id)) {
        return std::nullopt;
    }
    const PlayerId owner_player_id = static_cast<PlayerId>(entity_id >> 48U);
    if (owner_player_id == kInvalidPlayerId) {
        return std::nullopt;
    }
    return owner_player_id;
}

inline bool IsSpawnedNetEntityOwnedByPlayer(NetEntityId entity_id, PlayerId player_id) {
    const std::optional<PlayerId> owner_player_id = GetSpawnedNetEntityOwnerPlayerId(entity_id);
    return owner_player_id.has_value() && *owner_player_id == player_id;
}

} // namespace splonks::network
