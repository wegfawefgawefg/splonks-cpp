#pragma once

#include "ent.hpp"
#include "math_types.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <limits>
#include <optional>

namespace splonks {

inline std::optional<VID> FindPrimaryLocalPlayerVid(const State& state) {
    const PlayerSlot* const slot = state.players.FindPrimaryLocal();
    if (slot == nullptr || !slot->connected || !slot->ent_vid.has_value()) {
        return std::nullopt;
    }
    return slot->ent_vid;
}

inline Ent* GetPrimaryLocalPlayerMut(State& state) {
    const std::optional<VID> vid = FindPrimaryLocalPlayerVid(state);
    return vid.has_value() ? state.ents.GetEntMut(*vid) : nullptr;
}

inline const Ent* GetPrimaryLocalPlayer(const State& state) {
    const std::optional<VID> vid = FindPrimaryLocalPlayerVid(state);
    return vid.has_value() ? state.ents.GetEnt(*vid) : nullptr;
}

inline std::optional<VID> FindFirstConnectedPlayerVid(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.ent_vid.has_value()) {
            return slot.ent_vid;
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
    if (!slot.connected || !slot.ent_vid.has_value()) {
        return false;
    }
    if (state.net_session.input_lockstep_enabled) {
        return true;
    }
    return slot.connection_kind == PlayerConnectionKind::Local ||
           (state.net_session.role == network::NetRole::Host &&
            slot.connection_kind == PlayerConnectionKind::Remote);
}

inline bool HasAnyConnectedPlayerEnt(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player != nullptr && player->active) {
            return true;
        }
    }
    return false;
}

inline std::optional<VID> FindFirstConnectedLivingPlayerVid(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player != nullptr && player->active && player->condition != EntCondition::Dead) {
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
    sim::FxVec2 world_pos,
    bool require_normal_condition = true
) {
    std::optional<VID> best_vid;
    std::optional<sim::Scalar> best_dist_sq;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active) {
            continue;
        }
        if (require_normal_condition && player->condition != EntCondition::Normal) {
            continue;
        }
        const sim::FxVec2 delta = GetNearestWorldDelta(state.stage, world_pos, player->GetSimPos());
        const sim::Scalar dist_sq = gfxp::length_sq(delta);
        if (!best_dist_sq.has_value() || dist_sq < *best_dist_sq) {
            best_dist_sq = dist_sq;
            best_vid = player->vid;
        }
    }
    return best_vid;
}

inline std::optional<VID> FindNearestPlayerVid(
    const State& state,
    FVec2 world_pos,
    bool require_normal_condition = true
) {
    return FindNearestPlayerVid(state, sim::ToSimVec2(world_pos), require_normal_condition);
}

inline const Ent* FindNearestPlayer(
    const State& state,
    sim::FxVec2 world_pos,
    bool require_normal_condition = true
) {
    const std::optional<VID> vid =
        FindNearestPlayerVid(state, world_pos, require_normal_condition);
    return vid.has_value() ? state.ents.GetEnt(*vid) : nullptr;
}

inline Ent* FindNearestPlayerMut(
    State& state,
    sim::FxVec2 world_pos,
    bool require_normal_condition = true
) {
    const std::optional<VID> vid =
        FindNearestPlayerVid(state, world_pos, require_normal_condition);
    return vid.has_value() ? state.ents.GetEntMut(*vid) : nullptr;
}

inline const Ent* FindNearestPlayer(
    const State& state,
    FVec2 world_pos,
    bool require_normal_condition = true
) {
    return FindNearestPlayer(state, sim::ToSimVec2(world_pos), require_normal_condition);
}

inline Ent* FindNearestPlayerMut(
    State& state,
    FVec2 world_pos,
    bool require_normal_condition = true
) {
    return FindNearestPlayerMut(state, sim::ToSimVec2(world_pos), require_normal_condition);
}

} // namespace splonks
