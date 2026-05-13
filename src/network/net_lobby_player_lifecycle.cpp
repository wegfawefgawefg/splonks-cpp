#include "network/net_lobby.hpp"

#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "graphics.hpp"
#include "network/net_ent_links.hpp"
#include "network/net_lobby_internal.hpp"
#include "stage_progression.hpp"
#include "stage_spawning.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace splonks::network {

namespace {

bool ShouldApplyNetworkLifecycleToSlot(const State& state, const PlayerSlot& slot) {
    if (!slot.connected) {
        return false;
    }
    if (state.net_session.input_lockstep_enabled) {
        return true;
    }
    return state.net_session.role == NetRole::Host ||
           slot.connection_kind == PlayerConnectionKind::Local;
}

bool IsPlayerEntDeadOrMissing(State& state, const PlayerSlot& slot) {
    if (!slot.ent_vid.has_value()) {
        return true;
    }
    const Ent* const ent = state.ents.GetEnt(*slot.ent_vid);
    return ent == nullptr || !ent->active || ent->condition == EntCondition::Dead;
}

} // namespace

void EnsureSpawnedPlayer(
    State& state,
    PlayerId player_id,
    bool local,
    bool primary,
    const Vec2& pos,
    const Graphics& graphics
) {
    const bool is_peer_local_player =
        state.net_session.role == NetRole::Peer &&
        player_id == state.net_session.local_player_id;
    const bool effective_local = local || is_peer_local_player;
    const bool effective_primary = primary || is_peer_local_player;

    PlayerSlot& slot = effective_local
        ? state.players.EnsureLocalPlayer(
              player_id,
              "Player " + std::to_string(player_id),
              effective_primary
          )
        : state.players.EnsureRemotePlayer(player_id, "Remote " + std::to_string(player_id));

    if (slot.ent_vid.has_value()) {
        if (Ent* const ent = state.ents.GetEntMut(*slot.ent_vid)) {
            if (ent->active) {
                ent->pos = pos;
                state.net_session.LinkEnt(MakePlayerNetEntId(player_id), ent->vid);
                if (effective_local && effective_primary) {
                    state.controlled_ent_vid = ent->vid;
                }
                return;
            }
        }
        slot.ent_vid.reset();
    }

    const std::optional<VID> vid = SpawnPlayerForPlayerId(state, player_id, pos);
    if (vid.has_value()) {
        state.net_session.LinkEnt(MakePlayerNetEntId(player_id), *vid);
        state.UpdateSidForEnt(vid->id, graphics);
        if (effective_local && effective_primary) {
            state.controlled_ent_vid = *vid;
        }
    }
}

bool RespawnLocalPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out) {
    if (state.net_session.role == NetRole::Offline) {
        if (status_out != nullptr) {
            *status_out = "No network session is active.";
        }
        return false;
    }

    const std::optional<Vec2> entrance_pos = FindStageEntranceSpawnPos(state);
    if (!entrance_pos.has_value()) {
        if (status_out != nullptr) {
            *status_out = "Network respawn failed: no entrance was found.";
        }
        return false;
    }

    std::vector<VID> changed_ents;
    int respawn_index = 0;
    for (PlayerSlot& slot : state.players.slots) {
        if (!ShouldApplyNetworkLifecycleToSlot(state, slot)) {
            continue;
        }

        const Vec2 spawn_pos =
            *entrance_pos + Vec2::New(static_cast<float>(respawn_index) * 8.0F, 0.0F);
        ++respawn_index;

        Ent* ent = nullptr;
        if (slot.ent_vid.has_value()) {
            ent = state.ents.GetEntMut(*slot.ent_vid);
        }
        if (ent == nullptr) {
            EnsureSpawnedPlayer(
                state,
                slot.player_id,
                true,
                slot.primary_local,
                spawn_pos,
                graphics
            );
            continue;
        }

        for (const VID changed_vid :
             ents::common::SeverEntCarryLinksForReset(*ent, state)) {
            if (std::find(changed_ents.begin(), changed_ents.end(), changed_vid) ==
                changed_ents.end()) {
                changed_ents.push_back(changed_vid);
            }
        }
        const EntType respawn_type =
            IsPlayerLikeEntType(ent->type_) ? ent->type_ : EntType::Player;
        SetEntAs(*ent, respawn_type);
        ent->pos = spawn_pos;
        ent->vel = Vec2::New(0.0F, 0.0F);
        ent->acc = Vec2::New(0.0F, 0.0F);
        ent->grounded = false;
        ent->coyote_time = 0;
        ent->fall_timer = 0;
        ent->stun_timer = 0;
        ent->render_enabled = GetEntSpec(ent->type_).render_enabled;
        state.UpdateSidForEnt(ent->vid.id, graphics);
        if (std::find(changed_ents.begin(), changed_ents.end(), ent->vid) ==
            changed_ents.end()) {
            changed_ents.push_back(ent->vid);
        }
        if (slot.primary_local) {
            state.controlled_ent_vid = ent->vid;
        }
    }

    (void)ResetStageEntrancePres(state);

    state.game_over = false;
    state.pending_stage_transition.reset();
    state.gameplay_camera_anchor_world_pos.reset();
    if (status_out != nullptr) {
        *status_out = "Respawned local network players at entrance.";
    }
    return true;
}

