#include "network/net_lobby.hpp"

#include "entity/archetype.hpp"
#include "graphics.hpp"
#include "network/net_entity_links.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_progression.hpp"
#include "network/net_protocol.hpp"
#include "network/net_world_snapshot.hpp"
#include "entities/common/common.hpp"
#include "gameplay_messages.hpp"
#include "quest_stage_loader.hpp"
#include "stage_progression.hpp"
#include "stage_spawning.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace splonks::network {

namespace {

constexpr PlayerId kFirstRemotePlayerId = 2;
constexpr std::uint32_t kMaxPlayersPerEndpoint = 16;
constexpr std::uint32_t kJoinRetryFrames = 30;
constexpr std::uint64_t kRemoteEndpointTimeoutFrames = 180;
NetTransportRuntime& EnsureTransport(State& state) {
    if (!state.net_transport) {
        state.net_transport = std::make_unique<NetTransportRuntime>(NetTransportRuntime::New());
    }
    return *state.net_transport;
}

Vec2 GetPrimaryPlayerSpawnPos(const State& state) {
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal()) {
        if (primary->entity_vid.has_value()) {
            if (const Entity* const entity = state.entity_manager.GetEntity(*primary->entity_vid)) {
                return entity->pos;
            }
        }
    }
    return Vec2::New(24.0F, 24.0F);
}

Vec2 GetRemoteSpawnPos(const State& state) {
    return GetPrimaryPlayerSpawnPos(state) + Vec2::New(16.0F, 0.0F);
}

Vec2 GetEntranceOrRemoteSpawnPos(const State& state) {
    return FindStageEntranceSpawnPos(state).value_or(GetRemoteSpawnPos(state));
}

std::uint32_t MakeHostStageSeed(const State& state) {
    const std::uint32_t frame_component = state.frame == 0 ? 1U : state.frame;
    return frame_component ^ 0x51A7E5D3U;
}

bool StageCanBeNetworkSynced(const State& state) {
    return !state.stage.quest_id.empty() && !state.stage.quest_stage_id.empty();
}

bool LoadNetworkQuestStage(
    State& state,
    const std::string& quest_id,
    const std::string& quest_stage_id,
    std::uint32_t seed,
    bool preserve_player_state
) {
    return LoadQuestStage(
        state,
        quest_id,
        quest_stage_id,
        preserve_player_state,
        seed
    );
}

NetRetainedPlayerState* FindRetainedPlayerState(State& state, PlayerId player_id) {
    for (NetRetainedPlayerState& retained : state.net_session.retained_players) {
        if (retained.player_id == player_id) {
            return &retained;
        }
    }
    return nullptr;
}

const NetRetainedPlayerState* FindRetainedPlayerState(const State& state, PlayerId player_id) {
    for (const NetRetainedPlayerState& retained : state.net_session.retained_players) {
        if (retained.player_id == player_id) {
            return &retained;
        }
    }
    return nullptr;
}

void CopyEntityEffectsToRetained(
    const Entity& entity,
    std::uint8_t& effect_count,
    std::array<PlayerStatePatchedEffect, kPlayerStatePatchedEffectCount>& effects_out
) {
    effect_count = 0;
    if (const EntityEffects* const effects = entity.effects.get()) {
        effect_count = static_cast<std::uint8_t>(
            std::min<std::size_t>(effects->count, effects_out.size())
        );
        for (std::size_t i = 0; i < effect_count; ++i) {
            const EffectInstance& effect = effects->effects[i];
            effects_out[i] = PlayerStatePatchedEffect{
                .id = effect.id,
                .count = effect.count,
                .value = effect.value,
                .frames_remaining = effect.frames_remaining,
            };
        }
    }
}

void RestoreRetainedEffects(
    Entity& entity,
    std::uint8_t effect_count,
    const std::array<PlayerStatePatchedEffect, kPlayerStatePatchedEffectCount>& retained_effects
) {
    entity.effects.reset();
    const std::size_t count = std::min<std::size_t>(effect_count, retained_effects.size());
    if (count == 0) {
        return;
    }

    EntityEffects& effects = entity.effects.emplace();
    effects.count = static_cast<std::uint8_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        const PlayerStatePatchedEffect& retained_effect = retained_effects[i];
        effects.effects[i] = EffectInstance{
            .id = retained_effect.id,
            .count = retained_effect.count,
            .value = retained_effect.value,
            .frames_remaining = retained_effect.frames_remaining,
        };
    }
}

NetRetainedAttachedEntityState CaptureRetainedAttachedEntity(
    const State& state,
    std::optional<VID> attached_vid
) {
    NetRetainedAttachedEntityState retained;
    if (!attached_vid.has_value()) {
        return retained;
    }

    const Entity* const attached = state.entity_manager.GetEntity(*attached_vid);
    if (attached == nullptr || !attached->active || IsPlayerLikeEntityType(attached->type_)) {
        return retained;
    }

    retained.valid = true;
    retained.entity_type = attached->type_;
    retained.pos = attached->pos;
    retained.vel = attached->vel;
    retained.acc = attached->acc;
    retained.size = attached->size;
    retained.rotation = attached->rotation;
    retained.counter_a = attached->counter_a;
    retained.counter_b = attached->counter_b;
    retained.counter_c = attached->counter_c;
    retained.counter_d = attached->counter_d;
    retained.health = attached->health;
    retained.money = attached->money;
    retained.facing = static_cast<std::uint8_t>(attached->facing == LeftOrRight::Right ? 1 : 0);
    retained.condition = static_cast<std::uint8_t>(attached->condition);
    CopyEntityEffectsToRetained(*attached, retained.effect_count, retained.effects);
    return retained;
}

void RemoveRetainedPlayerState(State& state, PlayerId player_id) {
    state.net_session.retained_players.erase(
        std::remove_if(
            state.net_session.retained_players.begin(),
            state.net_session.retained_players.end(),
            [player_id](const NetRetainedPlayerState& retained) {
                return retained.player_id == player_id;
            }
        ),
        state.net_session.retained_players.end()
    );
}

