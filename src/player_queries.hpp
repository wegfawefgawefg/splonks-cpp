#pragma once

#include "entity.hpp"
#include "math_types.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <limits>
#include <optional>

namespace splonks {

inline std::optional<VID> FindPrimaryLocalPlayerVid(const State& state) {
    const PlayerSlot* const slot = state.players.FindPrimaryLocal();
    if (slot == nullptr || !slot->connected || !slot->entity_vid.has_value()) {
        return std::nullopt;
    }
    return slot->entity_vid;
}

inline Entity* GetPrimaryLocalPlayerMut(State& state) {
    const std::optional<VID> vid = FindPrimaryLocalPlayerVid(state);
    return vid.has_value() ? state.entity_manager.GetEntityMut(*vid) : nullptr;
}

inline const Entity* GetPrimaryLocalPlayer(const State& state) {
    const std::optional<VID> vid = FindPrimaryLocalPlayerVid(state);
    return vid.has_value() ? state.entity_manager.GetEntity(*vid) : nullptr;
}

inline std::optional<VID> FindFirstConnectedPlayerVid(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.entity_vid.has_value()) {
            return slot.entity_vid;
        }
    }
    return std::nullopt;
}

inline bool HasAnyConnectedPlayerSlot(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.player_id != kInvalidPlayerId) {
            return true;
        }
    }
    return false;
}

inline bool ShouldSimulatePlayerSlotGameplay(const State& state, const PlayerSlot& slot) {
    if (!slot.connected || !slot.entity_vid.has_value()) {
        return false;
    }
    if (state.net_session.input_lockstep_enabled) {
        return true;
    }
    return slot.connection_kind == PlayerConnectionKind::Local ||
           (state.net_session.role == network::NetRole::Coordinator &&
            slot.connection_kind == PlayerConnectionKind::Remote);
}

inline bool HasAnyConnectedPlayerEntity(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.entity_vid.has_value()) {
            continue;
        }
        const Entity* const player = state.entity_manager.GetEntity(*slot.entity_vid);
        if (player != nullptr && player->active) {
            return true;
        }
    }
    return false;
}

inline std::optional<VID> FindFirstConnectedLivingPlayerVid(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.entity_vid.has_value()) {
            continue;
        }
        const Entity* const player = state.entity_manager.GetEntity(*slot.entity_vid);
        if (player != nullptr && player->active && player->condition != EntityCondition::Dead) {
            return player->vid;
        }
    }
    return std::nullopt;
}

inline bool HasAnyConnectedLivingPlayer(const State& state) {
    return FindFirstConnectedLivingPlayerVid(state).has_value();
}

inline std::optional<VID> FindNearestPlayerVid(
    const State& state,
    Vec2 world_pos,
    bool require_normal_condition = true
) {
    std::optional<VID> best_vid;
    float best_dist_sq = std::numeric_limits<float>::max();
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.entity_vid.has_value()) {
            continue;
        }
        const Entity* const player = state.entity_manager.GetEntity(*slot.entity_vid);
        if (player == nullptr || !player->active) {
            continue;
        }
        if (require_normal_condition && player->condition != EntityCondition::Normal) {
            continue;
        }
        const Vec2 delta = GetNearestWorldDelta(state.stage, world_pos, player->pos);
        const float dist_sq = delta.x * delta.x + delta.y * delta.y;
        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_vid = player->vid;
        }
    }
    return best_vid;
}

inline const Entity* FindNearestPlayer(
    const State& state,
    Vec2 world_pos,
    bool require_normal_condition = true
) {
    const std::optional<VID> vid =
        FindNearestPlayerVid(state, world_pos, require_normal_condition);
    return vid.has_value() ? state.entity_manager.GetEntity(*vid) : nullptr;
}

inline Entity* FindNearestPlayerMut(
    State& state,
    Vec2 world_pos,
    bool require_normal_condition = true
) {
    const std::optional<VID> vid =
        FindNearestPlayerVid(state, world_pos, require_normal_condition);
    return vid.has_value() ? state.entity_manager.GetEntityMut(*vid) : nullptr;
}

} // namespace splonks