bool RespawnDeadNetworkPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out) {
    if (state.net_session.role == NetRole::Offline) {
        if (status_out != nullptr) {
            *status_out = "No network session is active.";
        }
        return false;
    }

    const std::optional<Vec2> entrance_pos = FindStageEntranceSpawnPos(state);
    if (!entrance_pos.has_value()) {
        if (status_out != nullptr) {
            *status_out = "Network respawn failed: no entrance was found.";
        }
        return false;
    }

    std::vector<VID> changed_ents;
    int respawn_index = 0;
    bool respawned_any = false;
    for (PlayerSlot& slot : state.players.slots) {
        if (!ShouldApplyNetworkLifecycleToSlot(state, slot)) {
            continue;
        }

        const Vec2 spawn_pos =
            *entrance_pos + Vec2::New(static_cast<float>(respawn_index) * 8.0F, 0.0F);
        ++respawn_index;

        if (!IsPlayerEntDeadOrMissing(state, slot)) {
            continue;
        }

        Ent* ent = nullptr;
        if (slot.ent_vid.has_value()) {
            ent = state.ents.GetEntMut(*slot.ent_vid);
        }
        if (ent == nullptr) {
            EnsureSpawnedPlayer(
                state,
                slot.player_id,
                slot.connection_kind == PlayerConnectionKind::Local,
                slot.primary_local,
                spawn_pos,
                graphics
            );
            ent = slot.ent_vid.has_value()
                ? state.ents.GetEntMut(*slot.ent_vid)
                : nullptr;
        }
        if (ent == nullptr) {
            continue;
        }

        for (const VID changed_vid :
             ents::common::SeverEntCarryLinksForReset(*ent, state)) {
            if (std::find(changed_ents.begin(), changed_ents.end(), changed_vid) ==
                changed_ents.end()) {
                changed_ents.push_back(changed_vid);
            }
        }
        const EntType respawn_type =
            IsPlayerLikeEntType(ent->type_) ? ent->type_ : EntType::Player;
        SetEntAs(*ent, respawn_type);
        ent->pos = spawn_pos;
        ent->vel = Vec2::New(0.0F, 0.0F);
        ent->acc = Vec2::New(0.0F, 0.0F);
        ent->grounded = false;
        ent->coyote_time = 0;
        ent->fall_timer = 0;
        ent->stun_timer = 0;
        ent->condition = EntCondition::Normal;
        ent->render_enabled = GetEntSpec(ent->type_).render_enabled;
        state.net_session.LinkEnt(MakePlayerNetEntId(slot.player_id), ent->vid);
        state.UpdateSidForEnt(ent->vid.id, graphics);
        if (slot.primary_local) {
            state.controlled_ent_vid = ent->vid;
        }
        respawned_any = true;
    }

    if (!respawned_any) {
        if (status_out != nullptr) {
            *status_out = "No dead network players needed respawn.";
        }
        return false;
    }

    for (const VID changed_vid : ResetStageEntrancePres(state)) {
        if (std::find(changed_ents.begin(), changed_ents.end(), changed_vid) ==
            changed_ents.end()) {
            changed_ents.push_back(changed_vid);
        }
    }

    state.game_over = false;
    state.gameplay_camera_anchor_world_pos.reset();
    if (status_out != nullptr) {
        *status_out = "Respawned dead network players at entrance.";
    }
    return true;
}

