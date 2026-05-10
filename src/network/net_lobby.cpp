#include "network/net_lobby.hpp"

#include "graphics.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_progression.hpp"
#include "state.hpp"

namespace splonks::network {

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
            SendOrderedMessagesToAllRemotes(state, *state.net_transport);
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
            SendPendingPeerMessagesToCoordinator(state, *state.net_transport);
            SendDurableMessageAckToCoordinator(state, *state.net_transport);
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
