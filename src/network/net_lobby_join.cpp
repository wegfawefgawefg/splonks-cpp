#include "network/net_lobby_internal.hpp"

#include "content_compat.hpp"
#include "graphics.hpp"
#include "network/net_ent_links.hpp"
#include "network/net_protocol.hpp"
#include "quest_stage_loader.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace splonks::network {

namespace {

constexpr PlayerId kFirstRemotePlayerId = 2;
constexpr std::uint32_t kMaxPlayersPerEndpoint = 16;

std::uint64_t TryComputeGameplayContentHash(std::string* status_out) {
    try {
        if (status_out != nullptr) {
            status_out->clear();
        }
        return ComputeGameplayContentHash();
    } catch (const std::exception& err) {
        if (status_out != nullptr) {
            *status_out = err.what();
        }
        return 0;
    }
}

void RegisterRemoteEndpoint(
    NetTransportRuntime& transport,
    const std::vector<PlayerId>& player_ids,
    const NetEndpoint& endpoint,
    std::uint64_t pump_tick
) {
    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            remote.player_ids = player_ids;
            remote.last_heard_pump_tick = pump_tick;
            return;
        }
    }
    transport.remotes.push_back(NetRemoteEndpoint{
        .player_ids = player_ids,
        .endpoint = endpoint,
        .last_heard_frame = 0,
        .last_heard_pump_tick = pump_tick,
    });
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

} // namespace

void SendJoinRequest(State& state) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }
    if (state.net_transport->host_endpoint.address.empty() ||
        state.net_transport->host_endpoint.port == 0) {
        return;
    }

    JoinRequestPacket request;
    request.local_player_count = CountLocalPlayers(state.players);
    request.content_hash =
        TryComputeGameplayContentHash(&state.net_transport->last_error);
    if (request.content_hash == 0) {
        return;
    }
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
    SendEncodedPacket(*state.net_transport, state.net_transport->host_endpoint, encoded);
}

namespace {

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

bool ShouldDeferJoinRequest(const State& state) {
    return state.mode == Mode::StageTransition || state.pending_stage_transition.has_value();
}

void RemovePendingJoinEndpoint(NetTransportRuntime& transport, const NetEndpoint& endpoint) {
    transport.pending_join_endpoints.erase(
        std::remove_if(
            transport.pending_join_endpoints.begin(),
            transport.pending_join_endpoints.end(),
            [&endpoint](const NetPendingJoinEndpoint& pending) {
                return EndpointsEqual(pending.endpoint, endpoint);
            }
        ),
        transport.pending_join_endpoints.end()
    );
}

void SendJoinPendingPacket(
    const State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    JoinPendingReason reason
) {
    JoinPendingPacket pending;
    pending.stage_instance_id = state.net_session.stage_instance_id;
    pending.sender_peer_id = state.net_session.local_player_id;
    pending.reason = reason;
    pending.pending_join_count =
        static_cast<std::uint32_t>(transport.pending_join_endpoints.size());
    SendEncodedPacket(transport, endpoint, EncodeJoinPending(pending));
}

void QueuePendingJoinRequest(
    const State& state,
    NetTransportRuntime& transport,
    const UdpPacket& udp_packet,
    const JoinRequestPacket& request
) {
    for (NetPendingJoinEndpoint& pending : transport.pending_join_endpoints) {
        if (EndpointsEqual(pending.endpoint, udp_packet.endpoint)) {
            pending.request = request;
            pending.last_heard_pump_tick = transport.pump_tick;
            SendJoinPendingPacket(
                state,
                transport,
                udp_packet.endpoint,
                JoinPendingReason::StageTransition
            );
            return;
        }
    }

    transport.pending_join_endpoints.push_back(NetPendingJoinEndpoint{
        .endpoint = udp_packet.endpoint,
        .request = request,
        .last_heard_pump_tick = transport.pump_tick,
    });
    SendJoinPendingPacket(
        state,
        transport,
        udp_packet.endpoint,
        JoinPendingReason::StageTransition
    );
}

} // namespace