void StoreRetainedPlayerState(State& state, const PlayerSlot& slot, const Entity& player) {
    RemoveRetainedPlayerState(state, slot.player_id);

    NetRetainedPlayerState retained;
    retained.player_id = slot.player_id;
    retained.display_name = slot.display_name;
    retained.quest_id = state.stage.quest_id;
    retained.quest_stage_id = state.stage.quest_stage_id;
    retained.entity_type = player.type_;
    retained.last_pos = player.pos;
    retained.health = player.health;
    retained.money = player.money;
    retained.disconnected_frame = state.frame;
    retained.held_item = CaptureRetainedAttachedEntity(state, player.holding_vid);
    retained.back_item = CaptureRetainedAttachedEntity(state, player.back_vid);

    if (const EntityToolState* const tools = state.entity_tools.FindEntityToolState(player.vid)) {
        for (std::size_t i = 0; i < retained.tool_slots.size() && i < tools->slots.size(); ++i) {
            const ToolSlot& tool_slot = tools->slots[i];
            retained.tool_slots[i] = PlayerStatePatchedToolSlot{
                .kind = tool_slot.kind,
                .count = tool_slot.count,
                .cooldown = tool_slot.cooldown,
                .active = static_cast<std::uint8_t>(tool_slot.active ? 1 : 0),
            };
        }
    }

    CopyEntityEffectsToRetained(player, retained.effect_count, retained.effects);

    state.net_session.retained_players.push_back(retained);
}

void CleanupExpiredRetainedPlayerStates(State& state) {
    const std::uint64_t lifetime = state.net_session.retained_player_lifetime_frames;
    if (lifetime == 0) {
        return;
    }

    state.net_session.retained_players.erase(
        std::remove_if(
            state.net_session.retained_players.begin(),
            state.net_session.retained_players.end(),
            [&](const NetRetainedPlayerState& retained) {
                return state.frame > retained.disconnected_frame &&
                       state.frame - retained.disconnected_frame > lifetime;
            }
        ),
        state.net_session.retained_players.end()
    );
}

void DeactivateRetainedAttachedEntity(
    State& state,
    const NetRetainedAttachedEntityState& retained,
    std::optional<VID> attached_vid
) {
    if (!retained.valid || !attached_vid.has_value()) {
        return;
    }
    if (const Entity* const attached = state.entity_manager.GetEntity(*attached_vid);
        attached != nullptr && attached->active && attached->type_ == retained.entity_type &&
        !IsPlayerLikeEntityType(attached->type_)) {
        (void)world_ops::DeactivateEntity(state, attached->vid);
    }
}

bool IsRetainedReconnectMode(NetReconnectSpawnMode mode) {
    return mode == NetReconnectSpawnMode::RetainedAtEntrance ||
           mode == NetReconnectSpawnMode::RetainedAtLastPosition ||
           mode == NetReconnectSpawnMode::RetainedAtHost;
}

void ApplyRetainedAttachedEntityState(
    State& state,
    Entity& holder,
    const NetRetainedAttachedEntityState& retained,
    AttachmentMode mode,
    const Graphics& graphics
);

Vec2 ResolveReconnectSpawnPos(
    const State& state,
    const NetRetainedPlayerState* retained,
    std::size_t player_index
) {
    Vec2 pos = GetRemoteSpawnPos(state) + Vec2::New(static_cast<float>(player_index) * 8.0F, 0.0F);
    switch (state.net_session.reconnect_spawn_mode) {
    case NetReconnectSpawnMode::FreshAtEntrance:
    case NetReconnectSpawnMode::RetainedAtEntrance:
        pos = GetEntranceOrRemoteSpawnPos(state) + Vec2::New(static_cast<float>(player_index) * 8.0F, 0.0F);
        break;
    case NetReconnectSpawnMode::FreshAtHost:
    case NetReconnectSpawnMode::RetainedAtHost:
        pos = GetPrimaryPlayerSpawnPos(state) + Vec2::New(16.0F + static_cast<float>(player_index) * 8.0F, 0.0F);
        break;
    case NetReconnectSpawnMode::RetainedAtLastPosition:
        if (retained != nullptr) {
            pos = retained->last_pos;
        }
        break;
    }
    return pos;
}

void ApplyRetainedPlayerState(
    State& state,
    PlayerId player_id,
    const NetRetainedPlayerState& retained,
    const Vec2& spawn_pos,
    const Graphics& graphics
) {
    EnsureSpawnedPlayer(state, player_id, false, false, spawn_pos, graphics);
    PlayerSlot* const slot = state.players.Find(player_id);
    if (slot == nullptr || !slot->entity_vid.has_value()) {
        return;
    }

    Entity* const player = state.entity_manager.GetEntityMut(*slot->entity_vid);
    if (player == nullptr || !player->active) {
        return;
    }

    SetEntityAs(*player, retained.entity_type);
    player->pos = spawn_pos;
    player->vel = Vec2::New(0.0F, 0.0F);
    player->acc = Vec2::New(0.0F, 0.0F);
    player->health = retained.health;
    player->money = retained.money;
    player->held_by_vid.reset();
    player->holding_vid.reset();
    player->back_vid.reset();
    player->attachment_mode = AttachmentMode::None;
    player->stun_timer = 0;
    player->fall_timer = 0;
    player->coyote_time = 0;
    player->hang_side.reset();
    player->hang_count = 0;

    for (std::size_t i = 0; i < retained.tool_slots.size(); ++i) {
        const PlayerStatePatchedToolSlot& retained_tool = retained.tool_slots[i];
        ToolSlot& tool_slot = state.entity_tools.EnsureToolSlot(player->vid, i);
        tool_slot.kind = retained_tool.kind;
        tool_slot.count = retained_tool.count;
        tool_slot.cooldown = retained_tool.cooldown;
        tool_slot.active = retained_tool.active != 0;
    }

    RestoreRetainedEffects(*player, retained.effect_count, retained.effects);

    state.UpdateSidForEntity(player->vid.id, graphics);
    ApplyRetainedAttachedEntityState(state, *player, retained.back_item, AttachmentMode::Back, graphics);
    ApplyRetainedAttachedEntityState(state, *player, retained.held_item, AttachmentMode::Held, graphics);
}

