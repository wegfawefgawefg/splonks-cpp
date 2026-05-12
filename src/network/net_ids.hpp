#pragma once

#include "player_id.hpp"

#include <cstdint>
#include <optional>

namespace splonks::network {

using NetEntityId = std::uint64_t;
using StageInstanceId = std::uint64_t;

constexpr NetEntityId kInvalidNetEntityId = 0;
constexpr StageInstanceId kInvalidStageInstanceId = 0;
constexpr NetEntityId kPlayerNetEntityIdMask = 0xFFFF000000000000ULL;

enum class NetRole : std::uint8_t {
    Offline,
    Coordinator,
    Peer,
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

inline std::optional<PlayerId> GetSpawnedNetEntityInputOwnerPlayerId(NetEntityId entity_id) {
    if (entity_id == kInvalidNetEntityId || IsPlayerNetEntityId(entity_id)) {
        return std::nullopt;
    }
    const PlayerId input_owner_player_id = static_cast<PlayerId>(entity_id >> 48U);
    if (input_owner_player_id == kInvalidPlayerId) {
        return std::nullopt;
    }
    return input_owner_player_id;
}

} // namespace splonks::network
