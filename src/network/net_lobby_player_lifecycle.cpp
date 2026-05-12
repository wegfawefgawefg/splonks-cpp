#include "network/net_lobby.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "network/net_entity_links.hpp"
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
    return state.net_session.role == NetRole::Coordinator ||
           slot.connection_kind == PlayerConnectionKind::Local;
}

bool IsPlayerEntityDeadOrMissing(State& state, const PlayerSlot& slot) {
    if (!slot.entity_vid.has_value()) {
        return true;
    }
    const Entity* const entity = state.entity_manager.GetEntity(*slot.entity_vid);
    return entity == nullptr || !entity->active || entity->condition == EntityCondition::Dead;
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

    if (slot.entity_vid.has_value()) {
        if (Entity* const entity = state.entity_manager.GetEntityMut(*slot.entity_vid)) {
            if (entity->active) {
                entity->pos = pos;
                state.net_session.LinkEntity(MakePlayerNetEntityId(player_id), entity->vid);
                if (effective_local && effective_primary) {
                    state.controlled_entity_vid = entity->vid;
                }
                return;
            }
        }
        slot.entity_vid.reset();
    }

    const std::optional<VID> vid = SpawnPlayerForPlayerId(state, player_id, pos);
    if (vid.has_value()) {
        state.net_session.LinkEntity(MakePlayerNetEntityId(player_id), *vid);
        state.UpdateSidForEntity(vid->id, graphics);
        if (effective_local && effective_primary) {
            state.controlled_entity_vid = *vid;
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

    std::vector<VID> changed_entities;
    int respawn_index = 0;
    for (PlayerSlot& slot : state.players.slots) {
        if (!ShouldApplyNetworkLifecycleToSlot(state, slot)) {
            continue;
        }

        const Vec2 spawn_pos =
            *entrance_pos + Vec2::New(static_cast<float>(respawn_index) * 8.0F, 0.0F);
        ++respawn_index;

        Entity* entity = nullptr;
        if (slot.entity_vid.has_value()) {
            entity = state.entity_manager.GetEntityMut(*slot.entity_vid);
        }
        if (entity == nullptr) {
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
             entities::common::SeverEntityCarryLinksForReset(*entity, state)) {
            if (std::find(changed_entities.begin(), changed_entities.end(), changed_vid) ==
                changed_entities.end()) {
                changed_entities.push_back(changed_vid);
            }
        }
        const EntityType respawn_type =
            IsPlayerLikeEntityType(entity->type_) ? entity->type_ : EntityType::Player;
        SetEntityAs(*entity, respawn_type);
        entity->pos = spawn_pos;
        entity->vel = Vec2::New(0.0F, 0.0F);
        entity->acc = Vec2::New(0.0F, 0.0F);
        entity->grounded = false;
        entity->coyote_time = 0;
        entity->fall_timer = 0;
        entity->stun_timer = 0;
        entity->render_enabled = GetEntityArchetype(entity->type_).render_enabled;
        state.UpdateSidForEntity(entity->vid.id, graphics);
        if (std::find(changed_entities.begin(), changed_entities.end(), entity->vid) ==
            changed_entities.end()) {
            changed_entities.push_back(entity->vid);
        }
        if (slot.primary_local) {
            state.controlled_entity_vid = entity->vid;
        }
    }

    (void)ResetStageEntrancePresentation(state);

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

    std::vector<VID> changed_entities;
    int respawn_index = 0;
    bool respawned_any = false;
    for (PlayerSlot& slot : state.players.slots) {
        if (!ShouldApplyNetworkLifecycleToSlot(state, slot)) {
            continue;
        }

        const Vec2 spawn_pos =
            *entrance_pos + Vec2::New(static_cast<float>(respawn_index) * 8.0F, 0.0F);
        ++respawn_index;

        if (!IsPlayerEntityDeadOrMissing(state, slot)) {
            continue;
        }

        Entity* entity = nullptr;
        if (slot.entity_vid.has_value()) {
            entity = state.entity_manager.GetEntityMut(*slot.entity_vid);
        }
        if (entity == nullptr) {
            EnsureSpawnedPlayer(
                state,
                slot.player_id,
                slot.connection_kind == PlayerConnectionKind::Local,
                slot.primary_local,
                spawn_pos,
                graphics
            );
            entity = slot.entity_vid.has_value()
                ? state.entity_manager.GetEntityMut(*slot.entity_vid)
                : nullptr;
        }
        if (entity == nullptr) {
            continue;
        }

        for (const VID changed_vid :
             entities::common::SeverEntityCarryLinksForReset(*entity, state)) {
            if (std::find(changed_entities.begin(), changed_entities.end(), changed_vid) ==
                changed_entities.end()) {
                changed_entities.push_back(changed_vid);
            }
        }
        const EntityType respawn_type =
            IsPlayerLikeEntityType(entity->type_) ? entity->type_ : EntityType::Player;
        SetEntityAs(*entity, respawn_type);
        entity->pos = spawn_pos;
        entity->vel = Vec2::New(0.0F, 0.0F);
        entity->acc = Vec2::New(0.0F, 0.0F);
        entity->grounded = false;
        entity->coyote_time = 0;
        entity->fall_timer = 0;
        entity->stun_timer = 0;
        entity->condition = EntityCondition::Normal;
        entity->render_enabled = GetEntityArchetype(entity->type_).render_enabled;
        state.net_session.LinkEntity(MakePlayerNetEntityId(slot.player_id), entity->vid);
        state.UpdateSidForEntity(entity->vid.id, graphics);
        if (slot.primary_local) {
            state.controlled_entity_vid = entity->vid;
        }
        respawned_any = true;
    }

    if (!respawned_any) {
        if (status_out != nullptr) {
            *status_out = "No dead network players needed respawn.";
        }
        return false;
    }

    for (const VID changed_vid : ResetStageEntrancePresentation(state)) {
        if (std::find(changed_entities.begin(), changed_entities.end(), changed_vid) ==
            changed_entities.end()) {
            changed_entities.push_back(changed_vid);
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

    std::vector<VID> orphan_player_entities;
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active &&
            IsPlayerLikeEntityType(entity.type_) &&
            !state.players.FindPlayerIdForEntity(entity.vid).has_value()) {
            orphan_player_entities.push_back(entity.vid);
        }
    }
    for (const VID orphan_vid : orphan_player_entities) {
        state.entity_manager.SetInactiveVid(orphan_vid);
    }

    std::vector<VID> changed_entities;
    int spawn_index = 0;
    for (PlayerSlot& slot : state.players.slots) {
        if (!ShouldApplyNetworkLifecycleToSlot(state, slot)) {
            continue;
        }

        const Vec2 spawn_pos =
            *entrance_pos + Vec2::New(static_cast<float>(spawn_index) * 8.0F, 0.0F);
        ++spawn_index;

        Entity* entity = nullptr;
        if (slot.entity_vid.has_value()) {
            entity = state.entity_manager.GetEntityMut(*slot.entity_vid);
        }

        const bool needs_fresh_spawn =
            entity == nullptr || !entity->active || entity->condition == EntityCondition::Dead;
        if (needs_fresh_spawn) {
            if (entity != nullptr) {
                for (const VID changed_vid :
                     entities::common::SeverEntityCarryLinksForReset(*entity, state)) {
                    if (std::find(changed_entities.begin(), changed_entities.end(), changed_vid) ==
                        changed_entities.end()) {
                        changed_entities.push_back(changed_vid);
                    }
                }
                const EntityType respawn_type =
                    IsPlayerLikeEntityType(entity->type_) ? entity->type_ : EntityType::Player;
                SetEntityAs(*entity, respawn_type);
                entity->pos = spawn_pos;
            } else {
                EnsureSpawnedPlayer(
                    state,
                    slot.player_id,
                    slot.connection_kind == PlayerConnectionKind::Local,
                    slot.primary_local,
                    spawn_pos,
                    graphics
                );
                entity = slot.entity_vid.has_value()
                    ? state.entity_manager.GetEntityMut(*slot.entity_vid)
                    : nullptr;
            }
        }

        if (entity == nullptr) {
            continue;
        }

        entity->pos = spawn_pos;
        entity->vel = Vec2::New(0.0F, 0.0F);
        entity->acc = Vec2::New(0.0F, 0.0F);
        entity->grounded = false;
        entity->coyote_time = 0;
        entity->fall_timer = 0;
        entity->stun_timer = 0;
        entity->condition = EntityCondition::Normal;
        entity->render_enabled = GetEntityArchetype(entity->type_).render_enabled;
        state.net_session.LinkEntity(MakePlayerNetEntityId(slot.player_id), entity->vid);
        state.UpdateSidForEntity(entity->vid.id, graphics);
        if (slot.primary_local) {
            state.controlled_entity_vid = entity->vid;
        }
        if (std::find(changed_entities.begin(), changed_entities.end(), entity->vid) ==
            changed_entities.end()) {
            changed_entities.push_back(entity->vid);
        }
    }

    for (const VID changed_vid : ResetStageEntrancePresentation(state)) {
        if (std::find(changed_entities.begin(), changed_entities.end(), changed_vid) ==
            changed_entities.end()) {
            changed_entities.push_back(changed_vid);
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