void ApplyRetainedAttachedEntityState(
    State& state,
    Entity& holder,
    const NetRetainedAttachedEntityState& retained,
    AttachmentMode mode,
    const Graphics& graphics
) {
    if (!retained.valid) {
        return;
    }

    Entity* const attached = world_ops::SpawnEntity(
        state,
        retained.entity_type,
        [&](Entity& entity) {
            entity.pos = retained.pos;
            entity.vel = retained.vel;
            entity.acc = retained.acc;
            entity.size = retained.size;
            entity.rotation = retained.rotation;
            entity.counter_a = retained.counter_a;
            entity.counter_b = retained.counter_b;
            entity.counter_c = retained.counter_c;
            entity.counter_d = retained.counter_d;
            entity.health = retained.health;
            entity.money = retained.money;
            entity.facing = retained.facing != 0 ? LeftOrRight::Right : LeftOrRight::Left;
            entity.condition = static_cast<EntityCondition>(retained.condition);
            RestoreRetainedEffects(entity, retained.effect_count, retained.effects);
        }
    );
    if (attached == nullptr) {
        return;
    }

    if (mode == AttachmentMode::Back) {
        holder.back_vid = attached->vid;
        attached->held_by_vid = holder.vid;
        attached->attachment_mode = AttachmentMode::Back;
        attached->has_physics = false;
        attached->can_collide = false;
    } else {
        entities::common::AttachEntityAsHeld(holder, *attached);
    }

    entities::common::SyncEntityAttachments(holder.vid.id, state, graphics);
    world_ops::MarkEntityHeld(state, holder, *attached, mode);
    world_ops::PatchEntityState(state, holder, holder);
    world_ops::PatchEntityState(state, holder, *attached);
}

bool EnsureHostSyncedStage(State& state, std::string* status_out) {
    if (!StageCanBeNetworkSynced(state)) {
        if (status_out != nullptr) {
            *status_out = "Host failed: current stage is not a quest stage.";
        }
        return false;
    }

    const std::string quest_id = state.stage.quest_id;
    const std::string quest_stage_id = state.stage.quest_stage_id;
    const std::uint32_t seed = state.stage.generation_seed.value_or(MakeHostStageSeed(state));
    if (!state.stage.generation_seed.has_value()) {
        if (!LoadNetworkQuestStage(state, quest_id, quest_stage_id, seed, false)) {
            if (status_out != nullptr) {
                *status_out = "Host failed: could not reload current quest stage with sync seed.";
            }
            return false;
        }
    }

    state.net_session.quest_id = quest_id;
    state.net_session.quest_stage_id = quest_stage_id;
    state.net_session.stage_seed = seed;
    return true;
}

void RegisterRemoteEndpoint(
    NetTransportRuntime& transport,
    const std::vector<PlayerId>& player_ids,
    const NetEndpoint& endpoint,
    std::uint64_t frame
) {
    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            remote.player_ids = player_ids;
            remote.last_heard_frame = frame;
            return;
        }
    }
    transport.remotes.push_back(NetRemoteEndpoint{
        .player_ids = player_ids,
        .endpoint = endpoint,
        .last_heard_frame = frame,
    });
}

void MarkRemoteEndpointHeard(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    std::uint64_t frame
) {
    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            remote.last_heard_frame = frame;
            return;
        }
    }
}

void RemoveRemotePlayers(
    State& state,
    NetTransportRuntime& transport,
    const std::vector<PlayerId>& player_ids
) {
    for (PlayerId player_id : player_ids) {
        if (player_id == kInvalidPlayerId || player_id == state.net_session.local_player_id) {
            continue;
        }
        if (PlayerSlot* const slot = state.players.Find(player_id)) {
            if (state.net_session.role == NetRole::Coordinator &&
                slot->connection_kind == PlayerConnectionKind::Remote) {
                if (slot->entity_vid.has_value()) {
                    if (Entity* const entity = state.entity_manager.GetEntityMut(*slot->entity_vid)) {
                        if (entity->active) {
                            const std::optional<VID> held_vid = entity->holding_vid;
                            const std::optional<VID> back_vid = entity->back_vid;
                            StoreRetainedPlayerState(state, *slot, *entity);
                            const NetRetainedPlayerState* const retained =
                                FindRetainedPlayerState(state, player_id);
                            const std::vector<VID> changed_entities =
                                entities::common::SeverEntityCarryLinksForReset(*entity, state);
                            for (const VID& changed_vid : changed_entities) {
                                if (const Entity* const changed =
                                        state.entity_manager.GetEntity(changed_vid)) {
                                    world_ops::PatchEntityState(state, *changed, *changed);
                                }
                            }
                            if (retained != nullptr) {
                                DeactivateRetainedAttachedEntity(state, retained->held_item, held_vid);
                                DeactivateRetainedAttachedEntity(state, retained->back_item, back_vid);
                            }
                            (void)world_ops::DeactivateEntity(state, entity->vid);
                        }
                    }
                }
                state.players.Remove(player_id);
                state.net_session.UnlinkEntity(MakePlayerNetEntityId(player_id));
            } else if (slot->entity_vid.has_value()) {
                state.entity_manager.SetInactiveVid(*slot->entity_vid);
            }
        }
        if (state.net_session.role == NetRole::Coordinator) {
            NetPeerState* peer_state = nullptr;
            for (NetPeerState& peer : state.net_session.peers) {
                if (peer.player_id == player_id) {
                    peer_state = &peer;
                    break;
                }
            }
            if (peer_state != nullptr) {
                peer_state->connected = false;
                peer_state->endpoint_address.clear();
                peer_state->endpoint_port = 0;
            }
        } else {
            state.players.Remove(player_id);
            state.net_session.UnlinkEntity(MakePlayerNetEntityId(player_id));
            state.net_session.peers.erase(
                std::remove_if(
                    state.net_session.peers.begin(),
                    state.net_session.peers.end(),
                    [player_id](const NetPeerState& peer) { return peer.player_id == player_id; }
                ),
                state.net_session.peers.end()
            );
        }
        transport.remote_player_targets.erase(
            std::remove_if(
                transport.remote_player_targets.begin(),
                transport.remote_player_targets.end(),
                [player_id](const NetRemotePlayerTarget& target) {
                    return target.player_id == player_id;
                }
            ),
            transport.remote_player_targets.end()
        );
    }

    for (NetRemoteEndpoint& remote : transport.remotes) {
        remote.player_ids.erase(
            std::remove_if(
                remote.player_ids.begin(),
                remote.player_ids.end(),
                [&](PlayerId remote_player_id) {
                    return std::find(player_ids.begin(), player_ids.end(), remote_player_id) !=
                           player_ids.end();
                }
            ),
            remote.player_ids.end()
        );
    }
    transport.remotes.erase(
        std::remove_if(
            transport.remotes.begin(),
            transport.remotes.end(),
            [](const NetRemoteEndpoint& remote) { return remote.player_ids.empty(); }
        ),
        transport.remotes.end()
    );
}

