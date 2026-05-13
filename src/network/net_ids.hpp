#pragma once

#include "player_id.hpp"

#include <cstdint>
#include <optional>

namespace splonks::network {

using NetEntId = std::uint64_t;
using StageInstanceId = std::uint64_t;

constexpr NetEntId kInvalidNetEntId = 0;
constexpr StageInstanceId kInvalidStageInstanceId = 0;
constexpr NetEntId kPlayerNetEntIdMask = 0xFFFF000000000000ULL;

enum class NetRole : std::uint8_t {
    Offline,
    Host,
    Peer,
};

inline NetEntId MakePlayerNetEntId(PlayerId player_id) {
    return kPlayerNetEntIdMask | static_cast<NetEntId>(player_id);
}

inline bool IsPlayerNetEntId(NetEntId ent_id) {
    return ent_id != kInvalidNetEntId &&
           (ent_id & kPlayerNetEntIdMask) == kPlayerNetEntIdMask;
}

inline PlayerId GetPlayerIdFromNetEntId(NetEntId ent_id) {
    return static_cast<PlayerId>(ent_id & ~kPlayerNetEntIdMask);
}

inline std::optional<PlayerId> GetSpawnedNetEntInputOwnerPlayerId(NetEntId ent_id) {
    if (ent_id == kInvalidNetEntId || IsPlayerNetEntId(ent_id)) {
        return std::nullopt;
    }
    const PlayerId input_owner_player_id = static_cast<PlayerId>(ent_id >> 48U);
    if (input_owner_player_id == kInvalidPlayerId) {
        return std::nullopt;
    }
    return input_owner_player_id;
}

} // namespace splonks::network
