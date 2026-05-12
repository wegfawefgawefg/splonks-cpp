#include "network/net_lobby_internal.hpp"

#include "entities/common/common.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace splonks::network {

namespace {

constexpr std::uint64_t kRemoteEndpointTimeoutFrames = 180;

} // namespace

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
                            (void)changed_entities;
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


} // namespace splonks::network