void RemoveRemoteEndpoint(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
) {
    std::vector<PlayerId> player_ids;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            player_ids = remote.player_ids;
            break;
        }
    }
    if (player_ids.empty()) {
        return;
    }
    RemoveRemotePlayers(state, transport, player_ids);
    transport.remotes.erase(
        std::remove_if(
            transport.remotes.begin(),
            transport.remotes.end(),
            [&](const NetRemoteEndpoint& remote) {
                return EndpointsEqual(remote.endpoint, endpoint);
            }
        ),
        transport.remotes.end()
    );
}

void CleanupTimedOutRemoteEndpoints(State& state, NetTransportRuntime& transport) {
    std::vector<NetEndpoint> timed_out;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (state.frame > remote.last_heard_frame &&
            state.frame - remote.last_heard_frame > kRemoteEndpointTimeoutFrames) {
            timed_out.push_back(remote.endpoint);
        }
    }
    for (const NetEndpoint& endpoint : timed_out) {
        RemoveRemoteEndpoint(state, transport, endpoint);
    }
}

std::uint32_t CountLocalPlayers(const PlayerRegistry& players) {
    std::uint32_t count = 0;
    for (const PlayerSlot& slot : players.slots) {
        if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local) {
            ++count;
        }
    }
    return std::max<std::uint32_t>(count, 1);
}

bool IsRemotePlayerIdAvailableForJoin(const State& state, PlayerId player_id) {
    if (player_id < kFirstRemotePlayerId || player_id == state.net_session.local_player_id) {
        return false;
    }

    if (const PlayerSlot* const slot = state.players.Find(player_id)) {
        return slot->connection_kind == PlayerConnectionKind::Remote && !slot->connected;
    }

    return std::none_of(
        state.net_session.peers.begin(),
        state.net_session.peers.end(),
        [player_id](const NetPeerState& peer) {
            return peer.player_id == player_id && peer.connected;
        }
    );
}

std::vector<PlayerId> GetPreferredJoinPlayerIds(
    State& state,
    const JoinRequestPacket& request,
    std::uint32_t player_count
) {
    std::vector<PlayerId> player_ids;
    player_ids.reserve(player_count);
    const std::uint32_t preferred_count = std::min<std::uint32_t>(
        request.preferred_player_count,
        static_cast<std::uint32_t>(request.preferred_player_ids.size())
    );
    for (std::uint32_t i = 0; i < preferred_count && player_ids.size() < player_count; ++i) {
        const PlayerId player_id = request.preferred_player_ids[i];
        if (std::find(player_ids.begin(), player_ids.end(), player_id) != player_ids.end()) {
            continue;
        }
        if (IsRemotePlayerIdAvailableForJoin(state, player_id)) {
            player_ids.push_back(player_id);
        }
    }

    while (player_ids.size() < player_count) {
        const PlayerId player_id = std::max(state.net_session.next_player_id++, kFirstRemotePlayerId);
        if (std::find(player_ids.begin(), player_ids.end(), player_id) != player_ids.end()) {
            continue;
        }
        if (IsRemotePlayerIdAvailableForJoin(state, player_id)) {
            player_ids.push_back(player_id);
        }
    }
    return player_ids;
}

void ClearLocalPlayersForJoin(State& state) {
    std::vector<PlayerId> local_player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connection_kind == PlayerConnectionKind::Local) {
            if (slot.entity_vid.has_value()) {
                state.entity_manager.SetInactiveVid(*slot.entity_vid);
            }
            local_player_ids.push_back(slot.player_id);
        }
    }
    for (PlayerId player_id : local_player_ids) {
        state.players.Remove(player_id);
    }
    state.debug_local_player_bots.clear();
    state.controlled_entity_vid.reset();
}

void SendJoinRequest(State& state) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }

    JoinRequestPacket request;
    request.local_player_count = CountLocalPlayers(state.players);
    if (state.net_transport) {
        request.preferred_player_count = static_cast<std::uint32_t>(std::min<std::size_t>(
            state.net_transport->preferred_player_ids.size(),
            request.preferred_player_ids.size()
        ));
        for (std::uint32_t i = 0; i < request.preferred_player_count; ++i) {
            request.preferred_player_ids[i] = state.net_transport->preferred_player_ids[i];
        }
    }
    WriteFixedString("Player", request.display_name);
    const EncodedNetPacket encoded = EncodeJoinRequest(request);
    SendEncodedPacket(*state.net_transport, state.net_transport->coordinator_endpoint, encoded);
}

LeaveNoticePacket MakeLocalLeaveNotice(const State& state) {
    LeaveNoticePacket notice;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local ||
            slot.player_id == kInvalidPlayerId) {
            continue;
        }
        if (notice.player_count >= notice.player_ids.size()) {
            break;
        }
        notice.player_ids[notice.player_count++] = slot.player_id;
    }
    return notice;
}