void SendLeaveNotice(State& state) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }
    if (state.net_session.role == NetRole::Peer) {
        SendLeaveNoticeToEndpoint(*state.net_transport, state.net_transport->host_endpoint, state);
        return;
    }
    if (state.net_session.role == NetRole::Host) {
        for (const NetRemoteEndpoint& remote : state.net_transport->remotes) {
            SendLeaveNoticeToEndpoint(*state.net_transport, remote.endpoint, state);
        }
    }
}

void DrainPendingJoinRequestsAsHost(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport
) {
    if (ShouldDeferJoinRequest(state) || transport.pending_join_endpoints.empty()) {
        return;
    }

    std::vector<NetPendingJoinEndpoint> pending = transport.pending_join_endpoints;
    transport.pending_join_endpoints.clear();
    for (const NetPendingJoinEndpoint& join : pending) {
        UdpPacket udp_packet;
        udp_packet.endpoint = join.endpoint;
        HandleJoinRequestAsHost(state, graphics, transport, udp_packet, join.request);
    }
}

void HandleJoinRequestAsHost(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const UdpPacket& udp_packet,
    const JoinRequestPacket& request
) {
    if (ShouldDeferJoinRequest(state)) {
        QueuePendingJoinRequest(state, transport, udp_packet, request);
        return;
    }
    RemovePendingJoinEndpoint(transport, udp_packet.endpoint);

    const std::uint64_t host_content_hash =
        TryComputeGameplayContentHash(&transport.last_error);
    if (host_content_hash == 0) {
        SendJoinPendingPacket(
            state,
            transport,
            udp_packet.endpoint,
            JoinPendingReason::ContentMismatch
        );
        return;
    }
    if (request.content_hash != host_content_hash) {
        transport.last_error = "Rejected join: gameplay content does not match host.";
        SendJoinPendingPacket(
            state,
            transport,
            udp_packet.endpoint,
            JoinPendingReason::ContentMismatch
        );
        return;
    }

    std::uint32_t player_count = std::clamp(
        request.local_player_count,
        1U,
        kMaxPlayersPerEndpoint
    );
    std::vector<PlayerId> player_ids;
    bool endpoint_already_registered = false;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, udp_packet.endpoint)) {
            player_ids = remote.player_ids;
            endpoint_already_registered = true;
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
        if (slot.ent_vid.has_value()) {
            if (const Ent* const ent = state.ents.GetEnt(*slot.ent_vid);
                ent != nullptr && ent->active) {
                state.net_session.LinkEnt(MakePlayerNetEntId(player_id), ent->vid);
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
    RegisterRemoteEndpoint(transport, player_ids, udp_packet.endpoint, transport.pump_tick);

    const Vec2 host_spawn = GetPrimaryPlayerSpawnPos(state);
    JoinAcceptPacket accept;
    accept.assigned_player_count = static_cast<std::uint32_t>(std::min<std::size_t>(
        player_ids.size(),
        accept.assigned_player_ids.size()
    ));
    for (std::uint32_t i = 0; i < accept.assigned_player_count; ++i) {
        accept.assigned_player_ids[i] = player_ids[i];
    }
    accept.host_player_id = state.net_session.host_player_id;
    accept.stage_instance_id = state.net_session.stage_instance_id;
    accept.remote_spawn_x = remote_spawn.x;
    accept.remote_spawn_y = remote_spawn.y;
    accept.host_spawn_x = host_spawn.x;
    accept.host_spawn_y = host_spawn.y;
    accept.stage_seed = state.net_session.stage_seed;
    accept.lockstep_start_frame = state.net_session.lockstep_next_frame_to_step;
    accept.lockstep_input_delay_frames = state.net_session.lockstep_input_delay_frames;
    accept.lockstep_max_rollback_frames = state.net_session.lockstep_max_rollback_frames;
    accept.content_hash = host_content_hash;
    accept.multiplayer_respawn_mode =
        static_cast<std::uint8_t>(state.multiplayer_respawn_mode);
    WriteFixedString(state.net_session.quest_id, accept.quest_id);
    WriteFixedString(state.net_session.quest_stage_id, accept.quest_stage_id);
    WriteFixedString("Host", accept.host_name);
    const EncodedNetPacket encoded = EncodeJoinAccept(accept);
    SendEncodedPacket(transport, udp_packet.endpoint, encoded);

    // A join accept only contains enough data to bootstrap the endpoint; the
    // authoritative player/entity topology comes from the join barrier snapshot.
    if (!player_ids.empty()) {
        if (endpoint_already_registered) {
            BeginJoinBarrierCatchup(state, player_ids.front());
        } else {
            BeginJoinBarrierTopologyChange(state, transport, player_ids);
        }
    }
}

void HandleJoinAcceptAsPeer(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const JoinAcceptPacket& accept
) {
    if (!transport.join_request_pending &&
        state.net_session.role == NetRole::Peer &&
        state.net_session.stage_instance_id == accept.stage_instance_id) {
        return;
    }

    const std::uint32_t assigned_count = std::clamp(
        accept.assigned_player_count,
        1U,
        static_cast<std::uint32_t>(accept.assigned_player_ids.size())
    );
    if (accept.assigned_player_ids[0] == kInvalidPlayerId) {
        return;
    }

    const std::uint64_t local_content_hash =
        TryComputeGameplayContentHash(&transport.last_error);
    if (local_content_hash == 0 || local_content_hash != accept.content_hash) {
        transport.last_error = "Join rejected: gameplay content does not match host.";
        transport.join_request_pending = false;
        transport.join_request_waiting_for_host = false;
        transport.join_pending_reason = JoinPendingReason::None;
        transport.join_request_retry_frames = 0;
        return;
    }

    state.net_session.role = NetRole::Peer;
    state.net_session.local_player_id = accept.assigned_player_ids[0];
    state.net_session.host_player_id = accept.host_player_id;
    state.net_session.stage_instance_id = accept.stage_instance_id;
    state.net_session.quest_id = ReadFixedString(accept.quest_id);
    state.net_session.quest_stage_id = ReadFixedString(accept.quest_stage_id);
    state.net_session.stage_seed = accept.stage_seed;
    state.net_session.lockstep_input_delay_frames = accept.lockstep_input_delay_frames;
    state.net_session.lockstep_max_rollback_frames = accept.lockstep_max_rollback_frames;
    switch (accept.multiplayer_respawn_mode) {
    case static_cast<std::uint8_t>(MultiplayerRespawnMode::GenerousNextLevel):
        state.multiplayer_respawn_mode = MultiplayerRespawnMode::GenerousNextLevel;
        break;
    case static_cast<std::uint8_t>(MultiplayerRespawnMode::NoRespawn):
        state.multiplayer_respawn_mode = MultiplayerRespawnMode::NoRespawn;
        break;
    case static_cast<std::uint8_t>(MultiplayerRespawnMode::RespawnAtEntrance):
        state.multiplayer_respawn_mode = MultiplayerRespawnMode::RespawnAtEntrance;
        break;
    default:
        state.multiplayer_respawn_mode = MultiplayerRespawnMode::GenerousNextLevel;
        break;
    }
    transport.join_request_pending = false;
    transport.join_request_waiting_for_host = false;
    transport.join_pending_reason = JoinPendingReason::None;
    ResetInputLockstepState(state);
    state.net_session.lockstep_next_frame_to_step = accept.lockstep_start_frame;
    state.net_session.lockstep_next_local_input_frame = accept.lockstep_start_frame;
    transport.preferred_player_ids.clear();
    for (std::uint32_t i = 0; i < assigned_count; ++i) {
        if (accept.assigned_player_ids[i] != kInvalidPlayerId) {
            transport.preferred_player_ids.push_back(accept.assigned_player_ids[i]);
        }
    }

    state.players = PlayerRegistry::New();
    if (state.net_session.input_lockstep_enabled) {
        state.ents = EntPool::New();
    }
    state.pending_stage_transition.reset();
    state.game_over = false;
    state.pause = false;
    state.controlled_ent_vid.reset();
    state.net_session.ClearStageEntLinks();

    if (!LoadQuestStage(
            state,
            state.net_session.quest_id,
            state.net_session.quest_stage_id,
            false,
            state.net_session.stage_seed
        )) {
        transport.last_error = "Join accepted, but synced quest stage load failed.";
        return;
    }
    PlayerSlot& host_slot =
        state.players.EnsureRemotePlayer(accept.host_player_id, ReadFixedString(accept.host_name));
    if (host_slot.ent_vid.has_value()) {
        if (Ent* const host = state.ents.GetEntMut(*host_slot.ent_vid)) {
            host->pos = Vec2::New(accept.host_spawn_x, accept.host_spawn_y);
            host->vel = Vec2::New(0.0F, 0.0F);
            host->acc = Vec2::New(0.0F, 0.0F);
            state.net_session.LinkEnt(MakePlayerNetEntId(accept.host_player_id), host->vid);
        }
    } else {
        EnsureSpawnedPlayer(
            state,
            accept.host_player_id,
            false,
            false,
            Vec2::New(accept.host_spawn_x, accept.host_spawn_y),
            graphics
        );
    }

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

    NetPeerState* peer_state = nullptr;
    for (NetPeerState& peer : state.net_session.peers) {
        if (peer.player_id == accept.host_player_id) {
            peer_state = &peer;
            break;
        }
    }
    if (peer_state == nullptr) {
        NetPeerState peer;
        peer.player_id = accept.host_player_id;
        state.net_session.peers.push_back(peer);
        peer_state = &state.net_session.peers.back();
    }
    peer_state->display_name = ReadFixedString(accept.host_name);
    peer_state->endpoint_address = transport.host_endpoint.address;
    peer_state->endpoint_port = transport.host_endpoint.port;
    RegisterStageEntLinks(state);
    state.frame = static_cast<std::uint32_t>(accept.lockstep_start_frame);
    state.stage_frame = static_cast<std::uint32_t>(accept.lockstep_start_frame);
    state.scene_frame = 0;
    state.net_session.join_barrier_active = true;
    state.net_session.join_barrier_phase = JoinBarrierPhase::WaitingForCatchup;
    state.net_session.join_barrier_active_peer_id = state.net_session.local_player_id;
}

void HandleJoinPendingAsPeer(
    State& state,
    NetTransportRuntime& transport,
    const JoinPendingPacket& pending
) {
    if (state.net_session.role != NetRole::Peer || !transport.join_request_pending) {
        return;
    }
    if (pending.reason == JoinPendingReason::ContentMismatch) {
        transport.last_error = "Join rejected: gameplay content does not match host.";
        transport.join_request_pending = false;
        transport.join_request_waiting_for_host = false;
        transport.join_pending_reason = JoinPendingReason::None;
        transport.join_request_retry_frames = 0;
        return;
    }
    transport.join_request_waiting_for_host = true;
    transport.join_pending_reason = pending.reason;
    transport.join_request_retry_frames = 180;
}

void HandleLeaveNoticeAsHost(
    State& state,
    NetTransportRuntime& transport,
    const LeaveNoticePacket& leave
) {
    std::vector<PlayerId> player_ids;
    player_ids.reserve(leave.player_count);
    for (std::uint32_t i = 0; i < leave.player_count; ++i) {
        if (leave.player_ids[i] != kInvalidPlayerId) {
            player_ids.push_back(leave.player_ids[i]);
        }
    }
    RemoveRemotePlayers(state, transport, player_ids);
}

} // namespace splonks::network