bool ReviveNetworkPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out) {
    if (state.net_session.role == NetRole::Offline) {
        if (status_out != nullptr) {
            *status_out = "No network session is active.";
        }
        return false;
    }

    const std::optional<Vec2> entrance_pos = FindStageEntranceSpawnPos(state);
    if (!entrance_pos.has_value()) {
        if (status_out != nullptr) {
            *status_out = "Network revive failed: no entrance was found.";
        }
        return false;
    }

    std::vector<VID> orphan_player_ents;
    for (const Ent& ent : state.ents.ents) {
        if (ent.active &&
            IsPlayerLikeEntType(ent.type_) &&
            !state.players.FindPlayerIdForEnt(ent.vid).has_value()) {
            orphan_player_ents.push_back(ent.vid);
        }
    }
    for (const VID orphan_vid : orphan_player_ents) {
        state.ents.SetInactiveVid(orphan_vid);
    }

    std::vector<VID> changed_ents;
    int spawn_index = 0;
    for (PlayerSlot& slot : state.players.slots) {
        if (!ShouldApplyNetworkLifecycleToSlot(state, slot)) {
            continue;
        }

        const Vec2 spawn_pos =
            *entrance_pos + Vec2::New(static_cast<float>(spawn_index) * 8.0F, 0.0F);
        ++spawn_index;

        Ent* ent = nullptr;
        if (slot.ent_vid.has_value()) {
            ent = state.ents.GetEntMut(*slot.ent_vid);
        }

        const bool needs_fresh_spawn =
            ent == nullptr || !ent->active || ent->condition == EntCondition::Dead;
        if (needs_fresh_spawn) {
            if (ent != nullptr) {
                for (const VID changed_vid :
                     ents::common::SeverEntCarryLinksForReset(*ent, state)) {
                    if (std::find(changed_ents.begin(), changed_ents.end(), changed_vid) ==
                        changed_ents.end()) {
                        changed_ents.push_back(changed_vid);
                    }
                }
                const EntType respawn_type =
                    IsPlayerLikeEntType(ent->type_) ? ent->type_ : EntType::Player;
                SetEntAs(*ent, respawn_type);
                ent->pos = spawn_pos;
            } else {
                EnsureSpawnedPlayer(
                    state,
                    slot.player_id,
                    slot.connection_kind == PlayerConnectionKind::Local,
                    slot.primary_local,
                    spawn_pos,
                    graphics
                );
                ent = slot.ent_vid.has_value()
                    ? state.ents.GetEntMut(*slot.ent_vid)
                    : nullptr;
            }
        }

        if (ent == nullptr) {
            continue;
        }

        ent->pos = spawn_pos;
        ent->vel = Vec2::New(0.0F, 0.0F);
        ent->acc = Vec2::New(0.0F, 0.0F);
        ent->grounded = false;
        ent->coyote_time = 0;
        ent->fall_timer = 0;
        ent->stun_timer = 0;
        ent->condition = EntCondition::Normal;
        ent->render_enabled = GetEntSpec(ent->type_).render_enabled;
        state.net_session.LinkEnt(MakePlayerNetEntId(slot.player_id), ent->vid);
        state.UpdateSidForEnt(ent->vid.id, graphics);
        if (slot.primary_local) {
            state.controlled_ent_vid = ent->vid;
        }
        if (std::find(changed_ents.begin(), changed_ents.end(), ent->vid) ==
            changed_ents.end()) {
            changed_ents.push_back(ent->vid);
        }
    }

    for (const VID changed_vid : ResetStageEntrancePres(state)) {
        if (std::find(changed_ents.begin(), changed_ents.end(), changed_vid) ==
            changed_ents.end()) {
            changed_ents.push_back(changed_vid);
        }
    }

    state.game_over = false;
    state.gameplay_camera_anchor_world_pos.reset();
    if (status_out != nullptr) {
        *status_out = "Revived network players at entrance.";
    }
    return true;
}

} // namespace splonks::network