void SendLeaveNoticeToEndpoint(NetTransportRuntime& transport, const NetEndpoint& endpoint, const State& state) {
    const LeaveNoticePacket notice = MakeLocalLeaveNotice(state);
    if (notice.player_count == 0) {
        return;
    }
    SendEncodedPacket(transport, endpoint, EncodeLeaveNotice(notice));
}

void SendLeaveNotice(State& state) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }
    if (state.net_session.role == NetRole::Peer) {
        SendLeaveNoticeToEndpoint(*state.net_transport, state.net_transport->coordinator_endpoint, state);
        return;
    }
    if (state.net_session.role == NetRole::Coordinator) {
        for (const NetRemoteEndpoint& remote : state.net_transport->remotes) {
            SendLeaveNoticeToEndpoint(*state.net_transport, remote.endpoint, state);
        }
    }
}

std::vector<PlayerId> GetLeavePlayerIds(const LeaveNoticePacket& notice) {
    std::vector<PlayerId> player_ids;
    player_ids.reserve(notice.player_count);
    for (std::uint32_t i = 0; i < notice.player_count; ++i) {
        if (notice.player_ids[i] != kInvalidPlayerId) {
            player_ids.push_back(notice.player_ids[i]);
        }
    }
    return player_ids;
}

void HandleJoinRequest(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const UdpPacket& udp_packet,
    const JoinRequestPacket& request
) {
    std::uint32_t player_count = std::clamp(
        request.local_player_count,
        1U,
        kMaxPlayersPerEndpoint
    );
    std::vector<PlayerId> player_ids;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, udp_packet.endpoint)) {
            player_ids = remote.player_ids;
            player_count = std::max<std::uint32_t>(
                static_cast<std::uint32_t>(player_ids.size()),
                1
            );
            break;
        }
    }
    if (player_ids.empty()) {
        player_ids = GetPreferredJoinPlayerIds(state, request, player_count);
    }

    const std::string display_name = ReadFixedString(request.display_name);
    Vec2 remote_spawn = GetRemoteSpawnPos(state);
    for (std::size_t i = 0; i < player_ids.size(); ++i) {
        const PlayerId player_id = player_ids[i];
        const std::string player_name = display_name.empty()
            ? "Remote " + std::to_string(player_id)
            : display_name + " " + std::to_string(i + 1);
        PlayerSlot& slot = state.players.EnsureRemotePlayer(player_id, player_name);
        const NetRetainedPlayerState* const retained = FindRetainedPlayerState(state, player_id);
        const Vec2 spawn_pos = ResolveReconnectSpawnPos(state, retained, i);
        if (i == 0) {
            remote_spawn = spawn_pos;
        }
        bool resumed_existing_body = false;
        if (slot.entity_vid.has_value()) {
            if (const Entity* const entity = state.entity_manager.GetEntity(*slot.entity_vid);
                entity != nullptr && entity->active) {
                state.net_session.LinkEntity(MakePlayerNetEntityId(player_id), entity->vid);
                resumed_existing_body = true;
            }
        }
        if (!resumed_existing_body && retained != nullptr &&
            IsRetainedReconnectMode(state.net_session.reconnect_spawn_mode)) {
            ApplyRetainedPlayerState(state, player_id, *retained, spawn_pos, graphics);
            RemoveRetainedPlayerState(state, player_id);
        } else if (!resumed_existing_body) {
            EnsureSpawnedPlayer(
                state,
                player_id,
                false,
                false,
                spawn_pos,
                graphics
            );
            if (retained != nullptr) {
                RemoveRetainedPlayerState(state, player_id);
            }
        }

        NetPeerState* peer_state = nullptr;
        for (NetPeerState& peer : state.net_session.peers) {
            if (peer.player_id == player_id) {
                peer_state = &peer;
                break;
            }
        }
        if (peer_state == nullptr) {
            NetPeerState peer;
            peer.player_id = player_id;
            state.net_session.peers.push_back(peer);
            peer_state = &state.net_session.peers.back();
        }
        peer_state->display_name = player_name;
        peer_state->endpoint_address = udp_packet.endpoint.address;
        peer_state->endpoint_port = udp_packet.endpoint.port;
        peer_state->connected = true;
    }
    RegisterRemoteEndpoint(transport, player_ids, udp_packet.endpoint, state.frame);

    const Vec2 host_spawn = GetPrimaryPlayerSpawnPos(state);
    JoinAcceptPacket accept;
    accept.assigned_player_count = static_cast<std::uint32_t>(std::min<std::size_t>(
        player_ids.size(),
        accept.assigned_player_ids.size()
    ));
    for (std::uint32_t i = 0; i < accept.assigned_player_count; ++i) {
        accept.assigned_player_ids[i] = player_ids[i];
    }
    accept.coordinator_player_id = state.net_session.coordinator_player_id;
    accept.stage_instance_id = state.net_session.stage_instance_id;
    accept.remote_spawn_x = remote_spawn.x;
    accept.remote_spawn_y = remote_spawn.y;
    accept.host_spawn_x = host_spawn.x;
    accept.host_spawn_y = host_spawn.y;
    accept.stage_seed = state.net_session.stage_seed;
    accept.snapshot_start_coordinator_order = state.net_session.next_coordinator_order;
    WriteFixedString(state.net_session.quest_id, accept.quest_id);
    WriteFixedString(state.net_session.quest_stage_id, accept.quest_stage_id);
    WriteFixedString("Host", accept.coordinator_name);
    const EncodedNetPacket encoded = EncodeJoinAccept(accept);
    SendEncodedPacket(transport, udp_packet.endpoint, encoded);
    EnqueueWorldSnapshotEvents(state);
}

