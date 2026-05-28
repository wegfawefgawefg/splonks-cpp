#include "network/net_lobby_internal.hpp"

#include "ents/common/common.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace splonks::network {

namespace {

constexpr std::uint64_t kRemoteEndpointTimeoutPumpTicks = 180;
constexpr std::uint64_t kPendingJoinTimeoutPumpTicks = 360;

void DeactivateDepartingAttachedEnt(State& state, std::optional<VID> attached_vid) {
    if (!attached_vid.has_value()) {
        return;
    }
    const Ent* const attached = state.ents.GetEnt(*attached_vid);
    if (attached == nullptr || !attached->active || IsPlayerLikeEntType(attached->type_)) {
        return;
    }
    (void)world_ops::DeactivateEnt(state, attached->vid);
}

void DeactivateDepartingPlayerEnt(State& state, Ent& ent) {
    const std::optional<VID> held_vid = ent.holding_vid;
    const std::optional<VID> back_vid = ent.back_vid;
    (void)ents::common::SeverEntCarryLinksForReset(ent, state);
    DeactivateDepartingAttachedEnt(state, held_vid);
    DeactivateDepartingAttachedEnt(state, back_vid);
    (void)world_ops::DeactivateEnt(state, ent.vid);
}

} // namespace

void MarkRemoteEndpointHeard(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
) {
    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            remote.last_heard_pump_tick = transport.pump_tick;
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
            if (state.net_session.role == NetRole::Host &&
                slot->connection_kind == PlayerConnectionKind::Remote) {
                if (slot->ent_vid.has_value()) {
                    if (Ent* const ent = state.ents.GetEntMut(*slot->ent_vid)) {
                        if (ent->active) {
                            StoreRetainedPlayerState(state, *slot, *ent);
                            DeactivateDepartingPlayerEnt(state, *ent);
                        }
                    }
                }
                state.players.Remove(player_id);
                state.net_session.UnlinkEnt(MakePlayerNetEntId(player_id));
            } else if (slot->ent_vid.has_value()) {
                if (Ent* const ent = state.ents.GetEntMut(*slot->ent_vid)) {
                    if (ent->active) {
                        DeactivateDepartingPlayerEnt(state, *ent);
                    }
                }
            }
        }
        if (state.net_session.role == NetRole::Host) {
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
            state.net_session.UnlinkEnt(MakePlayerNetEntId(player_id));
            state.net_session.peers.erase(
                std::remove_if(
                    state.net_session.peers.begin(),
                    state.net_session.peers.end(),
                    [player_id](const NetPeerState& peer) { return peer.player_id == player_id; }
                ),
                state.net_session.peers.end()
            );
        }
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

    if (state.net_session.role == NetRole::Host) {
        BeginJoinBarrierTopologyRemoval(state, transport, player_ids);
    }
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

bool KickRemoteEndpoint(
    State& state,
    const std::string& address,
    std::uint16_t port,
    std::string* status_out
) {
    if (state.net_session.role != NetRole::Host || state.net_transport == nullptr) {
        if (status_out != nullptr) {
            *status_out = "Only the host can kick direct players.";
        }
        return false;
    }
    NetEndpoint endpoint;
    endpoint.address = address;
    endpoint.port = port;
    for (const NetRemoteEndpoint& remote : state.net_transport->remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            RemoveRemoteEndpoint(state, *state.net_transport, endpoint);
            if (status_out != nullptr) {
                *status_out = "Kicked direct player.";
            }
            return true;
        }
    }
    if (status_out != nullptr) {
        *status_out = "Direct player is no longer connected.";
    }
    return false;
}

bool KickRemotePlayer(State& state, PlayerId player_id, std::string* status_out) {
    if (state.net_session.role != NetRole::Host || state.net_transport == nullptr) {
        if (status_out != nullptr) {
            *status_out = "Only the host can kick direct players.";
        }
        return false;
    }
    if (player_id == kInvalidPlayerId || player_id == state.net_session.local_player_id) {
        if (status_out != nullptr) {
            *status_out = "Cannot kick that player.";
        }
        return false;
    }
    for (const NetRemoteEndpoint& remote : state.net_transport->remotes) {
        if (std::find(remote.player_ids.begin(), remote.player_ids.end(), player_id) !=
            remote.player_ids.end()) {
            RemoveRemoteEndpoint(state, *state.net_transport, remote.endpoint);
            if (status_out != nullptr) {
                *status_out = "Kicked direct player.";
            }
            return true;
        }
    }
    if (status_out != nullptr) {
        *status_out = "Direct player is no longer connected.";
    }
    return false;
}

void CleanupTimedOutRemoteEndpoints(State& state, NetTransportRuntime& transport) {
    transport.pending_join_endpoints.erase(
        std::remove_if(
            transport.pending_join_endpoints.begin(),
            transport.pending_join_endpoints.end(),
            [&](const NetPendingJoinEndpoint& pending) {
                return transport.pump_tick > pending.last_heard_pump_tick &&
                    transport.pump_tick - pending.last_heard_pump_tick >
                        kPendingJoinTimeoutPumpTicks;
            }
        ),
        transport.pending_join_endpoints.end()
    );

    std::vector<NetEndpoint> timed_out;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (transport.pump_tick > remote.last_heard_pump_tick &&
            transport.pump_tick - remote.last_heard_pump_tick > kRemoteEndpointTimeoutPumpTicks) {
            timed_out.push_back(remote.endpoint);
        }
    }
    for (const NetEndpoint& endpoint : timed_out) {
        RemoveRemoteEndpoint(state, transport, endpoint);
    }
}


} // namespace splonks::network
