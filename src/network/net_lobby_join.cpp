#include "network/net_lobby_internal.hpp"

#include "graphics.hpp"
#include "network/net_entity_links.hpp"
#include "network/net_protocol.hpp"
#include "network/net_world_snapshot.hpp"
#include "quest_stage_loader.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace splonks::network {

namespace {

constexpr PlayerId kFirstRemotePlayerId = 2;
constexpr std::uint32_t kMaxPlayersPerEndpoint = 16;

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

} // namespace

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

void HandleJoinRequestAsCoordinator(
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
    accept.lockstep_start_frame = state.net_session.lockstep_next_frame_to_step;
    accept.lockstep_input_delay_frames = state.net_session.lockstep_input_delay_frames;
    WriteFixedString(state.net_session.quest_id, accept.quest_id);
    WriteFixedString(state.net_session.quest_stage_id, accept.quest_stage_id);
    WriteFixedString("Host", accept.coordinator_name);
    const EncodedNetPacket encoded = EncodeJoinAccept(accept);
    SendEncodedPacket(transport, udp_packet.endpoint, encoded);
    if (!state.net_session.input_lockstep_enabled) {
        EnqueueWorldSnapshotMessages(state);
    }
}

void HandleJoinAcceptAsPeer(
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
    state.net_session.lockstep_input_delay_frames = accept.lockstep_input_delay_frames;
    transport.join_request_pending = false;
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
    state.controlled_entity_vid.reset();
    transport.remote_player_targets.clear();
    transport.remote_entity_render_targets.clear();
    transport.replicated_entity_state_cache.clear();
    transport.replicated_fluid_cell_cache.clear();
    state.net_session.ClearStageEntityLinks();

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
    state.net_session.next_expected_coordinator_order = std::max<std::uint64_t>(
        accept.snapshot_start_coordinator_order,
        1
    );

    PlayerSlot& coordinator_slot =
        state.players.EnsureRemotePlayer(accept.coordinator_player_id, ReadFixedString(accept.coordinator_name));
    if (coordinator_slot.entity_vid.has_value()) {
        if (Entity* const coordinator = state.entity_manager.GetEntityMut(*coordinator_slot.entity_vid)) {
            coordinator->pos = Vec2::New(accept.host_spawn_x, accept.host_spawn_y);
            coordinator->vel = Vec2::New(0.0F, 0.0F);
            coordinator->acc = Vec2::New(0.0F, 0.0F);
            state.net_session.LinkEntity(MakePlayerNetEntityId(accept.coordinator_player_id), coordinator->vid);
        }
    } else {
        EnsureSpawnedPlayer(
            state,
            accept.coordinator_player_id,
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
    RegisterStageEntityLinks(state);
    state.frame = static_cast<std::uint32_t>(accept.lockstep_start_frame);
    state.stage_frame = static_cast<std::uint32_t>(accept.lockstep_start_frame);
    state.scene_frame = 0;
}

void HandleLeaveNoticeAsCoordinator(
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