void HandleJoinAccept(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const JoinAcceptPacket& accept
) {
    const std::uint32_t assigned_count = std::clamp(
        accept.assigned_player_count,
        1U,
        static_cast<std::uint32_t>(accept.assigned_player_ids.size())
    );
    if (accept.assigned_player_ids[0] == kInvalidPlayerId) {
        return;
    }

    state.net_session.role = NetRole::Peer;
    state.net_session.local_player_id = accept.assigned_player_ids[0];
    state.net_session.coordinator_player_id = accept.coordinator_player_id;
    state.net_session.stage_instance_id = accept.stage_instance_id;
    state.net_session.quest_id = ReadFixedString(accept.quest_id);
    state.net_session.quest_stage_id = ReadFixedString(accept.quest_stage_id);
    state.net_session.stage_seed = accept.stage_seed;
    transport.join_request_pending = false;
    transport.preferred_player_ids.clear();
    for (std::uint32_t i = 0; i < assigned_count; ++i) {
        if (accept.assigned_player_ids[i] != kInvalidPlayerId) {
            transport.preferred_player_ids.push_back(accept.assigned_player_ids[i]);
        }
    }

    state.players = PlayerRegistry::New();
    state.controlled_entity_vid.reset();
    transport.remote_player_targets.clear();
    transport.replicated_entity_state_cache.clear();
    transport.replicated_fluid_cell_cache.clear();
    state.net_session.ClearStageEntityLinks();

    if (!LoadNetworkQuestStage(
            state,
            state.net_session.quest_id,
            state.net_session.quest_stage_id,
            state.net_session.stage_seed,
            false
        )) {
        transport.last_error = "Join accepted, but synced quest stage load failed.";
        return;
    }
    state.net_session.next_expected_coordinator_order = std::max<std::uint64_t>(
        accept.snapshot_start_coordinator_order,
        1
    );

    ClearLocalPlayersForJoin(state);
    EnsureSpawnedPlayer(
        state,
        accept.assigned_player_ids[0],
        true,
        true,
        Vec2::New(accept.remote_spawn_x, accept.remote_spawn_y),
        graphics
    );
    for (std::uint32_t i = 1; i < assigned_count; ++i) {
        const PlayerId player_id = accept.assigned_player_ids[i];
        if (player_id == kInvalidPlayerId) {
            continue;
        }
        state.players.EnsureLocalPlayer(player_id, "Player " + std::to_string(player_id), false);
        EnsureSpawnedPlayer(
            state,
            player_id,
            true,
            false,
            Vec2::New(accept.remote_spawn_x + static_cast<float>(i) * 8.0F, accept.remote_spawn_y),
            graphics
        );
    }

    state.players.EnsureRemotePlayer(accept.coordinator_player_id, ReadFixedString(accept.coordinator_name));
    NetPeerState* peer_state = nullptr;
    for (NetPeerState& peer : state.net_session.peers) {
        if (peer.player_id == accept.coordinator_player_id) {
            peer_state = &peer;
            break;
        }
    }
    if (peer_state == nullptr) {
        NetPeerState peer;
        peer.player_id = accept.coordinator_player_id;
        state.net_session.peers.push_back(peer);
        peer_state = &state.net_session.peers.back();
    }
    peer_state->display_name = ReadFixedString(accept.coordinator_name);
    peer_state->endpoint_address = transport.coordinator_endpoint.address;
    peer_state->endpoint_port = transport.coordinator_endpoint.port;
    EnsureSpawnedPlayer(
        state,
        accept.coordinator_player_id,
        false,
        false,
        Vec2::New(accept.host_spawn_x, accept.host_spawn_y),
        graphics
    );
    RegisterStageEntityLinks(state);
}

void StepHostPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    for (int i = 0; i < 64; ++i) {
        std::string error;
        const std::optional<UdpPacket> packet = transport.socket.Receive(&error);
        if (!error.empty()) {
            transport.last_error = error;
        }
        if (!packet.has_value()) {
            CleanupTimedOutRemoteEndpoints(state, transport);
            return;
        }

        MarkRemoteEndpointHeard(transport, packet->endpoint, state.frame);

        if (const std::optional<JoinRequestPacket> request =
                TryDecodeJoinRequest(packet->bytes.data(), packet->size)) {
            HandleJoinRequest(state, graphics, transport, *packet, *request);
            continue;
        }

        if (const std::optional<LeaveNoticePacket> leave =
                TryDecodeLeaveNotice(packet->bytes.data(), packet->size)) {
            RemoveRemotePlayers(state, transport, GetLeavePlayerIds(*leave));
            continue;
        }

        if (const std::optional<DurableEventAckPacket> ack =
                TryDecodeDurableEventAck(packet->bytes.data(), packet->size)) {
            HandleDurableEventAckAsCoordinator(state, transport, packet->endpoint, *ack);
            continue;
        }

        if (const std::optional<PlayerSnapshotsPacket> snapshots =
                TryDecodePlayerSnapshots(packet->bytes.data(), packet->size)) {
            ApplyPlayerSnapshots(state, graphics, transport, *snapshots);
            RelaySnapshotsToOtherRemotes(transport, packet->endpoint, *snapshots);
            continue;
        }

        if (const std::optional<ActionRequestEventsPacket> action_requests =
                TryDecodeActionRequestEvents(packet->bytes.data(), packet->size)) {
            HandleActionRequestEventsAsCoordinator(state, transport, packet->endpoint, *action_requests);
            continue;
        }

        if (const std::optional<PresentationCommandEventsPacket> presentation_events =
                TryDecodePresentationCommandEvents(packet->bytes.data(), packet->size)) {
            HandlePresentationCommandEventsAsCoordinator(state, *presentation_events);
            continue;
        }
    }
}

void StepPeerPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    if (transport.join_request_pending) {
        if (transport.join_request_retry_frames == 0) {
            SendJoinRequest(state);
            transport.join_request_retry_frames = kJoinRetryFrames;
        } else {
            transport.join_request_retry_frames -= 1;
        }
    }

    for (int i = 0; i < 64; ++i) {
        std::string error;
        const std::optional<UdpPacket> packet = transport.socket.Receive(&error);
        if (!error.empty()) {
            transport.last_error = error;
        }
        if (!packet.has_value()) {
            return;
        }

        if (const std::optional<LeaveNoticePacket> leave =
                TryDecodeLeaveNotice(packet->bytes.data(), packet->size)) {
            RemoveRemotePlayers(state, transport, GetLeavePlayerIds(*leave));
            continue;
        }

        if (const std::optional<JoinAcceptPacket> accept =
                TryDecodeJoinAccept(packet->bytes.data(), packet->size)) {
            HandleJoinAccept(state, graphics, transport, *accept);
            continue;
        }

        if (const std::optional<StageSyncPacket> stage_sync =
                TryDecodeStageSync(packet->bytes.data(), packet->size)) {
            ApplyStageSync(state, graphics, transport, *stage_sync);
            continue;
        }

        if (const std::optional<PlayerSnapshotsPacket> snapshots =
                TryDecodePlayerSnapshots(packet->bytes.data(), packet->size)) {
            ApplyPlayerSnapshots(state, graphics, transport, *snapshots);
            continue;
        }

        if (const std::optional<TileEventsPacket> tile_events =
                TryDecodeTileEvents(packet->bytes.data(), packet->size)) {
            HandleTileEventsAsPeer(state, *tile_events);
            continue;
        }

        if (const std::optional<FluidCellEventsPacket> fluid_events =
                TryDecodeFluidCellEvents(packet->bytes.data(), packet->size)) {
            HandleFluidCellEventsAsPeer(state, *fluid_events);
            continue;
        }

        if (const std::optional<EntitySpawnedEventsPacket> entity_events =
                TryDecodeEntitySpawnedEvents(packet->bytes.data(), packet->size)) {
            HandleEntitySpawnedEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityDamageEventsPacket> entity_events =
                TryDecodeEntityDamageEvents(packet->bytes.data(), packet->size)) {
            HandleEntityDamageEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityStateEventsPacket> entity_events =
                TryDecodeEntityStateEvents(packet->bytes.data(), packet->size)) {
            HandleEntityStateEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityCarryEventsPacket> entity_events =
                TryDecodeEntityCarryEvents(packet->bytes.data(), packet->size)) {
            HandleEntityCarryEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityLifecycleEventsPacket> entity_events =
                TryDecodeEntityLifecycleEvents(packet->bytes.data(), packet->size)) {
            HandleEntityLifecycleEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<PlayerStateEventsPacket> player_state_events =
                TryDecodePlayerStateEvents(packet->bytes.data(), packet->size)) {
            HandlePlayerStateEventsAsPeer(state, *player_state_events);
            continue;
        }

        if (const std::optional<RunStateEventsPacket> run_state_events =
                TryDecodeRunStateEvents(packet->bytes.data(), packet->size)) {
            HandleRunStateEventsAsPeer(state, *run_state_events);
            continue;
        }

        if (const std::optional<PresentationCommandEventsPacket> presentation_events =
                TryDecodePresentationCommandEvents(packet->bytes.data(), packet->size)) {
            HandlePresentationCommandEventsAsPeer(state, *presentation_events);
            continue;
        }

        if (const std::optional<ActionRequestAckPacket> action_ack =
                TryDecodeActionRequestAck(packet->bytes.data(), packet->size)) {
            HandleActionRequestAckAsPeer(state, *action_ack);
            continue;
        }
    }
}

} // namespace

void HandleJoinRequestAsCoordinator(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const UdpPacket& udp_packet,
    const JoinRequestPacket& request
) {
    HandleJoinRequest(state, graphics, transport, udp_packet, request);
}

void HandleJoinAcceptAsPeer(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const JoinAcceptPacket& accept
) {
    HandleJoinAccept(state, graphics, transport, accept);
}

void HandleLeaveNoticeAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const LeaveNoticePacket& leave
) {
    RemoveRemotePlayers(state, transport, GetLeavePlayerIds(leave));
}

bool StartHostSession(State& state, std::uint16_t port, std::string* status_out) {
    NetTransportRuntime& transport = EnsureTransport(state);
    std::string error;
    if (!transport.socket.Open(port, &error)) {
        if (status_out != nullptr) {
            *status_out = "Host failed: " + error;
        }
        return false;
    }

    state.net_session = NetSessionState::NewOffline();
    state.net_session.role = NetRole::Coordinator;
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.coordinator_player_id = kPrimaryLocalPlayerId;
    state.net_session.next_player_id = kFirstRemotePlayerId;
    if (!EnsureHostSyncedStage(state, status_out)) {
        transport.socket.Close();
        state.net_session = NetSessionState::NewOffline();
        return false;
    }
    state.net_session.stage_instance_id = static_cast<StageInstanceId>(state.net_session.stage_seed);
    state.players.EnsurePrimaryLocalPlayer();
    RegisterStageEntityLinks(state);
    transport.remotes.clear();
    transport.remote_player_targets.clear();
    transport.replicated_entity_state_cache.clear();
    transport.replicated_fluid_cell_cache.clear();
    transport.preferred_player_ids.clear();
    transport.join_request_pending = false;
    if (status_out != nullptr) {
        *status_out = "Hosting UDP on port " + std::to_string(transport.socket.BoundPort()) + ".";
    }
    return true;
}

bool JoinHostSession(
    State& state,
    const std::string& host,
    std::uint16_t port,
    std::string* status_out
) {
    NetTransportRuntime& transport = EnsureTransport(state);
    std::string error;
    if (!transport.socket.Open(0, &error)) {
        if (status_out != nullptr) {
            *status_out = "Join failed: " + error;
        }
        return false;
    }

    state.net_session = NetSessionState::NewOffline();
    state.net_session.role = NetRole::Peer;
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.coordinator_player_id = kPrimaryLocalPlayerId;
    transport.coordinator_endpoint = NetEndpoint{.address = host, .port = port};
    transport.remotes.clear();
    transport.remote_player_targets.clear();
    transport.replicated_entity_state_cache.clear();
    transport.replicated_fluid_cell_cache.clear();
    transport.join_request_pending = true;
    transport.join_request_retry_frames = 0;
    SendJoinRequest(state);
    if (status_out != nullptr) {
        *status_out = "Joining " + EndpointToString(transport.coordinator_endpoint) + ".";
    }
    return true;
}

void DisconnectSession(State& state, std::string* status_out) {
    if (state.net_transport) {
        SendLeaveNotice(state);
        state.net_transport->socket.Close();
        state.net_transport->remotes.clear();
        state.net_transport->remote_player_targets.clear();
        state.net_transport->replicated_entity_state_cache.clear();
        state.net_transport->replicated_fluid_cell_cache.clear();
        state.net_transport->join_request_pending = false;
    }
    state.net_session = NetSessionState::NewOffline();
    if (status_out != nullptr) {
        *status_out = "Disconnected.";
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
        if (!slot.connected ||
            (state.net_session.role != NetRole::Coordinator &&
             slot.connection_kind != PlayerConnectionKind::Local)) {
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
        world_ops::PatchPlayerState(state, *entity);
    }

    for (const VID changed_vid : changed_entities) {
        if (const Entity* const changed_entity = state.entity_manager.GetEntity(changed_vid)) {
            if (changed_entity->active) {
                world_ops::PatchEntityState(state, *changed_entity, *changed_entity);
            }
        }
    }

    (void)ResetStageEntrancePresentation(state);

    state.game_over = false;
    state.pending_stage_transition.reset();
    state.gameplay_camera_anchor_world_pos.reset();
    world_ops::PatchRunState(state);
    if (status_out != nullptr) {
        *status_out = "Respawned local network players at entrance.";
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
        if (!slot.connected ||
            (state.net_session.role != NetRole::Coordinator &&
             slot.connection_kind != PlayerConnectionKind::Local)) {
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

    for (const VID changed_vid : changed_entities) {
        if (const Entity* const changed_entity = state.entity_manager.GetEntity(changed_vid)) {
            if (changed_entity->active) {
                world_ops::PatchEntityState(state, *changed_entity, *changed_entity);
            }
        }
    }

    state.game_over = false;
    state.gameplay_camera_anchor_world_pos.reset();
    world_ops::PatchRunState(state);
    if (status_out != nullptr) {
        *status_out = "Revived network players at entrance.";
    }
    return true;
}

bool ReloadSyncedQuestStage(State& state, const Graphics& graphics, std::string* status_out) {
    if (state.net_session.role == NetRole::Offline) {
        if (status_out != nullptr) {
            *status_out = "No synced network stage is active.";
        }
        return false;
    }
    if (state.net_session.quest_id.empty() || state.net_session.quest_stage_id.empty()) {
        if (status_out != nullptr) {
            *status_out = "No synced quest stage metadata is available.";
        }
        return false;
    }
    struct SavedPlayerSlot {
        PlayerId player_id = kInvalidPlayerId;
        bool local = false;
        bool primary = false;
    };
    std::vector<SavedPlayerSlot> saved_slots;
    saved_slots.reserve(state.players.slots.size());
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.player_id == kInvalidPlayerId) {
            continue;
        }
        saved_slots.push_back(SavedPlayerSlot{
            .player_id = slot.player_id,
            .local = slot.connection_kind == PlayerConnectionKind::Local,
            .primary = slot.primary_local,
        });
    }

    state.players = PlayerRegistry::New();
    if (state.net_transport) {
        state.net_transport->remote_player_targets.clear();
        state.net_transport->replicated_entity_state_cache.clear();
        state.net_transport->replicated_fluid_cell_cache.clear();
    }
    const bool loaded = LoadNetworkQuestStage(
        state,
        state.net_session.quest_id,
        state.net_session.quest_stage_id,
        state.net_session.stage_seed,
        false
    );
    if (!loaded) {
        if (status_out != nullptr) {
            *status_out = "Synced stage reload failed.";
        }
        return false;
    }
    RegisterStageEntityLinks(state);
    const Vec2 spawn_base = GetPrimaryPlayerSpawnPos(state);
    state.players.slots.clear();
    state.controlled_entity_vid.reset();
    for (std::size_t i = 0; i < saved_slots.size(); ++i) {
        const SavedPlayerSlot& slot = saved_slots[i];
        EnsureSpawnedPlayer(
            state,
            slot.player_id,
            slot.local,
            slot.primary,
            spawn_base + Vec2::New(static_cast<float>(i) * 8.0F, 0.0F),
            graphics
        );
    }

    if (status_out != nullptr) {
        *status_out = "Reloaded synced stage " + state.net_session.quest_stage_id +
                      " seed " + std::to_string(state.net_session.stage_seed) + ".";
    }
    return true;
}

void StepNetworkLobby(State& state, const Graphics& graphics) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }
    if (state.net_session.role == NetRole::Coordinator) {
        CleanupExpiredRetainedPlayerStates(state);
        StepHostPackets(state, graphics, *state.net_transport);
        const bool should_send = ShouldSendSnapshots(state, *state.net_transport);
        if (should_send) {
            SendStageSyncToAllRemotes(state, *state.net_transport);
            SendSnapshotsToAllRemotes(state, *state.net_transport);
            SendReplicatedEntityStatePatchesToAllRemotes(state, *state.net_transport);
            SendReplicatedFluidCellPatchesToAllRemotes(state, *state.net_transport);
            SendCoordinatorEntityRepairPatchesToAllRemotes(state, *state.net_transport);
            SendOrderedEventsToAllRemotes(state, *state.net_transport);
        }
    } else if (state.net_session.role == NetRole::Peer) {
        StepPeerPackets(state, graphics, *state.net_transport);
        const bool should_send = ShouldSendSnapshots(state, *state.net_transport);
        if (!state.net_transport->join_request_pending && should_send) {
            SendSnapshotsToEndpoint(
                state,
                *state.net_transport,
                state.net_transport->coordinator_endpoint
            );
            SendPendingPeerEventsToCoordinator(state, *state.net_transport);
            SendDurableEventAckToCoordinator(state, *state.net_transport);
        }
    }
    StepRemotePlayerInterpolation(state, *state.net_transport, graphics);
    SyncNetworkAttachmentsAfterRemoteMovement(state, graphics);
}

bool IsTransportOpen(const State& state) {
    return state.net_transport && state.net_transport->socket.IsOpen();
}

std::uint16_t BoundTransportPort(const State& state) {
    if (!IsTransportOpen(state)) {
        return 0;
    }
    return state.net_transport->socket.BoundPort();
}

} // namespace splonks::network
